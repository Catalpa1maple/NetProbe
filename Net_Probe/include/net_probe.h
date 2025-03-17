#ifndef NET_PROBE_H
#define NET_PROBE_H

#include "common.h"
#include <memory>
#include <functional>
#include <chrono>

// Forward declarations
class ThreadPool;

// Application settings
class NetOptions {
public:
    enum Mode {
        UNDEFINED = -1,
        SEND = 0,
        RECV = 1,
        RESPONSE = 2,
        HTTP = 3,
        HTTPS = 4
    };

    // Default initialization
    Mode mode = UNDEFINED;
    int stat = 500; // Stats reporting interval in ms
    std::string rhost = "localhost";
    std::string lhost = "IN_ADDR_ANY";
    int rport = 4180;
    int lport = 4180;
    std::string proto = "UDP";
    int pktsize = 1000;
    int pktrate = 1000;
    int pktnum = INFINITE_PACKETS;
    int sbufsize = 0;
    int rbufsize = 0;
    std::string isPresistent = "No";
    std::string tcpcca;
    int poolsize = 8;
    std::string url;
    std::string filename = "/dev/null";
    int lhttpport = 4080;
    int lhttpsport = 4081;

    // Validity check
    bool isValid() const {
        return mode != UNDEFINED;
    }

    // Helper for string to Mode conversion
    static Mode stringToMode(const std::string& modeStr) {
        if (modeStr == "send") return SEND;
        if (modeStr == "recv") return RECV;
        if (modeStr == "response") return RESPONSE;
        if (modeStr == "http") return HTTP;
        if (modeStr == "https") return HTTPS;
        return UNDEFINED;
    }
};

// Client information structure
class ClientInfo {
public:
    int socket = -1;
    struct sockaddr_in addr;
    int msg[7] = {0}; // [0:mode, 1:stat, 2:proto, 3:pktsize, 4:pktrate, 5:pktnum, 6:isPresistent]
    int snr_buf[2] = {0};
    std::string host;
    int port = 0;
    int http_port = 0;
    int https_port = 0;
    
    ~ClientInfo() {
        // Clean up socket if still open
        if (socket >= 0) {
            closesocket(socket);
            socket = -1;
        }
    }
};

// Statistics reporting function
void serverInfo(double elapsed, int tcpClients, int udpClients, 
                int activeThreads, int poolSize);

// Network utility functions
bool sendData(int socket, const void* data, size_t length);
ssize_t receiveData(int socket, void* buffer, size_t maxLength, int flags = 0);
int createServerSocket(int port, const std::string& bindAddress = "0.0.0.0");
int connectToServer(const std::string& host, int port);

// Message handling
std::vector<int> generateMessage(const NetOptions& options);
void handleClientMessage(std::shared_ptr<ClientInfo> clientInfo);

#endif // NET_PROBE_H