#include "../include/common.h"
#include "../include/net_probe.h"
#include "../include/thread_pool.h"
#include "../include/http_server.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <getopt.h>
#include <sys/time.h>

// Global counters for client connections
std::atomic<int> tcpClients(0);
std::atomic<int> udpClients(0);

// Global TCP congestion control algorithm
std::string tcpCongestionControl;

// Handle TCP client connections
void tcpThreadHandler(std::shared_ptr<ClientInfo> clientInfo) {
    // Create a TCP socket for the server side of the data connection
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = clientInfo->addr.sin_addr.s_addr; // Use client's IP

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create TCP socket: " << strerror(errno) << std::endl;
        return;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Set TCP congestion control algorithm if specified
#ifndef __APPLE__
    if (!tcpCongestionControl.empty()) {
        if (setsockopt(serverSocket, IPPROTO_TCP, TCP_CONGESTION, 
                        tcpCongestionControl.c_str(), tcpCongestionControl.length()) < 0) {
            std::cerr << "Failed to set TCP congestion control algorithm: " 
                      << strerror(errno) << std::endl;
        }
    }
#endif

    // Bind to any available port
    if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Get the port number assigned to the socket
    socklen_t addrLen = sizeof(serverAddr);
    if (getsockname(serverSocket, (struct sockaddr*)&serverAddr, &addrLen) < 0) {
        std::cerr << "Failed to get socket name: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }
    int port = ntohs(serverAddr.sin_port);

    // Send the port number to the client
    if (!sendData(clientInfo->socket, &port, sizeof(port))) {
        std::cerr << "Failed to send port to client" << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Accept connection from client
    int clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket < 0) {
        std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Enable TCP_NODELAY to disable Nagle's algorithm
    if (setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set TCP_NODELAY: " << strerror(errno) << std::endl;
    }

    // Close the original control connection as it's no longer needed
    closesocket(clientInfo->socket);

    // Set buffer sizes if specified
    if (clientInfo->snr_buf[0]) {
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, 
                   &clientInfo->snr_buf[0], sizeof(clientInfo->snr_buf[0]));
    }
    if (clientInfo->snr_buf[1]) {
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, 
                   &clientInfo->snr_buf[1], sizeof(clientInfo->snr_buf[1]));
    }

    // Handle based on client mode
    if (clientInfo->msg[0] == 2) {
        // Response mode
        char req[4];
        
        for (int i = 0; i < clientInfo->msg[5]; i++) {
            // If not persistent connection and not the first packet, create new connection
            if (i > 0 && !clientInfo->msg[6]) {
                closesocket(clientSocket);
                closesocket(serverSocket);
                usleep(1000); // Ensure the socket is closed
                
                // Create new socket
                serverSocket = socket(AF_INET, SOCK_STREAM, 0);
                if (serverSocket < 0) {
                    std::cerr << "Failed to create new socket: " << strerror(errno) << std::endl;
                    return;
                }
                
                opt = 1;
                setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                
                // Bind to next port
                serverAddr.sin_port = htons(port + i);
                if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                    std::cerr << "Failed to bind to port " << port + i << ": " 
                              << strerror(errno) << std::endl;
                    closesocket(serverSocket);
                    return;
                }
                
                // Listen and accept
                if (listen(serverSocket, 5) < 0) {
                    std::cerr << "Failed to listen on port " << port + i << ": " 
                              << strerror(errno) << std::endl;
                    closesocket(serverSocket);
                    return;
                }
                
                clientSocket = accept(serverSocket, nullptr, nullptr);
                if (clientSocket < 0) {
                    std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
                    closesocket(serverSocket);
                    return;
                }
            }
            
            // Receive request
            if (receiveData(clientSocket, req, 4) < 0) {
                std::cerr << "Failed to receive request" << std::endl;
                break;
            }
            
            // Send response
            if (!sendData(clientSocket, req, 4)) {
                std::cerr << "Failed to send response" << std::endl;
                break;
            }
            
            // Close connections if not persistent
            if (!clientInfo->msg[6]) {
                closesocket(clientSocket);
                closesocket(serverSocket);
            }
        }
    }
    else if (clientInfo->msg[0] == 0) {
        // Client Send Mode (Server Receiving)
        int bufSize = clientInfo->msg[3] / 4;
        std::vector<int> buffer(bufSize);
        
        for (int i = 0; i < clientInfo->msg[5]; i++) {
            int bytesReceived = 0;
            
            // Keep receiving until we get some data
            while (bytesReceived == 0) {
                bytesReceived = receiveData(clientSocket, buffer.data(), 
                                          buffer.size() * sizeof(int));
                if (bytesReceived < 0) {
                    break;
                }
            }
            
            if (bytesReceived < 0) {
                std::cerr << "Failed to receive data" << std::endl;
                break;
            }
        }
    }
    else if (clientInfo->msg[0] == 1) {
        // Client Recv Mode (Server Sending)
        int bufSize = clientInfo->msg[3] / 4;
        std::vector<int> buffer(bufSize);
        
        // Calculate delay between packets based on rate
        float sendDelay = 0.0;
        if (clientInfo->msg[4] != 0) {
            sendDelay = 1000000.0f * clientInfo->msg[3] / clientInfo->msg[4];
        }
        
        struct timeval startTime, sentTime;
        gettimeofday(&startTime, nullptr);
        double statTime = clientInfo->msg[1] * 1000.0; // in microseconds
        
        for (int i = 0; i < clientInfo->msg[5]; i++) {
            memset(buffer.data(), 0, buffer.size() * sizeof(int));
            buffer[0] = i; // Set packet sequence number
            
            if (!sendData(clientSocket, buffer.data(), buffer.size() * sizeof(int))) {
                std::cerr << "Failed to send data" << std::endl;
                break;
            }
            
            // Delay if rate is specified
            if (clientInfo->msg[4] != 0) {
                gettimeofday(&sentTime, nullptr);
                double elapsed = (sentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                 (sentTime.tv_usec - startTime.tv_usec);
                
                if (elapsed < sendDelay) {
                    usleep(static_cast<useconds_t>(sendDelay - elapsed));
                }
                
                gettimeofday(&startTime, nullptr);
            }
        }
    }

    // Clean up
    closesocket(clientSocket);
    closesocket(serverSocket);
    tcpClients++;
}

// Handle UDP client connections
void udpThreadHandler(std::shared_ptr<ClientInfo> clientInfo) {
    // Create a UDP socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
        return;
    }

    // Bind to any available port
    if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Get the port number assigned to the socket
    socklen_t addrLen = sizeof(serverAddr);
    if (getsockname(serverSocket, (struct sockaddr*)&serverAddr, &addrLen) < 0) {
        std::cerr << "Failed to get socket name: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }
    int port = ntohs(serverAddr.sin_port);

    // Send the port number to the client
    if (!sendData(clientInfo->socket, &port, sizeof(port))) {
        std::cerr << "Failed to send port to client" << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Close the original control connection as it's no longer needed
    closesocket(clientInfo->socket);

    // Set buffer sizes if specified
    if (clientInfo->snr_buf[0]) {
        setsockopt(serverSocket, SOL_SOCKET, SO_SNDBUF, 
                   &clientInfo->snr_buf[0], sizeof(clientInfo->snr_buf[0]));
    }
    if (clientInfo->snr_buf[1]) {
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVBUF, 
                   &clientInfo->snr_buf[1], sizeof(clientInfo->snr_buf[1]));
    }

    // Handle based on client mode
    if (clientInfo->msg[0] == 2) {
        // Response mode
        char req[4];
        struct sockaddr_in clientAddr = clientInfo->addr;
        clientAddr.sin_port = htons(clientInfo->port);
        
        for (int i = 0; i < clientInfo->msg[5]; i++) {
            // Receive request
            if (recvfrom(serverSocket, req, sizeof(req), 0, 
                         (struct sockaddr*)&serverAddr, &addrLen) < 0) {
                std::cerr << "Failed to receive request: " << strerror(errno) << std::endl;
                break;
            }
            
            // Send response
            if (sendto(serverSocket, req, sizeof(req), 0, 
                       (struct sockaddr*)&serverAddr, addrLen) < 0) {
                std::cerr << "Failed to send response: " << strerror(errno) << std::endl;
                break;
            }
        }
    }
    else if (clientInfo->msg[0] == 0) {
        // Client Send Mode (Server Receiving)
        int bufSize = clientInfo->msg[3] / 4;
        std::vector<int> buffer(bufSize);
        
        for (int i = 0; i < clientInfo->msg[5]; i++) {
            if (recvfrom(serverSocket, buffer.data(), buffer.size() * sizeof(int), 0,
                         (struct sockaddr*)&serverAddr, &addrLen) < 0) {
                std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                break;
            }
        }
    }
    else if (clientInfo->msg[0] == 1) {
        // Client Recv Mode (Server Sending)
        int bufSize = clientInfo->msg[3] / 4;
        std::vector<int> buffer(bufSize);
        
        struct sockaddr_in clientAddr = clientInfo->addr;
        clientAddr.sin_port = htons(clientInfo->port);
        
        // Calculate delay between packets based on rate
        float sendDelay = 0.0;
        if (clientInfo->msg[4] != 0) {
            sendDelay = 1000000.0f * clientInfo->msg[3] / clientInfo->msg[4];
        }
        
        struct timeval startTime, sentTime;
        gettimeofday(&startTime, nullptr);
        double statTime = clientInfo->msg[1] * 1000.0; // in microseconds
        
        for (int i = 0; i < clientInfo->msg[5]; i++) {
            memset(buffer.data(), 0, buffer.size() * sizeof(int));
            buffer[0] = i; // Set packet sequence number
            
            if (sendto(serverSocket, buffer.data(), buffer.size() * sizeof(int), 0,
                       (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
                std::cerr << "Failed to send data: " << strerror(errno) << std::endl;
                break;
            }
            
            // Delay if rate is specified
            if (clientInfo->msg[4] != 0) {
                gettimeofday(&sentTime, nullptr);
                double elapsed = (sentTime.tv_sec - startTime.tv_sec) * 1000000.0 + 
                                 (sentTime.tv_usec - startTime.tv_usec);
                
                if (elapsed < sendDelay) {
                    usleep(static_cast<useconds_t>(sendDelay - elapsed));
                }
                
                gettimeofday(&startTime, nullptr);
            }
        }
    }

    // Clean up
    closesocket(serverSocket);
    udpClients++;
}

// Initialize server
void initializeServer(const NetOptions& options) {
    // Create thread pool
    ThreadPool threadPool(options.poolsize);
    
    // Start HTTP and HTTPS servers
    auto httpInfo = std::make_shared<ClientInfo>();
    httpInfo->host = options.lhost;
    httpInfo->http_port = options.lhttpport;
    httpInfo->https_port = options.lhttpsport;
    
    threadPool.enqueue(httpInfo, httpServerHandler);
    threadPool.enqueue(httpInfo, httpsServerHandler);
    
    // Create main TCP server socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(options.lport);
    
    if (options.lhost == "IN_ADDR_ANY") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        serverAddr.sin_addr.s_addr = inet_addr(options.lhost.c_str());
    }
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create TCP socket: " << strerror(errno) << std::endl;
        return;
    }
    
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }
    
    if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port " << options.lport << ": " 
                  << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }
    
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on port " << options.lport << ": " 
                  << strerror(errno) << std::endl;
        closesocket(serverSocket);
        return;
    }
    
    std::cout << "TCP/UDP Server is listening on port " << options.lport << std::endl;
    
    // Main loop for accepting connections
    struct timeval startTime, currentTime;
    gettimeofday(&startTime, nullptr);
    
    while (true) {
        // Display server statistics periodically
        gettimeofday(&currentTime, nullptr);
        double elapsed = (currentTime.tv_sec - startTime.tv_sec) + 
                         (currentTime.tv_usec - startTime.tv_usec) / 1000000.0;
        
        if (elapsed > 2.0) {
            serverInfo(elapsed, tcpClients.load(), udpClients.load(),
                       threadPool.getActiveThreads(), threadPool.getPoolSize());
            gettimeofday(&startTime, nullptr);
        }
        
        // Accept new client connections
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        // Use select to avoid blocking indefinitely
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);
        
        // Set timeout to 1 second
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(serverSocket + 1, &readSet, nullptr, nullptr, &timeout);
        
        if (ready < 0) {
            std::cerr << "Select failed: " << strerror(errno) << std::endl;
            break;
        }
        
        if (ready == 0) {
            // Timeout, continue to update stats
            continue;
        }
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            continue;
        }
        
        // Create client info object
        auto clientInfo = std::make_shared<ClientInfo>();
        clientInfo->socket = clientSocket;
        clientInfo->addr = clientAddr;
        clientInfo->host = options.lhost;
        clientInfo->port = options.lport;
        clientInfo->snr_buf[0] = options.sbufsize;
        clientInfo->snr_buf[1] = options.rbufsize;
        
        // Receive client message
        if (receiveData(clientSocket, clientInfo->msg, sizeof(clientInfo->msg)) < 0) {
            std::cerr << "Failed to receive client message" << std::endl;
            continue;
        }
        
        // Process client message
        handleClientMessage(clientInfo);
        
        // Handle client based on protocol
        if (clientInfo->msg[2] == 0) { // TCP
            threadPool.enqueue(clientInfo, tcpThreadHandler);
            tcpClients++;
        } else if (clientInfo->msg[2] == 1) { // UDP
            threadPool.enqueue(clientInfo, udpThreadHandler);
            udpClients++;
        }
    }
    
    // Clean up
    closesocket(serverSocket);
}

int main(int argc, char** argv) {
    // Initialize sockets
    if (!initializeSockets()) {
        std::cerr << "Failed to initialize sockets" << std::endl;
        return EXIT_FAILURE;
    }
    
    // Parse command line options
    NetOptions options;
    
    static struct option longOptions[] = {
        {"lhost",      required_argument, 0, 'a'},
        {"lport",      required_argument, 0, 'b'},
        {"lhttpport",  required_argument, 0, 'e'},
        {"lhttpsport", required_argument, 0, 'f'},
        {"sbufsize",   required_argument, 0, 'c'},
        {"rbufsize",   required_argument, 0, 'd'},
        {"tcpcca",     required_argument, 0, 't'},
        {"poolsize",   required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    
    int optionIndex = 0;
    int opt;
    
    while ((opt = getopt_long_only(argc, argv, "", longOptions, &optionIndex)) != -1) {
        try {
            switch (opt) {
                case 'a':
                    options.lhost = optarg;
                    std::cout << "lhost: " << options.lhost << std::endl;
                    break;
                
                case 'b':
                    options.lport = std::stoi(optarg);
                    std::cout << "lport: " << options.lport << std::endl;
                    break;
                
                case 'c':
                    options.sbufsize = std::stoi(optarg);
                    std::cout << "sbufsize: " << options.sbufsize << std::endl;
                    break;
                
                case 'd':
                    options.rbufsize = std::stoi(optarg);
                    std::cout << "rbufsize: " << options.rbufsize << std::endl;
                    break;
                
                case 'e':
                    options.lhttpport = std::stoi(optarg);
                    std::cout << "lhttpport: " << options.lhttpport << std::endl;
                    break;
                
                case 'f':
                    options.lhttpsport = std::stoi(optarg);
                    std::cout << "lhttpsport: " << options.lhttpsport << std::endl;
                    break;
                
                case 't':
                    tcpCongestionControl = optarg;
                    std::cout << "tcpcca: " << tcpCongestionControl << std::endl;
                    break;
                
                case 'p':
                    options.poolsize = std::stoi(optarg);
                    std::cout << "poolsize: " << options.poolsize << std::endl;
                    break;
                
                default:
                    std::cerr << "Invalid option" << std::endl;
                    return EXIT_FAILURE;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing argument: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    // Initialize and run server
    initializeServer(options);
    
    // Clean up sockets
    shutdownSockets();
    
    return EXIT_SUCCESS;
}