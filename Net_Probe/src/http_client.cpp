#include "../include/http_client.h"
#include "../include/net_probe.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Global hostname buffer
char hostname[MAX_BUFFER_SIZE];

// Initialize OpenSSL for client use
void initClientOpenSSL() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
}

// Cleanup OpenSSL resources
void cleanupClientOpenSSL() {
    EVP_cleanup();
}

// Initialize trust store for certificate validation
int initTrustStore(SSL_CTX* ctx) {
#ifdef _WIN32
    HCERTSTORE hCertStore;
    PCCERT_CONTEXT pCertContext = NULL;

    if (!(hCertStore = CertOpenSystemStore(0, L"ROOT"))) {
        std::cerr << "Failed to open the ROOT store" << std::endl;
        return -1;
    }

    X509_STORE* xStore = SSL_CTX_get_cert_store(ctx);
    if (!xStore) {
        std::cerr << "SSL_CTX_get_cert_store(ctx) failed" << std::endl;
        CertCloseStore(hCertStore, 0);
        return -1;
    }

    while (pCertContext = CertEnumCertificatesInStore(hCertStore, pCertContext)) {
        X509* x = d2i_X509(NULL, &pCertContext->pbCertEncoded, pCertContext->cbCertEncoded);
        if (!x) continue;

        X509_STORE_add_cert(xStore, x);
        X509_free(x);
    }

    CertCloseStore(hCertStore, 0);
    return 0;
#else
    if (!SSL_CTX_load_verify_locations(ctx, 0, "/etc/ssl/certs")) {
        std::cerr << "Unable to set default verify paths" << std::endl;
        return -1;
    }
    return 0;
#endif
}

// Parse URL components
URLComponents parseURL(const std::string& url) {
    URLComponents components;
    
    // Default values
    components.port = 80;
    components.path = "/";
    
    // Parse protocol
    size_t protocolEnd = url.find("://");
    if (protocolEnd != std::string::npos) {
        components.protocol = url.substr(0, protocolEnd);
        if (components.protocol == "https") {
            components.port = 443;
        }
    } else {
        // No protocol specified, assume http
        components.protocol = "http";
        protocolEnd = 0;
    }
    
    // Skip "://" if present
    size_t hostStart = (protocolEnd == std::string::npos) ? 0 : protocolEnd + 3;
    
    // Parse hostname and port
    size_t hostEnd = url.find('/', hostStart);
    std::string hostPort = url.substr(hostStart, (hostEnd == std::string::npos) ? std::string::npos : hostEnd - hostStart);
    
    // Check for port
    size_t portPos = hostPort.find(':');
    if (portPos != std::string::npos) {
        components.hostname = hostPort.substr(0, portPos);
        try {
            components.port = std::stoi(hostPort.substr(portPos + 1));
        } catch (const std::exception&) {
            // Use default port if conversion fails
        }
    } else {
        components.hostname = hostPort;
    }
    
    // Parse path
    if (hostEnd != std::string::npos) {
        components.path = url.substr(hostEnd);
    }
    
    return components;
}

// Extract hostname from URL
std::string extractHostname(const std::string& url) {
    return parseURL(url).hostname;
}

// Create socket and connect to server
int createClientSocket(const char* url_str, BIO* out) {
    int sockfd;
    char portnum[6] = "443";
    char proto[6] = "";
    char* tmp_ptr = NULL;
    int port;
    struct hostent* host;
    struct sockaddr_in dest_addr;

    // Copy hostname from URL, removing any trailing slash
    if (url_str[strlen(url_str)] == '/') {
        char* temp = strdup(url_str);
        temp[strlen(temp) - 1] = '\0';
        safeCopy(hostname, sizeof(hostname), temp);
        free(temp);
    } else {
        safeCopy(hostname, sizeof(hostname), url_str);
    }

    // Extract hostname from URL
    if (strstr(url_str, "://")) {
        safeCopy(hostname, sizeof(hostname), strstr(url_str, "://") + 3);
    } else {
        safeCopy(hostname, sizeof(hostname), url_str);
    }

    // Extract port number if present
    if (strchr(hostname, ':')) {
        tmp_ptr = strchr(hostname, ':');
        safeCopy(portnum, sizeof(portnum), tmp_ptr + 1);
        *tmp_ptr = '\0';
    }

    port = atoi(portnum);

    // Resolve hostname
    host = gethostbyname(hostname);
    if (!host) {
        if (out) BIO_printf(out, "Error: Cannot resolve hostname %s.\n", hostname);
        return -1;
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        if (out) BIO_printf(out, "Error: Cannot create socket.\n");
        return -1;
    }

    // Prepare connection address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);

    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&dest_addr, sizeof(struct sockaddr)) < 0) {
        if (out) BIO_printf(out, "Error: Cannot connect to host %s on port %d.\n", hostname, port);
        closesocket(sockfd);
        return -1;
    }

    return sockfd;
}

// Perform HTTP GET request
std::string httpGet(const std::string& url, const std::string& outputFile) {
    URLComponents urlComponents = parseURL(url);
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return "";
    }
    
    // Resolve hostname
    struct hostent* host = gethostbyname(urlComponents.hostname.c_str());
    if (!host) {
        std::cerr << "Error resolving hostname: " << urlComponents.hostname << std::endl;
        closesocket(sock);
        return "";
    }
    
    // Connect to server
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(urlComponents.port);
    server.sin_addr = *((struct in_addr*)host->h_addr);
    
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        std::cerr << "Error connecting to server: " << strerror(errno) << std::endl;
        closesocket(sock);
        return "";
    }
    
    // Construct HTTP request
    std::string request = "GET " + urlComponents.path + " HTTP/1.1\r\n";
    request += "Host: " + urlComponents.hostname + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    
    // Send request
    if (send(sock, request.c_str(), request.length(), 0) < 0) {
        std::cerr << "Error sending HTTP request: " << strerror(errno) << std::endl;
        closesocket(sock);
        return "";
    }
    
    // Receive response
    std::stringstream responseStream;
    std::ofstream outFile;
    
    if (!outputFile.empty() && outputFile != "/dev/null") {
        outFile.open(outputFile, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "Error opening output file: " << outputFile << std::endl;
            closesocket(sock);
            return "";
        }
    }
    
    char buffer[4096];
    ssize_t bytesRead;
    
    while ((bytesRead = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        responseStream.write(buffer, bytesRead);
        if (outFile.is_open()) {
            outFile.write(buffer, bytesRead);
        }
    }
    
    if (bytesRead < 0) {
        std::cerr << "Error receiving HTTP response: " << strerror(errno) << std::endl;
    }
    
    closesocket(sock);
    
    return responseStream.str();
}

// Perform HTTPS GET request
std::string httpsGet(const std::string& url, const std::string& outputFile) {
    URLComponents urlComponents = parseURL(url);
    
    // Initialize OpenSSL
    initClientOpenSSL();
    
    // Create BIO for output
    BIO* outbio = BIO_new_fp(stdout, BIO_NOCLOSE);
    
    // Create SSL context
    const SSL_METHOD* method = TLS_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        BIO_printf(outbio, "Error creating SSL context\n");
        BIO_free_all(outbio);
        cleanupClientOpenSSL();
        return "";
    }
    
    // Initialize trust store
    initTrustStore(ctx);
    
    // Create SSL connection
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        BIO_printf(outbio, "Error creating SSL structure\n");
        SSL_CTX_free(ctx);
        BIO_free_all(outbio);
        cleanupClientOpenSSL();
        return "";
    }
    
    // Create socket and connect
    int server = createClientSocket(urlComponents.hostname.c_str(), outbio);
    if (server < 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        BIO_free_all(outbio);
        cleanupClientOpenSSL();
        return "";
    }
    
    // Attach SSL to socket
    SSL_set_fd(ssl, server);
    
    // Enable SNI
    SSL_set_tlsext_host_name(ssl, urlComponents.hostname.c_str());
    
    // Perform SSL handshake
    if (SSL_connect(ssl) != 1) {
        BIO_printf(outbio, "Error: SSL connection failed\n");
        SSL_free(ssl);
        closesocket(server);
        SSL_CTX_free(ctx);
        BIO_free_all(outbio);
        cleanupClientOpenSSL();
        return "";
    }
    
    // Verify certificate
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        BIO_printf(outbio, "Error: No certificate from server\n");
        SSL_free(ssl);
        closesocket(server);
        SSL_CTX_free(ctx);
        BIO_free_all(outbio);
        cleanupClientOpenSSL();
        return "";
    }
    
    // Construct and send HTTPS request
    std::string request = "GET " + urlComponents.path + " HTTP/1.1\r\n";
    request += "Host: " + urlComponents.hostname + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    
    int written = SSL_write(ssl, request.c_str(), request.length());
    if (written <= 0) {
        BIO_printf(outbio, "Error: Failed to write HTTPS request\n");
        X509_free(cert);
        SSL_free(ssl);
        closesocket(server);
        SSL_CTX_free(ctx);
        BIO_free_all(outbio);
        cleanupClientOpenSSL();
        return "";
    }
    
    // Set up output file if specified
    BIO* responseBio = nullptr;
    if (!outputFile.empty() && outputFile != "/dev/null") {
        responseBio = BIO_new_file(outputFile.c_str(), "w");
        if (!responseBio) {
            BIO_printf(outbio, "Error: Unable to open output file %s\n", outputFile.c_str());
        }
    }
    
    // Read response
    std::stringstream responseStream;
    char buffer[4096];
    int bytesRead;
    
    while ((bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        responseStream << buffer;
        
        if (responseBio) {
            BIO_write(responseBio, buffer, bytesRead);
        } else {
            BIO_write(outbio, buffer, bytesRead);
        }
    }
    
    // Clean up
    if (responseBio) {
        BIO_free(responseBio);
    }
    
    SSL_shutdown(ssl);
    X509_free(cert);
    SSL_free(ssl);
    closesocket(server);
    SSL_CTX_free(ctx);
    BIO_free_all(outbio);
    cleanupClientOpenSSL();
    
    return responseStream.str();
}

// HTTP/HTTPS client handler
void httpClientHandler(const NetOptions& options) {
    std::string response;
    
    // Determine if HTTP or HTTPS based on URL
    if (options.url.find("https://") == 0) {
        response = httpsGet(options.url, options.filename);
    } else {
        response = httpGet(options.url, options.filename);
    }
    
    // Output response if not saved to file
    if (options.filename == "/dev/null") {
        std::cout << response << std::endl;
    }
}