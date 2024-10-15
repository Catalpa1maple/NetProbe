#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <thread>
#include "NetProbe.h"
using namespace std;


void handle_msg(int msg[]){
    cout << "mode: ";
    if (msg[0] == 0) cout << "send" << endl;
    else if (msg[0] == 1) cout << "recv" << endl;
    cout << "stat: " << msg[1] << endl;
    cout << "proto: ";
    if (msg[2] == 0) cout << "TCP" << endl;
    else if (msg[2] == 1) cout << "UDP" << endl;
    cout << "pktsize: " << msg[3] << endl;
    cout << "pktrate: " << msg[4] << endl;
    cout << "pktnum: " << msg[5] << endl;
}

void TCP_thread(ClientInfo* client_info){
    if(client_info->msg[0] == 0){    //Send Mode
        for(int i=0;i<client_info->msg[5];i++){
            int buf[client_info->msg[3]/4];
            memset(buf, i, sizeof(buf));
            send(client_info->socket, buf, sizeof(buf), 0);
        }
    }
    else if(client_info->msg[0] == 1){   //Recv Mode

    }

}

void init_server(class Net_opt net_opt){

        struct sockaddr_in TCP_Serv_Addr,UDP_Serv_Addr;
        memset(&TCP_Serv_Addr, 0, sizeof(TCP_Serv_Addr));
        memset(&UDP_Serv_Addr, 0, sizeof(UDP_Serv_Addr));

        TCP_Serv_Addr.sin_family = AF_INET;
        TCP_Serv_Addr.sin_port = htons(net_opt.lport);
        if( net_opt.lhost == "IN_ADDR_ANY") {
            TCP_Serv_Addr.sin_addr.s_addr = INADDR_ANY;
        }else{
            TCP_Serv_Addr.sin_addr.s_addr = inet_addr(net_opt.lhost.c_str());
        }

        UDP_Serv_Addr.sin_family = AF_INET;
        UDP_Serv_Addr.sin_port = htons(net_opt.lport);
        UDP_Serv_Addr.sin_addr.s_addr = inet_addr(net_opt.lhost.c_str());

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


        if (::bind(TCP_Socket, (struct sockaddr*)&TCP_Serv_Addr, sizeof(TCP_Serv_Addr)) < 0) {
            std::cerr << "Failed to bind TCP socket" << std::endl;
            close(TCP_Socket);
            return;
        }

        if (listen(TCP_Socket, 5) < 0) {
            std::cerr << "Failed to listen on TCP socket" << std::endl;
            close(TCP_Socket);
            return;
        }

    cout << "Server is listening on port " << net_opt.lport << endl;

    // thrd_t t;
    // if (thrd_create(&t, UDP_thread, (void*)0) != thrd_suceess){
    //     cerr << "Error creating thread" << endl;
    //     return EXIT_FAILURE;
    // }// create a thread to handle UDP connection

    while (true) //Main thread to handle TCP connection
    {
        struct sockaddr_in TCP_Clnt_Addr;
        memset(&TCP_Clnt_Addr, 0, sizeof(TCP_Clnt_Addr));
        socklen_t TCP_Clnt_Addr_len = sizeof(TCP_Clnt_Addr);
        int TCP_Client_Socket = accept(TCP_Socket, (struct sockaddr*)&TCP_Clnt_Addr, &TCP_Clnt_Addr_len);
        if(TCP_Client_Socket < 0){
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        ClientInfo* client_info = new ClientInfo;
        client_info->socket = TCP_Client_Socket;
        client_info->addr = TCP_Clnt_Addr;  
        if (recv(TCP_Client_Socket, &client_info->msg, sizeof(client_info->msg), 0) < 0) {
            std::cerr << "Failed to receive data" << std::endl;
            close(TCP_Client_Socket);
            delete client_info;
            return;
        }

        handle_msg(client_info->msg);
        if(client_info->msg[2] == 0){        // Create TCP thread
            try {
                thread tcpThread(TCP_thread, client_info);
                tcpThread.detach();
            } catch (const system_error& e) {
                cerr << "Error creating thread: " << e.what() << endl;
                close(client_info->socket);
                delete client_info;
        }}
        else if((client_info->msg[2]) == 1){   // Create UDP thread
            puts("UPD");
        }
    }

}


// int init_connection(void* arg){
//     ClientInfo* client_info = (ClientInfo*)arg;
//     int client_socket = client_info->socket;
//     struct sockaddr_in client_addr = client_info->addr;
    
//     char msg[1000];
//     if (recv(client_socket, &msg, 10, 0) < 0) {
//         std::cerr << "Failed to receive data" << std::endl;
//         closesocket(client_socket);
//         delete client_info;
//         return 0;
//     }

//     istringstream iss(msg);
//     int stat, rport, pktsize, pktrate, pktnum, sbufsize, rbufsize;
//     string mode, rhost, lhost, proto;
//     iss >> mode >>stat >> rhost >> rport >> proto >> pktsize >> pktrate >> pktnum >> sbufsize >> lhost >> rbufsize;
// }


// int TCP_thread(void* arg) {
    

//     char buf[1000000]; // 1MB buffer
//     if (strcmp(mode,"send") == 0){
//         if (send(client_socket, &buf, sizeof(msg), 0) < 0) {
//             std::cerr << "Failed to send data" << std::endl;
//             closesocket(client_socket);
//             delete client_info;
//             return 0;
//         }
//     }else if (strcmp(msg, "RECV") == 0){
    
//     }

//     closesocket(client_socket);
//     delete client_info;
//     return 0;
// }

// int UDP_thread(struct sockaddr_in TCP_Clnt_Addr){
//     while (true)
//     {
//         if (recvfrom(UDP_client_socket, &buf, 1000000, 0, nullptr, nullptr) == SOCKET_ERROR) {
//             std::cerr << "Failed to connect to server. Error: " << WSAGetLastError() << std::endl;
//             closesocket(UDP_client_socket);
//             return;
//         }
//     }

//     return 0;
// }

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
    init_server(net_opt);
}



