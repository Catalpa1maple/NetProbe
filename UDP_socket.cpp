#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "UDP_socket.h"

void UPD_socket(int mode,char* address,int port){
    struct sockaddr_in UDP_Addr;
    memset (&UDP_Addr, 0, sizeof(UDP_Addr)); 
    UDP_Addr.sin_family = AF_INET;
    UDP_Addr.sin_port = htons(port); 
    UDP_Addr.sin_addr.s_addr = inet_addr(address);

    int UDP_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (UDP_Socket < 0) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return;
    }

    if (bind(UDP_Socket, (struct sockaddr*)&UDP_Addr, sizeof(UDP_Addr)) < 0) {
         std::cerr << "Failed to bind UDP socket" <<  std::endl;
        close(UDP_Socket);
        return;
    }

     std::cout << "UDP socket created and bound to " << address << ":" << port <<  std::endl;
    // Close the socket after use
    close(UDP_Socket);
}