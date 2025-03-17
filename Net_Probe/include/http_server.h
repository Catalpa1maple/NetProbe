#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "common.h"
#include <memory>

// Forward declarations
class ClientInfo;

// Initialize OpenSSL
void initOpenSSL();

// Cleanup OpenSSL resources
void cleanupOpenSSL();

// Create and configure SSL context
SSL_CTX* createSSLContext();
bool configureSSLContext(SSL_CTX* ctx, const std::string& certFile, const std::string& keyFile);

// Create a listening socket
int createSocket(int port, const std::string& bindAddress = "0.0.0.0");

// HTTPS server handler function
void httpsServerHandler(std::shared_ptr<ClientInfo> clientInfo);

// HTTP server handler function
void httpServerHandler(std::shared_ptr<ClientInfo> clientInfo);

// RAII wrapper for SSL context
class SSLContextWrapper {
public:
    SSLContextWrapper();
    ~SSLContextWrapper();
    
    SSL_CTX* get() const { return ctx; }
    
private:
    SSL_CTX* ctx;
};

// RAII wrapper for SSL connection
class SSLConnectionWrapper {
public:
    SSLConnectionWrapper(SSL_CTX* ctx, int socket);
    ~SSLConnectionWrapper();
    
    SSL* get() const { return ssl; }
    bool accept();
    int read(void* buffer, int length);
    int write(const void* buffer, int length);
    int shutdown();
    
private:
    SSL* ssl;
};

#endif // HTTP_SERVER_H