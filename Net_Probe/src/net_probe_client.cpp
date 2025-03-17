#include "../include/common.h"
#include "../include/net_probe.h"
#include "../include/http_client.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <getopt.h>
#include <sys/time.h>

// Statistics output functions
void statOutputResponse(double elapsed, int replies, double min, double max, double avg, double jitter) {
    std::lock_guard<std::mutex> lock(outputMutex);
    std::cout << "Elapsed " << std::fixed << std::setprecision(2) << elapsed << "s ";
    std::cout << "Replies " << replies << " ";
    std::cout << "Min " << min << "ms ";
    std::cout << "Max " << max << "ms ";
    std::cout << "Avg " << avg << "ms ";
    std::cout << "Jitter " << jitter << "ms  ";
    std::cout << "\r";
    std::cout.flush();
}

void statOutput(NetOptions::Mode mode, double stat, int pkts, int lossNum, 
                double lossRate, double rate, int jitter, int index) {
    double elapsed = stat * index; // stat in seconds
    std::string rateUnit = "KBps";
    
    if (rate > 1024.0) {
        rate /= 1024.0;
        rateUnit = "MBps";
    }
    
    std::lock_guard<std::mutex> lock(outputMutex);
    
    if (mode == NetOptions::RECV) {
        std::cout << "Elapsed " << std::fixed 
                  << std::setprecision(1) << elapsed << "s ";
        std::cout << "Pkts " << pkts << " Lost " << lossNum << ", " 
                  << lossRate << "% ";
        std::cout << "Rate " << rate << rateUnit << " ";
        std::cout << "Jitter " << jitter << "ms";
    } else if (mode == NetOptions::SEND) {
        std::cout << "Elapsed " << std::fixed 
                  << std::setprecision(1) << elapsed << "s ";
        std::cout << "Rate " << rate << rateUnit;
    }
    
    std::cout << "\r";
    std::cout.flush();
}

// Client initialization function
void initializeClient(const NetOptions& options) {
    // Handle HTTP/HTTPS mode separately
    if (options.mode == NetOptions::HTTP) {
        httpClientHandler(options);
        return;
    }
    
    // Initialize sockets
    if (!initializeSockets()) {
        std::cerr << "Failed to initialize sockets" << std::endl;
        return;
    }
    
    // Prepare message to send to server
    std::vector<int> message = generateMessage(options);
    
    // Create socket and connect to server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(options.rport);
    
    // Resolve hostname if needed
    std::string host = options.rhost;
    if (host == "localhost") {
        host = "127.0.0.1";
    }
    
    // Check if IP address or hostname
    if (!isValidIpAddress(host)) {
        struct hostent* server = gethostbyname(host.c_str());
        if (!server) {
            std::cerr << "Failed to resolve hostname: " << host << std::endl;
            shutdownSockets();
            return;
        }
        
        memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    } else {
        serverAddr.sin_addr.s_addr = inet_addr(host.c_str());
    }
    
    // Create control connection socket
    int controlSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (controlSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create control socket: " << strerror(errno) << std::endl;
        shutdownSockets();
        return;
    }
    
    // Connect to server
    if (connect(controlSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server: " << strerror(errno) << std::endl;
        closesocket(controlSocket);
        shutdownSockets();
        return;
    }
    
    // Send configuration message
    if (!sendData(controlSocket, message.data(), message.size() * sizeof(int))) {
        std::cerr << "Failed to send configuration message" << std::endl;
        closesocket(controlSocket);
        shutdownSockets();
        return;
    }
    
    // Receive data port from server
    int dataPort;
    int retries = 3;
    bool received = false;
    
    while (retries-- > 0 && !received) {
        ssize_t bytesRead = receiveData(controlSocket, &dataPort, sizeof(dataPort));
        if (bytesRead == sizeof(dataPort)) {
            received = true;
        } else {
            Sleep(100);
        }
    }
    
    if (!received) {
        std::cerr << "Failed to receive data port from server" << std::endl;
        closesocket(controlSocket);
        shutdownSockets();
        return;
    }
    
    // Close control connection
    closesocket(controlSocket);
    
    // Update server address with data port
    serverAddr.sin_port = htons(dataPort);
    
    // Handle TCP or UDP based on protocol
    if (options.proto == "TCP") {
        // Create data connection socket
        int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (dataSocket == INVALID_SOCKET) {
            std::cerr << "Failed to create data socket: " << strerror(errno) << std::endl;
            shutdownSockets();
            return;
        }
        
        // Connect to server
        if (connect(dataSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Failed to connect to server data port: " << strerror(errno) << std::endl;
            closesocket(dataSocket);
            shutdownSockets();
            return;
        }
        
        std::cout << "Connected to server on port " << dataPort << std::endl;
        
        // Set buffer sizes if specified
        if (options.sbufsize > 0) {
            setsockopt(dataSocket, SOL_SOCKET, SO_SNDBUF, &options.sbufsize, sizeof(options.sbufsize));
        }
        if (options.rbufsize > 0) {
            setsockopt(dataSocket, SOL_SOCKET, SO_RCVBUF, &options.rbufsize, sizeof(options.rbufsize));
        }
        
        // Handle based on mode
        if (options.mode == NetOptions::RESPONSE) {
            // Response mode
            int replies = 0;
            double repTimeSum = 0, min = 999999, max = 0, avg = 0;
            int numIntervals = 0;
            double meanJitter = 0, meanRecvInterval = 0;
            
            struct timeval sendTime, recvTime, lastRecvTime, startTime, endTime;
            gettimeofday(&startTime, nullptr);
            
            // Default to 10 packets per second if not specified
            int pktRate = (options.pktrate == 1000) ? 10 : options.pktrate;
            float sendDelay = (pktRate > 0) ? (1000000.0f / pktRate) : 0;
            
            char request[4] = "REQ";
            
            for (int i = 0; i < options.pktnum; i++) {
                // Send request
                if (!sendData(dataSocket, request, sizeof(request))) {
                    std::cerr << "Failed to send request" << std::endl;
                    break;
                }
                
                gettimeofday(&sendTime, nullptr);
                
                // Receive response
                char response[4];
                if (receiveData(dataSocket, response, sizeof(response)) <= 0) {
                    std::cerr << "Failed to receive response" << std::endl;
                    break;
                }
                
                gettimeofday(&recvTime, nullptr);
                replies++;
                
                // Calculate round-trip time in milliseconds
                double roundTripTime = (recvTime.tv_sec - sendTime.tv_sec) * 1000.0 + 
                                       (recvTime.tv_usec - sendTime.tv_usec) / 1000.0;
                
                repTimeSum += roundTripTime;
                min = std::min(min, roundTripTime);
                max = std::max(max, roundTripTime);
                avg = repTimeSum / replies;
                
                // Calculate jitter
                if (i > 0) {
                    double recvInterval = (recvTime.tv_sec - lastRecvTime.tv_sec) * 1000.0 + 
                                         (recvTime.tv_usec - lastRecvTime.tv_usec) / 1000.0;
                    
                    numIntervals++;
                    double jitterDelta = std::abs(recvInterval - meanRecvInterval);
                    meanJitter = (meanJitter * (numIntervals - 1) + jitterDelta) / numIntervals;
                    meanRecvInterval = (meanRecvInterval * (numIntervals - 1) + recvInterval) / numIntervals;
                }
                
                lastRecvTime = recvTime;
                
                // Output statistics periodically
                gettimeofday(&endTime, nullptr);
                double elapsed = (endTime.tv_sec - startTime.tv_sec) + 
                                 (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
                
                if (elapsed >= options.stat / 1000.0) {
                    statOutputResponse(elapsed, replies, min, max, avg, meanJitter);
                    startTime = endTime;
                }
                
                // Maintain packet rate if specified
                if (sendDelay > 0) {
                    gettimeofday(&endTime, nullptr);
                    double packetElapsed = (endTime.tv_sec - sendTime.tv_sec) * 1000000.0 + 
                                          (endTime.tv_usec - sendTime.tv_usec);
                    
                    if (packetElapsed < sendDelay) {
                        usleep(static_cast<useconds_t>(sendDelay - packetElapsed));
                    }
                }
                
                // Handle connection persistence
                if (options.isPresistent == "No" && i < options.pktnum - 1) {
                    closesocket(dataSocket);
                    
                    // Create new connection for next packet
                    dataSocket = socket(AF_INET, SOCK_STREAM, 0);
                    if (dataSocket == INVALID_SOCKET) {
                        std::cerr << "Failed to create new data socket: " << strerror(errno) << std::endl;
                        break;
                    }
                    
                    // Connect to next port
                    serverAddr.sin_port = htons(dataPort + i + 1);
                    if (connect(dataSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                        std::cerr << "Failed to connect to server on port " << dataPort + i + 1 << std::endl;
                        closesocket(dataSocket);
                        break;
                    }
                }
            }
        }
        else if (options.mode == NetOptions::SEND) {
            // Send mode
            int bufSize = options.pktsize / 4;
            std::vector<int> buffer(bufSize);
            
            // Calculate delay between packets based on rate
            float sendDelay = 0.0f;
            if (options.pktrate > 0) {
                sendDelay = 1000000.0f * options.pktsize / options.pktrate;
            }
            
            int statIndex = 1, packetsSent = 0;
            struct timeval startTime, sentTime;
            gettimeofday(&startTime, nullptr);
            
            for (int i = 0; i < options.pktnum; i++) {
                // Fill buffer with packet index
                std::fill(buffer.begin(), buffer.end(), i);
                
                if (options.pktrate > 0) {
                    gettimeofday(&startTime, nullptr);
                }
                
                // Send data
                if (!sendData(dataSocket, buffer.data(), buffer.size() * sizeof(int))) {
                    std::cerr << "Failed to send data" << std::endl;
                    break;
                }
                
                // Handle rate limiting
                if (options.pktrate > 0) {
                    gettimeofday(&sentTime, nullptr);
                    double elapsed = (sentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                     (sentTime.tv_usec - startTime.tv_usec);
                    
                    // Output statistics if necessary
                    double statTime = options.stat * 1000.0; // microseconds
                    while (elapsed < sendDelay) {
                        Sleep(10); // Sleep for 10ms
                        elapsed += 10000;
                        
                        if (elapsed >= statTime) {
                            statOutput(NetOptions::SEND, options.stat / 1000.0, 0, 0, 0, 
                                      options.pktrate, 0, statIndex++);
                            statTime += options.stat * 1000.0;
                        }
                    }
                }
                else { // Unlimited rate, measure throughput
                    gettimeofday(&sentTime, nullptr);
                    double elapsed = (sentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                     (sentTime.tv_usec - startTime.tv_usec);
                    
                    double statTime = options.stat / 1000.0 * 1000000.0; // microseconds
                    if (elapsed >= statTime) {
                        int packetsDelta = i - packetsSent;
                        double rate = (packetsDelta * options.pktsize) / (elapsed / 1000000.0);
                        
                        statOutput(NetOptions::SEND, options.stat / 1000.0, 0, 0, 0, 
                                  rate, 0, statIndex++);
                        
                        packetsSent = i;
                        gettimeofday(&startTime, nullptr);
                    }
                }
            }
        }
        else if (options.mode == NetOptions::RECV) {
            // Receive mode
            int bufSize = options.pktsize / 4;
            std::vector<int> buffer(bufSize);
            
            int pktIndex = 0, pktLoss = 0, pktRecv = 0, statIndex = 1, totalBytes = 0;
            int numIntervals = 0;
            double meanJitter = 0, meanRecvInterval = 0;
            
            struct timeval startTime, currentTime;
            struct timeval recvTime, lastRecvTime;
            gettimeofday(&startTime, nullptr);
            gettimeofday(&lastRecvTime, nullptr);
            
            for (int i = 0; i < options.pktnum; i++) {
                // Receive data
                ssize_t bytesRead = receiveData(dataSocket, buffer.data(), buffer.size() * sizeof(int));
                if (bytesRead <= 0) {
                    std::cerr << "Failed to receive data" << std::endl;
                    break;
                }
                
                pktRecv++;
                totalBytes += bytesRead;
                gettimeofday(&currentTime, nullptr);
                
                // Check for packet loss
                if (buffer[0] != pktIndex) {
                    pktLoss++;
                } else {
                    // Calculate jitter
                    gettimeofday(&recvTime, nullptr);
                    double recvInterval = (recvTime.tv_sec - lastRecvTime.tv_sec) * 1000.0 + 
                                         (recvTime.tv_usec - lastRecvTime.tv_usec) / 1000.0;
                    
                    lastRecvTime = recvTime;
                    numIntervals++;
                    
                    double jitterDelta = std::abs(recvInterval - meanRecvInterval);
                    meanJitter = (meanJitter * (numIntervals - 1) + jitterDelta) / numIntervals;
                    meanRecvInterval = (meanRecvInterval * (numIntervals - 1) + recvInterval) / numIntervals;
                    
                    // Convert to appropriate units
                    meanJitter /= 1000000.0;
                }
                
                pktIndex++;
                
                // Output statistics periodically
                double elapsed = (currentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                (currentTime.tv_usec - startTime.tv_usec);
                
                if (elapsed >= options.stat * 1000.0) {
                    double rate = pktRecv * buffer.size() * sizeof(int) / (elapsed / 1000000.0);
                    double lossRate = (pktLoss * 100.0) / pktIndex;
                    
                    statOutput(NetOptions::RECV, options.stat / 1000.0, pktIndex, pktLoss, 
                              lossRate, rate, meanJitter, statIndex++);
                    
                    pktRecv = 0;
                    gettimeofday(&startTime, nullptr);
                }
            }
        }
        
        // Clean up
        closesocket(dataSocket);
    }
    else if (options.proto == "UDP") {
        // Create UDP socket
        int dataSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (dataSocket == INVALID_SOCKET) {
            std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
            shutdownSockets();
            return;
        }
        
        // Handle based on mode
        if (options.mode == NetOptions::RESPONSE) {
            // Response mode
            int replies = 0;
            double repTimeSum = 0, min = 999999, max = 0, avg = 0;
            int numIntervals = 0;
            double meanJitter = 0, meanRecvInterval = 0;
            
            struct timeval sendTime, recvTime, lastRecvTime, startTime, endTime;
            gettimeofday(&startTime, nullptr);
            
            // Default to 10 packets per second if not specified
            int pktRate = (options.pktrate == 1000) ? 10 : options.pktrate;
            float sendDelay = (pktRate > 0) ? (1000000.0f / pktRate) : 0;
            
            char request[4] = "REQ";
            
            for (int i = 0; i < options.pktnum; i++) {
                // Send request
                if (sendto(dataSocket, request, sizeof(request), 0, 
                         (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                    std::cerr << "Failed to send UDP request" << std::endl;
                    break;
                }
                
                gettimeofday(&sendTime, nullptr);
                
                // Receive response
                char response[4];
                socklen_t addrLen = sizeof(serverAddr);
                if (recvfrom(dataSocket, response, sizeof(response), 0, 
                           (struct sockaddr*)&serverAddr, &addrLen) < 0) {
                    std::cerr << "Failed to receive UDP response" << std::endl;
                    break;
                }
                
                gettimeofday(&recvTime, nullptr);
                replies++;
                
                // Calculate round-trip time in milliseconds
                double roundTripTime = (recvTime.tv_sec - sendTime.tv_sec) * 1000.0 + 
                                       (recvTime.tv_usec - sendTime.tv_usec) / 1000.0;
                
                repTimeSum += roundTripTime;
                min = std::min(min, roundTripTime);
                max = std::max(max, roundTripTime);
                avg = repTimeSum / replies;
                
                // Calculate jitter
                if (i > 0) {
                    double recvInterval = (recvTime.tv_sec - lastRecvTime.tv_sec) * 1000.0 + 
                                         (recvTime.tv_usec - lastRecvTime.tv_usec) / 1000.0;
                    
                    numIntervals++;
                    double jitterDelta = std::abs(recvInterval - meanRecvInterval);
                    meanJitter = (meanJitter * (numIntervals - 1) + jitterDelta) / numIntervals;
                    meanRecvInterval = (meanRecvInterval * (numIntervals - 1) + recvInterval) / numIntervals;
                }
                
                lastRecvTime = recvTime;
                
                // Output statistics periodically
                gettimeofday(&endTime, nullptr);
                double elapsed = (endTime.tv_sec - startTime.tv_sec) + 
                                 (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
                
                if (elapsed >= options.stat / 1000.0) {
                    statOutputResponse(elapsed, replies, min, max, avg, meanJitter);
                    startTime = endTime;
                }
                
                // Maintain packet rate if specified
                if (sendDelay > 0) {
                    gettimeofday(&endTime, nullptr);
                    double packetElapsed = (endTime.tv_sec - sendTime.tv_sec) * 1000000.0 + 
                                          (endTime.tv_usec - sendTime.tv_usec);
                    
                    if (packetElapsed < sendDelay) {
                        usleep(static_cast<useconds_t>(sendDelay - packetElapsed));
                    }
                }
            }
        }
        else if (options.mode == NetOptions::SEND) {
            // Send mode
            int bufSize = options.pktsize / 4;
            std::vector<int> buffer(bufSize);
            
            // Set buffer size if specified
            if (options.sbufsize > 0) {
                setsockopt(dataSocket, SOL_SOCKET, SO_SNDBUF, 
                           &options.sbufsize, sizeof(options.sbufsize));
            }
            
            // Calculate delay between packets based on rate
            float sendDelay = 0.0f;
            if (options.pktrate > 0) {
                sendDelay = 1000000.0f * options.pktsize / options.pktrate;
            }
            
            int statIndex = 1, packetsSent = 0;
            struct timeval startTime, sentTime;
            gettimeofday(&startTime, nullptr);
            
            for (int i = 0; i < options.pktnum; i++) {
                // Fill buffer with packet index
                std::fill(buffer.begin(), buffer.end(), 0);
                buffer[0] = i + 1; // Set packet index
                
                if (options.pktrate > 0) {
                    gettimeofday(&startTime, nullptr);
                }
                
                // Send data
                if (sendto(dataSocket, buffer.data(), buffer.size() * sizeof(int), 0, 
                         (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                    std::cerr << "Failed to send UDP data" << std::endl;
                    break;
                }
                
                // Handle rate limiting
                if (options.pktrate > 0) {
                    gettimeofday(&sentTime, nullptr);
                    double elapsed = (sentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                     (sentTime.tv_usec - startTime.tv_usec);
                    
                    // Output statistics if necessary
                    double statTime = options.stat * 1000.0; // microseconds
                    while (elapsed < sendDelay) {
                        Sleep(10); // Sleep for 10ms
                        elapsed += 10000;
                        
                        if (elapsed >= statTime) {
                            statOutput(NetOptions::SEND, options.stat / 1000.0, 0, 0, 0, 
                                      options.pktrate, 0, statIndex++);
                            statTime += options.stat * 1000.0;
                        }
                    }
                }
                else { // Unlimited rate, measure throughput
                    gettimeofday(&sentTime, nullptr);
                    double elapsed = (sentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                     (sentTime.tv_usec - startTime.tv_usec);
                    
                    double statTime = options.stat / 1000.0 * 1000000.0; // microseconds
                    if (elapsed >= statTime) {
                        int packetsDelta = i - packetsSent;
                        double rate = (packetsDelta * options.pktsize) / (elapsed / 1000000.0);
                        
                        statOutput(NetOptions::SEND, options.stat / 1000.0, 0, 0, 0, 
                                  rate, 0, statIndex++);
                        
                        packetsSent = i;
                        gettimeofday(&startTime, nullptr);
                    }
                }
            }
        }
        else if (options.mode == NetOptions::RECV) {
            // Receive mode - Bind to a local port first
            struct sockaddr_in clientAddr;
            memset(&clientAddr, 0, sizeof(clientAddr));
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_addr.s_addr = INADDR_ANY;
            clientAddr.sin_port = 0; // Let the system choose a port
            
            if (::bind(dataSocket, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
                std::cerr << "Failed to bind UDP socket: " << strerror(errno) << std::endl;
                closesocket(dataSocket);
                shutdownSockets();
                return;
            }
            
            // Get the assigned port
            socklen_t addrLen = sizeof(clientAddr);
            if (getsockname(dataSocket, (struct sockaddr*)&clientAddr, &addrLen) < 0) {
                std::cerr << "Failed to get socket name: " << strerror(errno) << std::endl;
                closesocket(dataSocket);
                shutdownSockets();
                return;
            }
            
            std::cout << "UDP socket bound to port " << ntohs(clientAddr.sin_port) << std::endl;
            
            // Set buffer size if specified
            if (options.rbufsize > 0) {
                setsockopt(dataSocket, SOL_SOCKET, SO_RCVBUF, 
                           &options.rbufsize, sizeof(options.rbufsize));
            }
            
            int bufSize = options.pktsize / 4;
            std::vector<int> buffer(bufSize);
            
            int pktIndex = 0, pktLoss = 0, pktRecv = 0, statIndex = 1;
            int numIntervals = 0;
            double meanJitter = 0, meanRecvInterval = 0;
            
            struct timeval startTime, currentTime;
            struct timeval recvTime, lastRecvTime;
            gettimeofday(&startTime, nullptr);
            gettimeofday(&lastRecvTime, nullptr);
            
            for (int i = 0; i < options.pktnum; i++) {
                // Receive data
                socklen_t serverAddrLen = sizeof(serverAddr);
                ssize_t bytesRead = recvfrom(dataSocket, buffer.data(), buffer.size() * sizeof(int), 
                                           0, (struct sockaddr*)&serverAddr, &serverAddrLen);
                
                if (bytesRead <= 0) {
                    std::cerr << "Failed to receive UDP data: " << strerror(errno) << std::endl;
                    break;
                }
                
                pktRecv++;
                gettimeofday(&currentTime, nullptr);
                
                // Check for packet loss
                if (buffer[0] != pktIndex) {
                    pktLoss++;
                } else {
                    // Calculate jitter
                    gettimeofday(&recvTime, nullptr);
                    double recvInterval = (recvTime.tv_sec - lastRecvTime.tv_sec) * 1000.0 + 
                                         (recvTime.tv_usec - lastRecvTime.tv_usec) / 1000.0;
                    
                    lastRecvTime = recvTime;
                    numIntervals++;
                    
                    double jitterDelta = std::abs(recvInterval - meanRecvInterval);
                    meanJitter = (meanJitter * (numIntervals - 1) + jitterDelta) / numIntervals;
                    meanRecvInterval = (meanRecvInterval * (numIntervals - 1) + recvInterval) / numIntervals;
                    
                    // Convert to appropriate units
                    meanJitter /= 1000000.0;
                }
                
                pktIndex++;
                
                // Output statistics periodically
                double elapsed = (currentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                (currentTime.tv_usec - startTime.tv_usec);
                
                if (elapsed >= options.stat * 1000.0) {
                    double rate = pktRecv * buffer.size() * sizeof(int) / (elapsed / 1000000.0);
                    double lossRate = (pktLoss * 100.0) / pktIndex;
                    
                    statOutput(NetOptions::RECV, options.stat / 1000.0, pktIndex, pktLoss, 
                              lossRate, rate * 1000, meanJitter, statIndex++);
                    
                    pktRecv = 0;
                    gettimeofday(&startTime, nullptr);
                }
            }
        }
        
        // Clean up
        closesocket(dataSocket);
    }
    
    std::cout << std::endl << "Mission Completed" << std::endl;
    shutdownSockets();
}

int main(int argc, char** argv) {
    std::cout << "Network Probe Client" << std::endl;
    
    // Parse command line options
    NetOptions options;
    
    static struct option longOptions[] = {
        {"send",     no_argument,       0, '1'},
        {"recv",     no_argument,       0, '2'},
        {"response", no_argument,       0, '3'},
        {"http",     required_argument, 0, '4'},
        {"stat",     required_argument, 0, 'a'},
        {"rhost",    required_argument, 0, 'b'},
        {"rport",    required_argument, 0, 'c'},
        {"proto",    required_argument, 0, 'd'},
        {"pktsize",  required_argument, 0, 'e'},
        {"pktrate",  required_argument, 0, 'f'},
        {"pktnum",   required_argument, 0, 'g'},
        {"sbufsize", required_argument, 0, 'h'},
        {"rbufsize", required_argument, 0, 'k'},
        {"presist",  required_argument, 0, 'l'},
        {"file",     required_argument, 0, 'm'},
        {0, 0, 0, 0}
    };
    
    int optionIndex = 0;
    int opt;
    
    while ((opt = getopt_long_only(argc, argv, "", longOptions, &optionIndex)) != -1) {
        try {
            switch (opt) {
                case '1':
                    options.mode = NetOptions::SEND;
                    break;
                
                case '2':
                    options.mode = NetOptions::RECV;
                    break;
                
                case '3':
                    options.mode = NetOptions::RESPONSE;
                    break;
                
                case '4':
                    options.mode = NetOptions::HTTP;
                    options.url = optarg;
                    break;
                
                case 'a':
                    options.stat = std::stoi(optarg);
                    break;
                
                case 'b':
                    options.rhost = optarg;
                    // Try to resolve hostname
                    {
                        struct hostent* he = gethostbyname(options.rhost.c_str());
                        if (!he) {
                            std::cerr << "Failed to resolve hostname: " << options.rhost << std::endl;
                        } else {
                            struct in_addr** addrList = (struct in_addr**)he->h_addr_list;
                            std::cout << "IP Address: " << inet_ntoa(*addrList[0]) << std::endl;
                        }
                    }
                    break;
                
                case 'c':
                    options.rport = std::stoi(optarg);
                    break;
                
                case 'd':
                    options.proto = optarg;
                    break;
                
                case 'e':
                    options.pktsize = std::stoi(optarg);
                    break;
                
                case 'f':
                    options.pktrate = std::stoi(optarg);
                    break;
                
                case 'g':
                    options.pktnum = std::stoi(optarg);
                    break;
                
                case 'h':
                    options.sbufsize = std::stoi(optarg);
                    break;
                
                case 'k':
                    options.rbufsize = std::stoi(optarg);
                    break;
                
                case 'l':
                    options.isPresistent = optarg;
                    break;
                
                case 'm':
                    options.filename = optarg;
                    break;
                
                default:
                    std::cerr << "Unknown option" << std::endl;
                    return EXIT_FAILURE;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing argument: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    // Validate options
    if (options.mode == NetOptions::UNDEFINED) {
        std::cerr << "Error: Mode not specified" << std::endl;
        return EXIT_FAILURE;
    }
    
    // Print configuration
    std::cout << "Mode:                ";
    switch (options.mode) {
        case NetOptions::SEND:     std::cout << "SEND"; break;
        case NetOptions::RECV:     std::cout << "RECV"; break;
        case NetOptions::RESPONSE: std::cout << "RESPONSE"; break;
        case NetOptions::HTTP:     std::cout << "HTTP"; break;
        default:                   std::cout << "UNKNOWN"; break;
    }
    std::cout << std::endl;
    
    std::cout << "Stat:                " << options.stat << " ms" << std::endl;
    std::cout << "Remote Host:         " << options.rhost << std::endl;
    std::cout << "Remote Port:         " << options.rport << std::endl;
    std::cout << "Protocol:            " << options.proto << std::endl;
    std::cout << "Packet Size:         " << options.pktsize << " bytes" << std::endl;
    std::cout << "Packet Rate:         " << options.pktrate << " bytes/second" << std::endl;
    std::cout << "Packet Number:       ";
    
    if (options.pktnum == INFINITE_PACKETS) {
        std::cout << "Infinite" << std::endl;
    } else {
        std::cout << options.pktnum << std::endl;
    }
    
    std::cout << "Send Buffer Size:    " << options.sbufsize << " bytes" << std::endl;
    std::cout << "Receive Buffer Size: " << options.rbufsize << " bytes" << std::endl;
    std::cout << "Persistent:          " << options.isPresistent << std::endl;
    std::cout << "File:                " << options.filename << std::endl;
    
    // Initialize client
    initializeClient(options);
    
    return EXIT_SUCCESS;
}