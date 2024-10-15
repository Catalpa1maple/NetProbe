#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define WSAWOULDBLOCK EWOULDBLOCK

//Macros to translate winsock API to Linux API
#define WSAGetLastError() (errno)
#define closesocket(s) close(s)
#define ioctlsocket ioctl
#define Sleep(x) usleep(x*1000) /Sleep() is in ms

#endif
#include <iostream>
#include <getopt.h>
#include <string>
#include "NetProbe.h"
using namespace std;


int* generate_msg(class Net_opt net_opt){
    int mode,proto,pktsate;
    if (net_opt.proto == "TCP")proto = 0;else if (net_opt.proto == "UDP")proto = 1;
    if (net_opt.mode == Net_opt::SEND)mode = 0;else if (net_opt.mode == Net_opt::RECV)mode = 1; 
    if (net_opt.pktnum == infinite)net_opt.pktnum = 0;
    int* msg = new int[6];
    msg[0] = mode;
    msg[1] = net_opt.stat;
    msg[2] = proto;
    msg[3] = net_opt.pktsize;
    msg[4] = net_opt.pktrate;
    msg[5] = net_opt.pktnum;
        return msg;
}

void init_client(class Net_opt net_opt){
    #ifdef WIN32
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;
        wVersionRequested = MAKEWORD(2, 2);
        err = WSAStartup(wVersionRequested, &wsaData);
        if (err != 0) {
            std::cerr << "Failed to initialize Winsock" << std::endl;
            return;
        }
    #endif //Setup Winsock

    int* msg_int = generate_msg(net_opt);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(net_opt.rport);
    if( net_opt.rhost == "localhost") { net_opt.rhost = "127.0.0.1"; }
    serv_addr.sin_addr.s_addr = inet_addr(net_opt.rhost.c_str());
    
    char buf[1000000]; // 1MB buffer
    if(net_opt.proto == "TCP"){
        SOCKET TCP_client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (TCP_client_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create client socket. Error: " << WSAGetLastError() << std::endl;
            return;
        }
        if (connect(TCP_client_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to connect to server. Error: " << WSAGetLastError() << std::endl;
            closesocket(TCP_client_socket);
            return;
        }

        if( send(TCP_client_socket, msg_int, sizeof(msg_int), 0) == INVALID_SOCKET){
            std::cerr << "Failed to send data. Error: " << WSAGetLastError() << std::endl;
            closesocket(TCP_client_socket);
            return;
        }
        cout << msg_int[3] << " " << msg_int[4] << " " << msg_int[5] << endl;
    }


    // else if(net_opt.proto == "UDP"){
    //     SOCKET UDP_client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    //     if (UDP_client_socket == INVALID_SOCKET) {
    //         std::cerr << "Failed to create client socket. Error: " << WSAGetLastError() << std::endl;
    //         return;
    //     }
    
    //     if (sendto(UDP_client_socket, "SEND", sizeof(char*("SEND")), 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
    //         std::cerr << "Failed to connect to server. Error: " << WSAGetLastError() << std::endl;
    //         closesocket(UDP_client_socket);
    //         return;
    //     }
    
    // }

}



int main(int argc, char *argv[]){
    Net_opt net_opt;
    static struct option options[] = {
            {"send",     no_argument, 0, '1'},
            {"recv",     no_argument, 0, '2'},
            {"stat",     required_argument, 0, 'a'},
            {"rhost",    required_argument, 0, 'b'},
            {"rport",    required_argument, 0, 'c'},
            {"proto",    required_argument, 0, 'd'},
            {"pktsize",  required_argument, 0, 'e'},
            {"pktrate",  required_argument, 0, 'f'},
            {"pktnum",   required_argument, 0, 'g'},
            {"sbufsize", required_argument, 0, 'h'},
            {"lhost",    required_argument, 0, 'i'},
            {"lport",    required_argument, 0, 'j'},
            {"rbufsize", required_argument, 0, 'k'},
            {0, 0, 0, 0}
            };
    int option_index = 0;
    int opt;
    while ((opt = getopt_long_only(argc, argv, "", options, &option_index)) != -1) {
            switch (opt) { 
                case '1':
                    net_opt.mode = Net_opt::SEND;
                    break;
                case '2':
                    net_opt.mode = Net_opt::RECV;
                    break;
                case 'a':
                    net_opt.stat = atoi(optarg);
                    break;
                case 'b':
                    net_opt.rhost = optarg;
                    {
                        struct hostent *he = gethostbyname(net_opt.rhost.c_str());
                        if (he == nullptr) {
                            std::cerr << "Failed to resolve hostname: " << net_opt.rhost << std::endl;
                        } else {
                            struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
                            for (int i = 0; addr_list[i] != nullptr; i++) {
                                std::cout << "IP Address: " << inet_ntoa(*addr_list[i]) << std::endl;
                            }
                        }
                    }
                    break;
                case 'c':
                    net_opt.rport = atoi(optarg);
                    break;
                case 'd':
                    net_opt.proto = optarg;
                    break;
                case 'e':
                    net_opt.pktsize = atoi(optarg);
                    break;
                case 'f':
                    net_opt.pktrate = atoi(optarg);
                    break;
                case 'g':
                    net_opt.pktnum = atoi(optarg);
                    break;
                case 'h':
                    net_opt.sbufsize = atoi(optarg);
                    break;
                case 'i':
                    net_opt.lhost = optarg;
                    break;
                case 'j':
                    net_opt.lport = atoi(optarg);
                    break;
                case 'k':   
                    net_opt.rbufsize = atoi(optarg);
                    break;
                default:
                    std::cout << "Error : Invalid option" << std::endl;
                    return EXIT_FAILURE;
            }
        }
        if (net_opt.mode < 0){
            std::cerr << "Error : Mode not specified" << std::endl;
            return EXIT_FAILURE;
        }
        init_client(net_opt);
}


