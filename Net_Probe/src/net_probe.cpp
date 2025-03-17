#include "../include/net_probe.h"
#include <iostream>
#include <iomanip>
#include <string>

// Network utility functions
bool sendData(int socket, const void* data, size_t length) {
    const char* buffer = static_cast<const char*>(data);
    size_t totalSent = 0;
    
    while (totalSent < length) {
        ssize_t sent = send(socket, buffer + totalSent, length - totalSent, 0);
        if (sent <= 0) {
            if (errno == EINTR) continue; // Interrupted, try again
            std::cerr << "Error sending data: " << strerror(errno) << std::endl;
            return false;
        }
        totalSent += sent;
    }
    return true;
}

ssize_t receiveData(int socket, void* buffer, size_t maxLength, int flags) {
    ssize_t totalReceived = 0;
    
    while (totalReceived < maxLength) {
        ssize_t received = recv(socket, 
                              static_cast<char*>(buffer) + totalReceived, 
                              maxLength - totalReceived, 
                              flags);
        
        if (received < 0) {
            if (errno == EINTR) continue; // Interrupted, try again
            std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
            return -1;
        }
        
        if (received == 0) { // Connection closed
            break;
        }
        
        totalReceived += received;
        
        // If we're not trying to fill the buffer completely, return what we got
        if (flags & MSG_DONTWAIT) break;
    }
    
    return totalReceived;
}

int createServerSocket(int port, const std::string& bindAddress) {
    int socketFd;
    struct sockaddr_in addr;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (bindAddress == "0.0.0.0" || bindAddress == "IN_ADDR_ANY") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        addr.sin_addr.s_addr = inet_addr(bindAddress.c_str());
    }

    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        std::cerr << "Unable to create socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // Set SO_REUSEADDR to avoid "address already in use" errors
    int opt = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << strerror(errno) << std::endl;
        closesocket(socketFd);
        return -1;
    }

    if (::bind(socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Unable to bind to port " << port << ": " << strerror(errno) << std::endl;
        closesocket(socketFd);
        return -1;
    }

    if (listen(socketFd, 10) < 0) {
        std::cerr << "Unable to listen on port " << port << ": " << strerror(errno) << std::endl;
        closesocket(socketFd);
        return -1;
    }
    
    return socketFd;
}

int connectToServer(const std::string& host, int port) {
    struct hostent* server;
    struct sockaddr_in serverAddr;
    
    // Get host information
    server = gethostbyname(host.c_str());
    if (!server) {
        std::cerr << "Error resolving hostname: " << host << std::endl;
        return -1;
    }
    
    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return -1;
    }
    
    // Set up server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error connecting to server: " << strerror(errno) << std::endl;
        closesocket(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Generate message from options
std::vector<int> generateMessage(const NetOptions& options) {
    std::vector<int> msg(7, 0);
    
    // Fill message fields
    msg[0] = static_cast<int>(options.mode);  // Mode
    msg[1] = options.stat;                    // Stats reporting interval
    msg[2] = (options.proto == "TCP") ? 0 : 1; // Protocol (0=TCP, 1=UDP)
    msg[3] = options.pktsize;                 // Packet size
    msg[4] = options.pktrate;                 // Packet rate
    msg[5] = (options.pktnum == INFINITE_PACKETS) ? 0 : options.pktnum; // Packet count
    msg[6] = (options.isPresistent == "Yes") ? 1 : 0; // Persistent connection
    
    return msg;
}

// Handle client message
void handleClientMessage(std::shared_ptr<ClientInfo> clientInfo) {
    // Log received message information (for debugging)
    auto msg = clientInfo->msg;
    
    std::string modeStr;
    switch(msg[0]) {
        case 0: modeStr = "Send"; break;
        case 1: modeStr = "Receive"; break;
        case 2: modeStr = "Response"; break;
        case 3: modeStr = "HTTP"; break;
        default: modeStr = "Unknown"; break;
    }
    
    std::string protoStr = (msg[2] == 0) ? "TCP" : "UDP";
    
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Client connection from " 
                  << inet_ntoa(clientInfo->addr.sin_addr) << ":" 
                  << ntohs(clientInfo->addr.sin_port) << std::endl;
        std::cout << "  Mode: " << modeStr << ", Protocol: " << protoStr 
                  << ", Packet size: " << msg[3] << " bytes" << std::endl;
        std::cout << "  Rate: " << msg[4] << " bytes/s, Count: " 
                  << (msg[5] == 0 ? "Infinite" : std::to_string(msg[5])) << std::endl;
    }
    
    // If packet number is 0, set it to infinite
    if (clientInfo->msg[5] == 0) {
        clientInfo->msg[5] = INFINITE_PACKETS;
    }
}

// Server stats reporting
void serverInfo(double elapsed, int tcpClients, int udpClients, 
                int activeThreads, int poolSize) {
    std::lock_guard<std::mutex> lock(outputMutex);
    std::cout << "Elapsed " << std::fixed << std::setprecision(1) << elapsed << "s ";
    std::cout << "ThreadPool " << activeThreads + 1 << "/" << poolSize;
    std::cout << " TCP Clients " << tcpClients << " UDP Clients " << udpClients;
    std::cout << "\r";
    std::cout.flush();
}