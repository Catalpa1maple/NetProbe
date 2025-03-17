#include "../include/http_server.h"
#include "../include/net_probe.h"
#include <iostream>
#include <string>
#include <vector>

// HTTP response headers and content
const char httpHeaders[] = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n";

const char httpResponse[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "    <title>IERG 4180 Project 4</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>IERG 4180 Project 4</h1>\n"
    "    <p>HTTP Server</p>\n"
    "</body>\n"
    "</html>\n";

const char httpsResponse[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "    <title>IERG 4180 Project 4</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>IERG 4180 Project 4</h1>\n"
    "    <p>HTTPS Server</p>\n"
    "</body>\n"
    "</html>\n";

// Initialize OpenSSL library
void initOpenSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

// Cleanup OpenSSL resources
void cleanupOpenSSL() {
    EVP_cleanup();
}

// Create SSL context for server
SSL_CTX* createSSLContext() {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        std::cerr << "Unable to create SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        return nullptr;
    }
    
    return ctx;
}

// Configure SSL context with certificates
bool configureSSLContext(SSL_CTX* ctx, const std::string& certFile, const std::string& keyFile) {
    if (!ctx) return false;
    
    SSL_CTX_set_ecdh_auto(ctx, 1);

    // Load certificate file
    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Error loading certificate file: " << certFile << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Load private key file
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Error loading private key file: " << keyFile << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    return true;
}

// Create a socket and bind to port
int createSocket(int port, const std::string& bindAddress) {
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

    if (listen(socketFd, 5) < 0) {
        std::cerr << "Unable to listen on port " << port << ": " << strerror(errno) << std::endl;
        closesocket(socketFd);
        return -1;
    }
    
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Server listening on port " << port << std::endl;
    }
    
    return socketFd;
}

// SSL Context wrapper implementation
SSLContextWrapper::SSLContextWrapper() {
    initOpenSSL();
    ctx = createSSLContext();
    configureSSLContext(ctx, "domain.crt", "domain.key");
}

SSLContextWrapper::~SSLContextWrapper() {
    if (ctx) {
        SSL_CTX_free(ctx);
    }
    cleanupOpenSSL();
}

// SSL Connection wrapper implementation
SSLConnectionWrapper::SSLConnectionWrapper(SSL_CTX* ctx, int socket) {
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket);
}

SSLConnectionWrapper::~SSLConnectionWrapper() {
    if (ssl) {
        SSL_free(ssl);
    }
}

bool SSLConnectionWrapper::accept() {
    return SSL_accept(ssl) > 0;
}

int SSLConnectionWrapper::read(void* buffer, int length) {
    return SSL_read(ssl, buffer, length);
}

int SSLConnectionWrapper::write(const void* buffer, int length) {
    return SSL_write(ssl, buffer, length);
}

int SSLConnectionWrapper::shutdown() {
    return SSL_shutdown(ssl);
}

// HTTPS server handler function
void httpsServerHandler(std::shared_ptr<ClientInfo> clientInfo) {
    // Create and configure SSL context
    SSLContextWrapper contextWrapper;
    if (!contextWrapper.get()) {
        std::cerr << "Failed to create SSL context" << std::endl;
        return;
    }
    
    // Create socket for HTTPS server
    int serverSocket = createSocket(clientInfo->https_port, "0.0.0.0");
    if (serverSocket < 0) {
        std::cerr << "Failed to create HTTPS server socket" << std::endl;
        return;
    }
    
    std::cout << "HTTPS Server listening on port " << clientInfo->https_port << std::endl;
    
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    static std::atomic<int> tcpClients(0);
    
    while (true) {
        // Accept client connection
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            continue;
        }
        
        // Create SSL connection
        SSLConnectionWrapper sslConnection(contextWrapper.get(), clientSocket);
        
        if (!sslConnection.accept()) {
            // SSL handshake failed
            ERR_print_errors_fp(stderr);
            closesocket(clientSocket);
            continue;
        }
        
        // Handle HTTPS request
        char request[4096] = {0};
        int bytesRead = sslConnection.read(request, sizeof(request) - 1);
        
        if (bytesRead > 0 && strncmp(request, "GET ", 4) == 0) {
            // Send headers
            sslConnection.write(httpHeaders, strlen(httpHeaders));
            
            // Send content
            sslConnection.write(httpsResponse, strlen(httpsResponse));
        }
        
        // Properly shut down SSL connection
        int shutdownResult = sslConnection.shutdown();
        int attempts = 30;
        
        while (shutdownResult != 1 && attempts--) {
            Sleep(100);
            shutdownResult = sslConnection.shutdown();
            
            if (shutdownResult < 0) {
                int sslError = SSL_get_error(sslConnection.get(), shutdownResult);
                std::cerr << "SSL shutdown error: " << sslError << std::endl;
                break;
            }
        }
        
        // Close client socket
        closesocket(clientSocket);
        tcpClients++;
    }
    
    // Close server socket
    closesocket(serverSocket);
}

// HTTP server handler function
void httpServerHandler(std::shared_ptr<ClientInfo> clientInfo) {
    // Create socket for HTTP server
    int serverSocket = createSocket(clientInfo->http_port, "0.0.0.0");
    if (serverSocket < 0) {
        std::cerr << "Failed to create HTTP server socket" << std::endl;
        return;
    }
    
    std::cout << "HTTP Server listening on port " << clientInfo->http_port << std::endl;
    
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    static std::atomic<int> tcpClients(0);
    
    while (true) {
        // Accept client connection
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            continue;
        }
        
        // Handle HTTP request
        char request[4096] = {0};
        int bytesRead = receiveData(clientSocket, request, sizeof(request) - 1);
        
        if (bytesRead > 0) {
            if (strncmp(request, "GET / HTTP/1.1", 14) == 0) {
                // Send headers
                if (!sendData(clientSocket, httpHeaders, strlen(httpHeaders))) {
                    std::cerr << "Failed to send HTTP headers" << std::endl;
                    closesocket(clientSocket);
                    continue;
                }
                
                // Send content
                if (!sendData(clientSocket, httpResponse, strlen(httpResponse))) {
                    std::cerr << "Failed to send HTTP response" << std::endl;
                    closesocket(clientSocket);
                    continue;
                }
            } else {
                // Send 404 response for other requests
                const char* notFoundResponse = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: 9\r\n"
                    "\r\n"
                    "Not Found";
                
                if (!sendData(clientSocket, notFoundResponse, strlen(notFoundResponse))) {
                    std::cerr << "Failed to send 404 response" << std::endl;
                    closesocket(clientSocket);
                    continue;
                }
            }
        } else {
            std::cerr << "Failed to receive HTTP request" << std::endl;
        }
        
        // Close client socket
        closesocket(clientSocket);
        tcpClients++;
    }
    
    // Close server socket
    closesocket(serverSocket);
}