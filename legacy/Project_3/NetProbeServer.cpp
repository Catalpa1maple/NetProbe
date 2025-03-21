#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <iomanip>
#include <errno.h>
#include <iostream>
#include <getopt.h>
#include <string>
#include <thread>
#include <sys/time.h>
#include "Threadspool.h"
using namespace std;

//Declare some global variables
int TCP_client = 0, UDP_client = 0;
string tcpcca;


void handle_msg(ClientInfo *client_info){
    // cout << "mode: ";
    // if (client_info->msg[0] == 0) cout << "send" << endl;
    // else if (client_info->msg[0] == 1) cout << "recv" << endl;
    // cout << "stat: " << client_info->msg[1] << endl;
    // cout << "proto: ";
    // if (client_info->msg[2] == 0) cout << "TCP" << endl;
    // else if (client_info->msg[2] == 1) cout << "UDP" << endl;
    // cout << "pktsize: " << client_info->msg[3] << endl;
    // cout << "pktrate: " << client_info->msg[4] << endl;
    // cout << "pktnum: " << client_info->msg[5] << endl;
    if (client_info->msg[5] == 0)client_info->msg[5] = infinite;
}

void TCP_thread(ClientInfo* client_info){
    /*
        Set up TCP server and
        Pack the info of socket to send to client
        Send back a info of port to client
    */
    struct sockaddr_in TCP_Trd;
    memset(&TCP_Trd, 0, sizeof(TCP_Trd));
    TCP_Trd.sin_family = AF_INET;
    TCP_Trd.sin_addr.s_addr = client_info->addr.sin_addr.s_addr; //Client IP

    int TCP_Trd_Socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;

    // if(!tcpcca.empty()){
    //     if(setsockopt(TCP_Trd_Socket, IPPROTO_TCP, TCP_CONGESTION, tcpcca.c_str(), tcpcca.length()) < 0) {
    //     cerr << "Failed to set TCP congestion control algorithm: " << strerror(errno) << endl;
    // }}
    //Noted that MAC OS does not support TCP_CONGESTION

    setsockopt(TCP_Trd_Socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    ::bind(TCP_Trd_Socket, (struct sockaddr*)&TCP_Trd, sizeof(TCP_Trd));
    listen(TCP_Trd_Socket, 5);
    socklen_t len = sizeof(TCP_Trd);
    getsockname(TCP_Trd_Socket, (struct sockaddr*)&TCP_Trd, &len); //Get the port
    int port = ntohs(TCP_Trd.sin_port);

    if (send((client_info->socket), &port, sizeof(port), 0) < 0) {
        std::cerr << "Failed to send data" << strerror(errno)<< std::endl;
        close(TCP_Trd_Socket);
        close(client_info->socket);
        delete client_info;
        return;
    }
    int new_socket = accept(TCP_Trd_Socket, (struct sockaddr*)&TCP_Trd, (socklen_t*)&TCP_Trd);
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY,(char *)((socklen_t*)&TCP_Trd), sizeof((socklen_t*)&TCP_Trd));
    //Close the original socket
    close(client_info->socket); 

    //Set up OS buf
    if(!client_info->snr_buf[0]) setsockopt(new_socket, SOL_SOCKET, SO_SNDBUF, &client_info->snr_buf[0], sizeof(client_info->snr_buf[0])); 
    if(!client_info->snr_buf[1]) setsockopt(new_socket, SOL_SOCKET, SO_RCVBUF, &client_info->snr_buf[1], sizeof(client_info->snr_buf[1]));


    /*
        Client Response Mode
    */
    if(client_info->msg[0] == 2){
        char* req = new char[4]; 
        
        for(int i=0;i<client_info->msg[5];i++){     //msg[5] is pktnum
            if(i > 0 && !client_info->msg[6]){
                close(new_socket);
                close(TCP_Trd_Socket);
                usleep(1000); //Ensure the socket is closed

                TCP_Trd_Socket = socket(AF_INET, SOCK_STREAM, 0);
                if(TCP_Trd_Socket < 0) {
                    cerr << "Failed to create HTTP socket" << strerror(errno) << endl;
                    delete[] req;
                    return;
                }
                    
                int opt = 1;
                setsockopt(TCP_Trd_Socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                TCP_Trd.sin_port = htons(port+i);
                if(::bind(TCP_Trd_Socket, (struct sockaddr*)&TCP_Trd, sizeof(TCP_Trd)) < 0) {
                    cerr << "Failed to bind HTTP socket " << strerror(errno) << endl;
                    delete[] req;
                    return;
                }
                if(listen(TCP_Trd_Socket, 5) < 0) {
                    cerr << "Failed to listen on HTTP socket " << strerror(errno) << endl;
                    delete[] req;
                    return;
                }
                new_socket = accept(TCP_Trd_Socket, (struct sockaddr*)&TCP_Trd, (socklen_t*)&TCP_Trd);
                if ( new_socket < 0) {
                    cerr << "Failed to accept connection " << strerror(errno) << endl;
                    delete[] req;
                    return;
                }

            }

            if (recv(new_socket, req, 4, 0) < 0) {
                cerr << "Failed to receive data" << strerror(errno) << endl;close(new_socket);
                return;}
            if (send(new_socket, req, 4, 0) <0) {
                cerr << "Failed to send data " << strerror(errno) << endl;close(new_socket);
                return;}

            if(!client_info->msg[6]){ //Reconnect for each packet
                close(new_socket);
                close(TCP_Trd_Socket);
            }
        }
    }


    /*
        Client Send Mode  -- Server Receiving 
    */
   int buf[client_info->msg[3]/4];
    if(client_info->msg[0] == 0){    

        for(int i=0;i<client_info->msg[5];i++){     //msg[5] is pktnum
            int r = 0; //bytes received from client
            while(!r){
                r = recv(new_socket, buf, sizeof(buf), 0);
            }
        }
    }

    /*
        Client Recv Mode  -- Server Sending 
    */
    else if(client_info->msg[0] == 1){

        float SendDelay = 0.0;
        if(client_info->msg[4] != 0) {  //Msg : [3: pktsize, 4: pktrate]
            SendDelay = 1000000*(float)client_info->msg[3]/client_info->msg[4];} //Set Delay for packet rate
        
        struct timeval StartTime, SentTime;
        gettimeofday(&StartTime, NULL);                 // Get intial start time   
        double stat_time = client_info->msg[1]*1000;    //msg[1] is stat

        for(int i=0;i<client_info->msg[5];i++){         //msg[5] is pktnum
            memset(buf, 0, sizeof(buf));
            buf[0] = i;                                 //Set pkt syn_num
            if (send(new_socket, buf, sizeof(buf), 0) < 0) {
                    cerr << "Failed to send data" << endl;close(new_socket);
                    return;}
            if(client_info->msg[4] != 0){
                gettimeofday(&SentTime, NULL);      // Get sent time if rate is set
                double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                usleep(stat_time-elapsed);
                gettimeofday(&StartTime, NULL);
                }
        }
    } 

    /*
        Free up the connection
    */
    close(new_socket);
    delete client_info;
}


void UDP_thread(ClientInfo* client_info){
    /*
        Set up UDP server and
        Pack the info of socket to send to client
        Send back a info of port to client
    */
    struct sockaddr_in UDP_Trd;
    memset(&UDP_Trd, 0, sizeof(UDP_Trd));
    UDP_Trd.sin_family = AF_INET;
    UDP_Trd.sin_addr.s_addr = INADDR_ANY;

    int UDP_Trd_Socket = socket(AF_INET, SOCK_DGRAM, 0);
    ::bind(UDP_Trd_Socket, (struct sockaddr*)&UDP_Trd, sizeof(UDP_Trd));
    socklen_t len = sizeof(UDP_Trd);
    getsockname(UDP_Trd_Socket, (struct sockaddr*)&UDP_Trd, &len); //Get the port
    int port = ntohs(UDP_Trd.sin_port);
    if (send((client_info->socket), &port, sizeof(port), 0) < 0) {
        std::cerr << "Failed to send data" << strerror(errno)<< std::endl;
        close(UDP_Trd_Socket);
        close(client_info->socket);
        delete client_info;
        return;
    }
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(UDP_Trd.sin_addr), ip_str, INET_ADDRSTRLEN);
    // cout << "Connected to " << ip_str << " port " << port;
    

    //Close the original socket
    close(client_info->socket); 

    //Set up OS buf
    if(!client_info->snr_buf[0]) setsockopt(UDP_Trd_Socket, SOL_SOCKET, SO_SNDBUF, &client_info->snr_buf[0], sizeof(client_info->snr_buf[0])); 
    if(!client_info->snr_buf[1]) setsockopt(UDP_Trd_Socket, SOL_SOCKET, SO_RCVBUF, &client_info->snr_buf[1], sizeof(client_info->snr_buf[1]));


    /*
        Client Response Mode
    */
    if(client_info->msg[0] == 2){
        char* req = new char[4]; 
        struct sockaddr_in client_addr = client_info->addr; //Client IP;
        client_addr.sin_port = htons(client_info->port); //Client Port
        
        for(int i=0;i<client_info->msg[5];i++){     //msg[5] is pktnum
            if (recvfrom(UDP_Trd_Socket, &req, sizeof(req), 0, (struct sockaddr*)&UDP_Trd, &len) < 0) {
                cerr << "Failed to receive data" << strerror(errno) << endl;close(UDP_Trd_Socket);
                return;}
            // puts("UDP Response Mode Received");
            if (sendto(UDP_Trd_Socket, req, sizeof(req), 0, (struct sockaddr*)&UDP_Trd, len) <0) {
                cerr << "Failed to send data " << strerror(errno) << endl;close(UDP_Trd_Socket);
                return;}
            // puts("UDP Response Mode Sent");
            // cout << req << endl;
        }
    }


    /*
        Client Send Mode  -- Server Receiving 
    */
    int buf[client_info->msg[3]/4];
    if(client_info->msg[0] == 0){    

        for(int i=0;i<client_info->msg[5];i++){     //msg[5] is pktnum
            recvfrom(UDP_Trd_Socket, buf, sizeof(buf), 0, (struct sockaddr*)&UDP_Trd, &len);
        }
    }
    /*
        Client Recv Mode  -- Server Sending 
    */
    else if(client_info->msg[0] == 1){

        struct sockaddr_in client_addr = client_info->addr; //Client IP;
        client_addr.sin_port = htons(client_info->port); //Client Port
        float SendDelay = 0.0;
        if(client_info->msg[4] != 0) {  //Msg : [3: pktsize, 4: pktrate]
            SendDelay = 1000000*(float)client_info->msg[3]/client_info->msg[4];} //Set Delay for packet rate
        
        struct timeval StartTime, SentTime;
        gettimeofday(&StartTime, NULL);                 // Get intial start time   
        double stat_time = client_info->msg[1]*1000;    //msg[1] is stat

        for(int i=0;i<client_info->msg[5];i++){         //msg[5] is pktnum
            memset(buf, 0, sizeof(buf));
            buf[0] = i;                                 //Set pkt syn_num
            if (sendto(UDP_Trd_Socket, buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
                    cerr << "Failed to send data" << endl;close(UDP_Trd_Socket);
                    return;}
            if(client_info->msg[4] != 0){
                gettimeofday(&SentTime, NULL);      // Get sent time if rate is set
                double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                usleep(stat_time-elapsed);
                gettimeofday(&StartTime, NULL);
                }
        }
        /*
        Free up the connection
        */
        // cout << "Mission Completed, ternimating connection" << endl;
        close(UDP_Trd_Socket);
        delete client_info;
    }
} 

void init_server(class Net_opt net_opt){
        ThreadPool threadpool(net_opt.poolsize);

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

    struct timeval StartTime, ElapsedTime;
    gettimeofday(&StartTime, NULL);                 // Get intial start time
    

    while (true) //Main thread to handle TCP connection
    {
        /*
            Client socket setup and connection
        */
        struct sockaddr_in TCP_Clnt_Addr;
        memset(&TCP_Clnt_Addr, 0, sizeof(TCP_Clnt_Addr));
        socklen_t TCP_Clnt_Addr_len = sizeof(TCP_Clnt_Addr);
        int TCP_Client_Socket = accept(TCP_Socket, (struct sockaddr*)&TCP_Clnt_Addr, &TCP_Clnt_Addr_len);
        if(TCP_Client_Socket < 0){
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }


        /*
            Receive msg from client 
            Print out the msg
        */
        ClientInfo* client_info = new ClientInfo;
        client_info->socket = TCP_Client_Socket;
        client_info->addr = TCP_Clnt_Addr;
        client_info->host = net_opt.lhost;
        client_info->port = net_opt.lport;
        if(!net_opt.sbufsize)client_info->snr_buf[0] = net_opt.sbufsize;
        if(!net_opt.rbufsize)client_info->snr_buf[1] = net_opt.rbufsize;
        if (recv(TCP_Client_Socket, &client_info->msg, sizeof(client_info->msg), 0) < 0) {
            cerr << "Failed to receive data" << endl;
            close(TCP_Client_Socket);
            delete client_info;
            return;
        }handle_msg(client_info);

        /*
            Create a thread TCP or UDP to handle the connection separately
            where msg[2] is the protocol 
            0 for TCP and 1 for UDP
        */
        if (client_info->msg[2] == 0) {
            threadpool.enqueue(client_info,TCP_thread);
            TCP_client++;
        } else if (client_info->msg[2] == 1) {
            threadpool.enqueue(client_info,UDP_thread);
            UDP_client++;
        }
        gettimeofday(&ElapsedTime, NULL);
        cout << "Elapsed " << (ElapsedTime.tv_sec - StartTime.tv_sec) << "s ";
        cout << "ThreadPool " << threadpool.get_active_threads() + 1 << "/" << threadpool.get_current_pool_size();
        cout << " TCP Clients " << TCP_client << " UDP Clients " << UDP_client; 
        cout<< "\r";
        cout.flush();
        
        

    }

}




int main(int argc, char *argv[]){
    Net_opt net_opt;
    static struct option options[] = {
            {"lhost",    required_argument, 0, 'a'},
            {"lport",    required_argument, 0, 'b'},
            {"sbufsize", required_argument, 0, 'c'},
            {"rbufsize", required_argument, 0, 'd'},
            {"tcpcca",   required_argument, 0, 't'},
            {"poolsize", required_argument, 0, 'p'},
            {0, 0, 0, 0}};

    int option_index = 0;
    int opt;
    while ((opt = getopt_long_only(argc, argv, "", options, &option_index)) != -1) {
            switch (opt) { 
                case 'p':
                    std::cout << "poolsize: " << optarg << std::endl;
                    net_opt.poolsize = atoi(optarg);
                    break;
                case 't':
                    std::cout << "tcpcca: " << optarg << std::endl;
                    net_opt.tcpcca = optarg; tcpcca = optarg;
                    break;
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



