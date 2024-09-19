#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

using namespace std;  
enum mode {SEND,RECEIVE,HOST};

void UPD_socket(int mode,char* address,int port){
    struct sockaddr_in UDP_Addr;
    memset (&UDP_Addr, 0, sizeof(UDP_Addr)); 
    UDP_Addr.sin_family = AF_INET;
    UDP_Addr.sin_port = htons(port); 
    UDP_Addr.sin_addr.s_addr = inet_addr(address);

    int UDP_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (UDP_Socket < 0) {
        cerr << "Failed to create UDP socket" << endl;
        return;
    }

    if (bind(UDP_Socket, (struct sockaddr*)&UDP_Addr, sizeof(UDP_Addr)) < 0) {
        cerr << "Failed to bind UDP socket" << endl;
        close(UDP_Socket);
        return;
    }

    cout << "UDP socket created and bound to " << address << ":" << port << endl;
    // Close the socket after use
    close(UDP_Socket);
}


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


int main(int argc, char *argv[]) {
    int opt;
    enum mode mode;
    while ((opt = getopt(argc, argv, "send:receive:host:")) != -1) {
        switch (opt) {
            case 'send':
                cout<<"send"<<endl;
                break;
            case 'receive':
                cout<<"receive"<<endl;
                break;
            case 'host':
                cout<<"host"<<endl;
                break;
            default:
                cout<<"Please Select Mode"<<endl;
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}