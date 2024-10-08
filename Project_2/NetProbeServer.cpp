#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "NetProbe.h"
#include "tinythread.h"
using namespace tthread;

int main(int argc, char *argv[]){
    Net_opt net_opt;
    static struct option options[] = {
            {"lhost",    required_argument, 0, 'a'},
            {"lport",    required_argument, 0, 'b'},
            {"sbufsize", required_argument, 0, 'c'},
            {"rbufsize", required_argument, 0, 'd'},
            {0, 0, 0, 0}};

    int option_index = 0;
    int opt;
    while ((opt = getopt_long_only(argc, argv, "", options, &option_index)) != -1) {
            switch (opt) { 
                case 'a':
                    std::cout << "lhost: " << optarg << std::endl;
                    net_opt.lhost = optarg;
                    break;
                case 'b':
                    std::cout << "lport: " << optarg << std::endl;
                    net_opt.lport = atoi(optarg);
                    break;
                case 'c':
                    std::cout << "sbufsize: " << optarg << std::endl;
                    net_opt.sbufsize = atoi(optarg);
                    break;
                case 'd':
                    std::cout << "rbufsize: " << optarg << std::endl;
                    net_opt.rbufsize = atoi(optarg);
                    break;
                default:
                    std::cout << "Invalid option" << std::endl;
                    return EXIT_FAILURE;   
            }
    }
    // init_server(net_opt);
}



void init_server(class Net_opt net_opt){
        struct sockaddr_in TCP_Serv_Addr,UDP_Serv_Addr;
        memset(&TCP_Serv_Addr, 0, sizeof(TCP_Serv_Addr));
        memset(&UDP_Serv_Addr, 0, sizeof(UDP_Serv_Addr));
        TCP_Serv_Addr.sin_family = AF_INET;
        TCP_Serv_Addr.sin_port = htons(net_opt.lport);
        UDP_Serv_Addr.sin_family = AF_INET;
        UDP_Serv_Addr.sin_port = htons(net_opt.lport);

        int TCP_Socket = socket(AF_INET, SOCK_STREAM, 0);
        if (TCP_Socket < 0) {
            std::cerr << "Failed to create TCP socket" << std::endl;
            return;
        }
        int UDP_Socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (UDP_Socket < 0) {
            std::cerr << "Failed to create UDP socket" << std::endl;
            return;
        }


        if (bind(TCP_Socket, (struct sockaddr*)&TCP_Serv_Addr, sizeof(TCP_Serv_Addr)) < 0) {
            std::cerr << "Failed to bind TCP socket" << std::endl;
            close(TCP_Socket);
            return;
        }

        if (listen(TCP_Socket, 5) < 0) {
            std::cerr << "Failed to listen on TCP socket" << std::endl;
            close(TCP_Socket);
            return;
        }
}