#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
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
using namespace std;
#define infinite 1000000000
class Net_opt {
    public:
        enum mode {SEND, RECV} mode;
        int stat = 500; // Default 500 ms
        string rhost = "localhost"; // Default localhost
        string lhost = "IN_ADDR_ANY"; // Default Late Binding
        int rport = 4180, lport = 4180; // Default 4180
        string proto = "UDP"; // Default UDP
        int pktsize = 1000; // Default 1000 bytes
        int pktrate = 1000; // Default 1000 bytes/second
        int pktnum = infinite; // Default 0 (infinite)
        int sbufsize = 0;
        int rbufsize = 0;
        string hostname = "localhost";
};

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
                    std::cout << "Invalid option" << std::endl;
                    return EXIT_FAILURE;
            }
        }
}  