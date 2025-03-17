#ifndef HTTPS_SERVER_H
#define HTTPS_SERVER_H

#include <stdio.h>

#ifdef WIN32 // Windows
#	include <winsock2.h>
#	include <ws2tcpip.h>
#else // Assume Linux
#   include <sys/types.h>
#	include <sys/socket.h>
#   include <sys/ioctl.h>
#	include <sys/fcntl.h>
#   include <netdb.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <resolv.h>
#   include <string.h>
#   include <stdlib.h>
#   include <unistd.h>
#   include <errno.h>
#define SOCKET int
#define SOCKET_ERROR -1
#define WSAGetLastError() (errno)
#define closesocket(s) close(s)
#define ioctlsocket ioctl
#define WSAEWOULDBLOCK EWOULDBLOCK
  // There are other WSAExxxx constants which may also need to be #define'd to the Exxxx versions.
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
#define SD_BOTH SHUT_RDWR
#endif


#ifdef WIN32
#   include <wincrypt.h>
#   pragma comment(lib, "crypt32.lib")
#	 pragma comment(lib, "ws2_32.lib")
#endif

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

char hostname[8192];

// Initialize the Winsock library
void InitializeSockets()
{
#ifdef WIN32
   WSADATA wsaData;  // For external access.
   WORD wVersionRequested;

   wVersionRequested = MAKEWORD(2, 0);

   int err = WSAStartup(wVersionRequested, &wsaData);
   if (err != 0) {
      /* Tell the user that we couldn't find a useable */
      /* winsock.dll.                                  */
      printf("\nUnable to initialize winsock.dll!");
      exit(-1);
   }

   if (LOBYTE(wsaData.wVersion) != 2 ||
      HIBYTE(wsaData.wVersion) != 0) {
      /* Tell the user that we couldn't find a useable */
      /* winsock.dll.                                  */
      WSACleanup();
      printf("\nIncorrect winsock version, expecting 2.0 !");
      exit(-1);
   }
#endif // WIN32
}

void ShutdownSockets()
{
#ifdef WIN32
   WSACleanup();
#endif // WIN32
}

int create_socket(int port)
{
   int s;
   struct sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   addr.sin_addr.s_addr = htonl(INADDR_ANY);

   s = socket(AF_INET, SOCK_STREAM, 0);
   if (s < 0) {
      perror("Unable to create socket");
      printf("Error code = %i", WSAGetLastError());
      exit(EXIT_FAILURE);
   }

   if (::bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      perror("Unable to bind");
      exit(EXIT_FAILURE);
   }

   if (listen(s, 1) < 0) {
      perror("Unable to listen");
      exit(EXIT_FAILURE);
   }
   cout << "HTTPS Server is listening on port " << port << endl;

   return s;
}

void init_openssl()
{
   SSL_load_error_strings();
   OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
   EVP_cleanup();
}

SSL_CTX *create_context()
{
   const SSL_METHOD *method;
   SSL_CTX *ctx;

   //method = SSLv23_server_method();
   method = TLS_server_method();

   ctx = SSL_CTX_new(method);
   if (!ctx) {
      perror("Unable to create SSL context");
      ERR_print_errors_fp(stderr);
      exit(EXIT_FAILURE);
   }

   return ctx;
}

void configure_context(SSL_CTX *ctx)
{
   SSL_CTX_set_ecdh_auto(ctx, 1);

   /* Set the key and cert */
   if (SSL_CTX_use_certificate_file(ctx, "domain.crt", SSL_FILETYPE_PEM) <= 0) {
      ERR_print_errors_fp(stderr);
      exit(EXIT_FAILURE);
   }

   if (SSL_CTX_use_PrivateKey_file(ctx, "domain.key", SSL_FILETYPE_PEM) <= 0) {
      ERR_print_errors_fp(stderr);
      exit(EXIT_FAILURE);
   }
}

// int main(int argc, char **argv)
// {
//    int sock;
//    SSL_CTX *ctx;

//    InitializeSockets();

//    init_openssl();
//    ctx = create_context();

//    configure_context(ctx);

//    sock = create_socket(4180);

//    /* Handle connections */
//    while (1) {
//       struct sockaddr_in addr;
//       size_t len = sizeof(addr);
//       SSL *ssl;
//       const char reply[] = "This is a test message from the server!\n";

//       int client = accept(sock, (struct sockaddr*)&addr, &len);
//       if (client < 0) {
//          perror("Unable to accept");
//          exit(EXIT_FAILURE);
//       }

//       ssl = SSL_new(ctx);      // Create a new SSL connection
//       SSL_set_fd(ssl, client); // Associate the socket handle with it

//       if (SSL_accept(ssl) <= 0) { // Perform SSL/TLS handshake with peer
//          ERR_print_errors_fp(stderr);
//       }
//       else {
         
//          char buff[2048];
//          while (SSL_read(ssl, buff, sizeof(buff)) > 0)
//             ; // Clear the recv buffer or else SSL_shutdown() will fail.
         
//          int stlen = strlen(reply);
//          if (SSL_write(ssl, reply, stlen) != stlen) { // Send the message over SSL/TLS
//             printf("\n SSL_write failed!\n"); // Note partial send should be handled.
//          }

//          int bShutdownComplete = SSL_shutdown(ssl);

//          int countdown = 30; // Allow max 30 retries for peer to properly shutdown the SSL connection
//          while ((bShutdownComplete != 1) && (countdown--)) {
//             Sleep(100);
//             bShutdownComplete = SSL_shutdown(ssl);
//             if (bShutdownComplete == -1) {
//                printf("SSL_shutdown() error = %i\n", SSL_get_error(ssl, bShutdownComplete));
//                break;
//             }
//          }
//          if ((bShutdownComplete != -1) && (countdown))
//             printf("SSL shutdown sequence completes.\n");
//          else
//             printf("SSL shutdown sequence timeout.\n");
//       }

//       SSL_free(ssl);       // Free the SSL connection
//       closesocket(client); // Close the socket
//    }

//    closesocket(sock);
//    SSL_CTX_free(ctx);
//    cleanup_openssl();
//    ShutdownSockets();
// }
#endif // HTTPS_SERVER_H