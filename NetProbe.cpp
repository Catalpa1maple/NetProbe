#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <getopt.h>

using namespace std;  
enum mode {SEND,RECV,HOST};

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

    static struct option long_options[] = {
        {"send", no_argument, 0, 's'},
        {"recv", no_argument, 0, 'r'},
        {"host", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

     while ((opt = getopt_long(argc, argv, "s:r:h:", long_options, NULL)) != -1) {
        switch (opt) {
            case 's':
                cout << "send" << endl;
                mode = SEND;
                break;
            case 'r':
                cout << "recv" << endl;
                mode = RECV;
                break;
            case 'h':
                cout << "host" << endl;
                mode = HOST;
                break;
            default:
                cout << "Please Select Mode" << endl;
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}