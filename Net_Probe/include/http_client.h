#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "common.h"
#include <memory>
#include <string>

// Forward declarations
class NetOptions;

// Initialize OpenSSL for client use
void initClientOpenSSL();

// Cleanup OpenSSL resources
void cleanupClientOpenSSL();

// Initialize trust store for certificate validation
int initTrustStore(SSL_CTX* ctx);

// Create socket and connect to server
int createClientSocket(const char* url, BIO* outbio);

// HTTP GET request
std::string httpGet(const std::string& url, const std::string& outputFile = "");

// HTTPS GET request
std::string httpsGet(const std::string& url, const std::string& outputFile = "");

// HTTP/HTTPS client handler
void httpClientHandler(const NetOptions& options);

// Parse hostname from URL
std::string extractHostname(const std::string& url);

// Parse URL components
struct URLComponents {
    std::string protocol;
    std::string hostname;
    std::string path;
    int port;
};

URLComponents parseURL(const std::string& url);

#endif // HTTP_CLIENT_H