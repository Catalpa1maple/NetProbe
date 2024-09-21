#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "TCP_socket.h"

void TCP_socket(int mode,char* address,int port) {
    struct sockaddr_in TCP_Addr;
    memset(&TCP_Addr, 0, sizeof(TCP_Addr));
    TCP_Addr.sin_family = AF_INET;
    TCP_Addr.sin_port = htons(port);
    TCP_Addr.sin_addr.s_addr = inet_addr(address);

    int TCP_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (TCP_Socket < 0) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return;
    }

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
    // Close the socket after use
    close(TCP_Socket);
}