#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Socket_Protocol.h"

void TCP_socket(int mode,int stat, const std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize){
    struct sockaddr_in TCP_Addr;
    memset(&TCP_Addr, 0, sizeof(TCP_Addr));
    TCP_Addr.sin_family = AF_INET;
    TCP_Addr.sin_port = htons(port);
    TCP_Addr.sin_addr.s_addr = inet_addr(host.c_str());

    int TCP_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (TCP_Socket < 0) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return;
    }
    std::cout << "TCP socket created" << std::endl;

    if(mode == RECV){ //As RECV mode be consider as Server
    if (bind(TCP_Socket, (struct sockaddr*)&TCP_Addr, sizeof(TCP_Addr)) < 0) {
        std::cerr << "Failed to bind TCP socket" << std::endl;
        close(TCP_Socket);
        return;
    }

    if (listen(TCP_Socket, 5) < 0) {
        std::cerr << "Failed to listen on TCP socket" << std::endl;
        close(TCP_Socket);
        return;
    }
    std::cout << "TCP socket created, bound to port " << port << ", and listening for connections" << std::endl;


    if (accept(TCP_Socket, NULL, NULL) < 0) {
        std::cerr << "Failed to accept connection" << std::endl;
        close(TCP_Socket);
        return;
    }
    std::cout << "Connection accepted" << std::endl;
   

    recvfrom(TCP_Socket, NULL, 0, 0, NULL, NULL);
    }

    
    else if(mode == SEND){ //As SEND mode be consider as Client


    }

    // Close the socket after use
    close(TCP_Socket);
}


void UDP_socket(int mode,int stat, const std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize){
    struct sockaddr_in UDP_Addr;
    memset (&UDP_Addr, 0, sizeof(UDP_Addr)); 
    UDP_Addr.sin_family = AF_INET;
    UDP_Addr.sin_port = htons(port); 
    UDP_Addr.sin_addr.s_addr = inet_addr(host.c_str());

    int UDP_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (UDP_Socket < 0) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return;
    }
    std::cout << "UDP socket created" << std::endl;

    if(mode == RECV) //As RECV mode be consider as Server
    if (bind(UDP_Socket, (struct sockaddr*)&UDP_Addr, sizeof(UDP_Addr)) < 0) {
         std::cerr << "Failed to bind UDP socket" <<  std::endl;
        close(UDP_Socket);
        return;
    }

     std::cout << "UDP socket created and bound to " << host << ":" << port <<  std::endl;
    // Close the socket after use
    close(UDP_Socket);
}