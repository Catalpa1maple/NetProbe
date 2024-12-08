#ifndef HTTPS_CLIENT_H
#define HTTPS_CLIENT_H

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
#endif


#ifdef WIN32
#   include <wincrypt.h>
#   pragma comment (lib, "crypt32.lib")
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


int InitTrustStore(SSL_CTX *ctx)
{
#ifdef WIN32
	HCERTSTORE       hCertStore;
	PCCERT_CONTEXT   pCertContext = NULL;
	char pszNameString[256];
	void*            pvData;
	DWORD            cbData;
	DWORD            dwPropId = 0;
	int              ret;

	X509 *x;
	X509_STORE *xStore;

	if (hCertStore = CertOpenSystemStore(0, L"ROOT")) // Note the use of L to specify wide char type (wchar_t)
	{
		fprintf(stderr, "Populating the cert store from Windows ROOT store ");
	}
	else
	{
		// If the store was not opened, exit to an error routine.
		fprintf(stderr, "failed to open the ROOT store.\n");
		return -1;
	}

	xStore = SSL_CTX_get_cert_store(ctx);
	if (!xStore) {
		fprintf(stderr, "SSL_CTX_get_cert_store(ctx) failed.\n");
		return -1;
	}

	//-------------------------------------------------------------------
	// Use CertEnumCertificatesInStore to get the certificates
	// from the open store. pCertContext must be reset to
	// NULL to retrieve the first certificate in the store.

	// pCertContext = NULL;

	while (pCertContext = CertEnumCertificatesInStore(
		hCertStore,
		pCertContext))
	{
		// printf("."); Sleep(100);
		x = d2i_X509(NULL, &pCertContext->pbCertEncoded, pCertContext->cbCertEncoded);
		if (!x) {
			fprintf(stderr, "d2i_X509 failed.\n");
			return -1;
		}
		int retval = X509_STORE_add_cert(xStore, x);
//		if (retval == 1)
//			printf("Added cert from Windows ...\n");
		X509_free(x);
	}
	printf("... completed.\n");
	return 0;
#else
//	if (!SSL_CTX_set_default_verify_paths(ctx)) {
	if (!SSL_CTX_load_verify_locations(ctx, 0, "/etc/ssl/certs")) {
		fprintf(stderr, "Unable to set default verify paths.\n");
		return -1;
    } else
	   return 0;
#endif
}

/* ---------------------------------------------------------- *
 * First we need to make a standard TCP socket connection.    *
 * create_socket() creates a socket & TCP-connects to server. *
 * ---------------------------------------------------------- */
int create_socket(char[], BIO *);


// int main(int argc, char **argv) {

// 	char           dest_url[8192];
// 	BIO               *outbio = NULL;
// 	X509                *cert = NULL;
// 	X509_NAME       *certname = NULL;
// 	const SSL_METHOD *method;
// 	SSL_CTX *ctx;
// 	SSL *ssl;
// 	int server = 0;
// 	int ret, i;
// 	char *ptr = NULL;

// 	if (argc == 1) strcpy(dest_url, "www.google.com");
// 	else strncpy(dest_url, argv[1], 8192);

// 	InitializeSockets();

// 	/* ---------------------------------------------------------- *
// 	* These function calls initialize openssl for correct work.  *
// 	* ---------------------------------------------------------- */
// 	OpenSSL_add_all_algorithms();
// 	ERR_load_BIO_strings();
// 	ERR_load_crypto_strings();
// 	SSL_load_error_strings();

// 	/* ---------------------------------------------------------- *
// 	* Create the Input/Output BIO's.                             *
// 	* ---------------------------------------------------------- */
// 	outbio = BIO_new_fp(stdout, BIO_NOCLOSE);

// 	/* ---------------------------------------------------------- *
// 	* initialize SSL library and register algorithms             *
// 	* ---------------------------------------------------------- */
// 	if (SSL_library_init() < 0)
// 		BIO_printf(outbio, "Could not initialize the OpenSSL library !\n");

// 	/* ---------------------------------------------------------- *
// 	* Set SSLv2 client hello, also announce SSLv3 and TLSv1      *
// 	* ---------------------------------------------------------- */
// 	method = TLS_method();

// 	/* ---------------------------------------------------------- *
// 	* Try to create a new SSL context                            *
// 	* ---------------------------------------------------------- */
// 	if ((ctx = SSL_CTX_new(method)) == NULL)
// 		BIO_printf(outbio, "Unable to create a new SSL context structure.\n");

// 	/* ---------------------------------------------------------- *
// 	* Disabling SSLv2/SSLv3 will leave TLS for negotiation    *
// 	* ---------------------------------------------------------- */
// //	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

// 	InitTrustStore(ctx);

// 	/* ---------------------------------------------------------- *
// 	* Load the root cert which signed the server CA              *
// 	* ---------------------------------------------------------- */
// 	ret = SSL_CTX_load_verify_file(ctx, "rootCA.crt");
// 	if (ret == 1)
// 		printf("rootCA.crt added to cert store.\n");

// 	/* ---------------------------------------------------------- *
// 	* Create new SSL connection state object                     *
// 	* ---------------------------------------------------------- */
// 	ssl = SSL_new(ctx);

// 	/* ---------------------------------------------------------- *
// 	* Make the underlying TCP socket connection                  *
// 	* ---------------------------------------------------------- */
// 	server = create_socket(dest_url, outbio);
// 	if (server != 0)
// 		BIO_printf(outbio, "Successfully made the TCP connection to: %s.\n", dest_url);

// 	/* ---------------------------------------------------------- *
// 	* Attach the SSL session to the socket descriptor            *
// 	* ---------------------------------------------------------- */
// 	SSL_set_fd(ssl, server);

// 	/* ---------------------------------------------------------- *
// 	* Enable Server-Name-Indication                              *
// 	* ---------------------------------------------------------- */
// 	SSL_set_tlsext_host_name(ssl, dest_url);

// 	/* ---------------------------------------------------------- *
// 	* Try to SSL-connect here, returns 1 for success             *
// 	* ---------------------------------------------------------- */
// 	if (SSL_connect(ssl) != 1)
// 		BIO_printf(outbio, "Error: Could not build a SSL session to: %s.\n", dest_url);
// 	else
// 		BIO_printf(outbio, "Successfully enabled SSL/TLS session to: %s.\n", dest_url);

// 	/* ---------------------------------------------------------- *
// 	* Get the remote certificate into the X509 structure         *
// 	* ---------------------------------------------------------- */
// 	cert = SSL_get_peer_certificate(ssl);
// 	if (cert == NULL)
// 		BIO_printf(outbio, "Error: Could not get a certificate from: %s.\n", dest_url);
// 	else
// 		BIO_printf(outbio, "Retrieved the server's certificate from: %s.\n", dest_url);

// 	/* ---------------------------------------------------------- *
// 	* extract various certificate information                    *
// 	* -----------------------------------------------------------*/
// 	certname = X509_NAME_new();
// 	certname = X509_get_subject_name(cert);

// 	/* ---------------------------------------------------------- *
// 	* display the cert subject here                              *
// 	* -----------------------------------------------------------*/
// 	BIO_printf(outbio, "Displaying the certificate subject data:\n");
// 	X509_NAME_print_ex(outbio, certname, 0, 0);
// 	BIO_printf(outbio, "\n");

// 	/* ---------------------------------------------------------- *
// 	* Validate the remote certificate is from a trusted root     *
// 	* ---------------------------------------------------------- */
// 	ret = SSL_get_verify_result(ssl);
// 	if (ret != X509_V_OK)
// 		BIO_printf(outbio, "Warning: Validation failed (code %i) for certificate from: %s.\n", ret, dest_url);
// 	else
// 		BIO_printf(outbio, "Successfully validated the server's certificate from: %s.\n", dest_url);

// 	/* ---------------------------------------------------------- *
// 	* Perform hostname validation                                 *
// 	* ---------------------------------------------------------- */
// 	ret = X509_check_host(cert, hostname, strlen(hostname), 0, &ptr);
// 	if (ret == 1) {
// 		BIO_printf(outbio, "Successfully validated the server's hostname matched to: %s.\n", ptr);
// 		OPENSSL_free(ptr); ptr = NULL;
// 	}
// 	else if (ret == 0)
// 		BIO_printf(outbio, "Server's hostname validation failed: %s.\n", hostname);
// 	else
// 		BIO_printf(outbio, "Hostname validation internal error: %s.\n", hostname);

// 	/* ---------------------------------------------------------- *
// 	* Send an HTTP GET request                                   *
// 	* ---------------------------------------------------------- */

// 	char request[8192];
// 	sprintf(request,
// 		"GET / HTTP/1.1\r\n"
// 		"Host: %s\r\n"
// 		"Accept: image/gif, image/jpeg, */*\r\n"
// 		"Accept-Language: en - us\r\n"
// //		"Accept-Encoding: gzip, deflate\r\n"
// 		"User-Agent: Mozilla / 4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n\r\n" // This is required by www.cuhk.edu.hk.
// 		, hostname);
// 	/*
// 	sprintf(request,
// 		   "GET / HTTP/1.1\r\n"
// 		   "Host: %s\r\n"
// 		   "Connection: close\r\n\r\n", hostname);
// 	*/
// 	printf(request);
// 	int len = strlen(request);
// 	ret = SSL_write(ssl, request, len);
// 	if (ret < len)
// 		printf("Incomplete SSL_write() not handled!\n");
	
// 	int bShutdownComplete = SSL_shutdown(ssl);

// 	BIO_printf(outbio, "------------------ RESPONSE RECEIVED ---------------------\n");
// 	do
// 	{
// 		char buff[1536];
// 		len = SSL_read(ssl, buff, sizeof(buff));

// 		printf("len = %i\n", len);

// 		if (len > 0)
// 			BIO_write(outbio, buff, len);
// 			//fwrite(buff, len, 1, stdout);
// 		else {
// 			int err = SSL_get_error(ssl, len);
// 			printf("SSL_get_error = %i\n", err);
// 			printf("WSAGetLastError = %i\n", WSAGetLastError());
// 		}

// 	} while (len > 0);
// 	BIO_printf(outbio, "\n----------------------------------------------------------\n");

// 	int countdown = 30; // Allow max 30 retries for peer to properly shutdown the SSL connection
// 	while ((bShutdownComplete != 1) && (countdown--)) {
// 		bShutdownComplete = SSL_shutdown(ssl);
// 		if (bShutdownComplete == -1) {
// 			printf("SSL_shutdown() error = %i\n", SSL_get_error(ssl, bShutdownComplete));
// 			break;
// 		}
// 		Sleep(100);
// 	}
// 	if ((bShutdownComplete != -1) && (countdown))
// 		printf("SSL shutdown sequence completes.\n");
// 	else
// 		printf("SSL shutdown sequence timeout.\n");

// 	/* ---------------------------------------------------------- *
// 	* Free the structures we don't need anymore                  *
// 	* -----------------------------------------------------------*/
// 	SSL_free(ssl);
// 	closesocket(server);
// 	X509_free(cert);
// 	SSL_CTX_free(ctx);
// 	BIO_printf(outbio, "\nFinished SSL/TLS connection with server: %s.\n", dest_url);

// 	ShutdownSockets();

// 	return(0);
// }


/* ---------------------------------------------------------- *
* create_socket() creates the socket & TCP-connect to server *
* ---------------------------------------------------------- */
int create_socket(char url_str[], BIO *out) {
	int sockfd;
	char    portnum[6] = "443";
	char      proto[6] = "";
	char      *tmp_ptr = NULL;
	int           port;
	struct hostent *host;
	struct sockaddr_in dest_addr;

	/* ---------------------------------------------------------- *
	* Remove the final / from url_str, if there is one           *
	* ---------------------------------------------------------- */
	if (url_str[strlen(url_str)] == '/')
		url_str[strlen(url_str)] = '\0';

	/* ---------------------------------------------------------- *
	* the first : ends the protocol string, i.e. http            *
	* ---------------------------------------------------------- */
	if (strstr(url_str, "http:") || strstr(url_str, "https:"))
		strncpy(proto, url_str, (strchr(url_str, ':') - url_str));
	else
		strcpy(proto, "https");

	/* ---------------------------------------------------------- *
	* the hostname starts after the "://" part                   *
	* ---------------------------------------------------------- */
	if (strstr(url_str, "://"))
		strncpy(hostname, strstr(url_str, "://") + 3, sizeof(hostname));
	else
		strncpy(hostname, url_str, sizeof(hostname));

	/* ---------------------------------------------------------- *
	* if the hostname contains a colon :, we got a port number   *
	* ---------------------------------------------------------- */
	if (strchr(hostname, ':')) {
		tmp_ptr = strchr(hostname, ':');
		/* the last : starts the port number, if avail, i.e. 8443 */
		strncpy(portnum, tmp_ptr + 1, sizeof(portnum));
		*tmp_ptr = '\0';
	}

	port = atoi(portnum);

	if ((host = gethostbyname(hostname)) == NULL) {
		BIO_printf(out, "Error: Cannot resolve hostname %s.\n", hostname);
		abort();
	}

	/* ---------------------------------------------------------- *
	* create the basic TCP socket                                *
	* ---------------------------------------------------------- */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(port);
	dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);
    // cout << hostname << " and " << port << endl;
	/* ---------------------------------------------------------- *
	* Zeroing the rest of the struct                             *
	* ---------------------------------------------------------- */
	memset(&(dest_addr.sin_zero), '\0', 8);

	tmp_ptr = inet_ntoa(dest_addr.sin_addr);

	/* ---------------------------------------------------------- *
	* Try to make the host connect here                          *
	* ---------------------------------------------------------- */
	if (connect(sockfd, (struct sockaddr *) &dest_addr,
		sizeof(struct sockaddr)) == -1) {
		BIO_printf(out, "Error: Cannot connect to host %s [%s] on port %d.\n",
			hostname, tmp_ptr, port);
		exit(-1);
	}

	return sockfd;
}


#endif 