#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <queue>

// Platform-specific includes and definitions
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/fcntl.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    
    // Windows to Unix compatibility macros
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define WSAGetLastError() (errno)
    #define closesocket(s) close(s)
    #define ioctlsocket ioctl
    #define WSAEWOULDBLOCK EWOULDBLOCK
    #define SD_SEND SHUT_WR
    #define SD_RECEIVE SHUT_RD
    #define SD_BOTH SHUT_RDWR
    #define Sleep(x) usleep((x)*1000)
#endif

// OpenSSL includes
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

// Constants
constexpr int INFINITE_PACKETS = 1000000000;
constexpr size_t MAX_BUFFER_SIZE = 8192;

// Error handling helper
inline void logError(const std::string& message) {
    std::cerr << "Error: " << message << " - " << strerror(errno) << std::endl;
}

// Socket initialization
inline bool initializeSockets() {
#ifdef _WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        logError("Unable to initialize winsock.dll");
        return false;
    }
    
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        WSACleanup();
        logError("Incorrect winsock version, expecting 2.2");
        return false;
    }
#endif
    return true;
}

// Socket cleanup
inline void shutdownSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Safer string functions
inline void safeCopy(char* dest, size_t destSize, const char* src) {
#ifdef _WIN32
    strncpy_s(dest, destSize, src, destSize - 1);
#else
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
#endif
}

// Network utility functions
inline bool isValidIpAddress(const std::string& ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 0;
}

// Thread-safe output (prevents garbled console)
extern std::mutex outputMutex;
template<typename... Args>
void threadSafeLog(Args... args) {
    std::lock_guard<std::mutex> lock(outputMutex);
    (std::cout << ... << args) << std::endl;
}

#endif // COMMON_H