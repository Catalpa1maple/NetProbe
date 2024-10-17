#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
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
#include "NetProbe.h"
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define WSAWOULDBLOCK EWOULDBLOCK

//Macros to translate winsock API to Linux API
#define WSAGetLastError() (errno)
#define closesocket(s) close(s)
#define ioctlsocket ioctl
#define Sleep(x) usleep(x*1000) //Sleep() is in ms

#endif
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

void stat_cout(int mode, double stat, int pkts, int lossnum, double lossrate, double rate, int jitter, int index){
    //stat unit is second
    double elap = (double)stat*index;
    string rate_unit = "KBps";
    

    if(rate > (double)1024){
        rate = (rate/1024);
        rate_unit = "MBps";
    }


    if(mode == Net_opt::RECV){
        cout << "Elapsed " << fixed; 
        cout<< setprecision(1) << elap;
        cout<<"s Pkts " << pkts << " Lost "<< lossnum;
        cout<< ", " << lossrate << "% Rate " << rate;
        cout<< rate_unit << " Jitter " <<  jitter << "ms";
        cout<< "\r";
        cout.flush();
    }

    else if(mode == Net_opt::SEND){
        cout << "Elapsed " << fixed;
        cout<< setprecision(1);
        cout<< elap << "s Rate "; 
        cout<< rate << rate_unit; 
        cout<< "\r";
        cout.flush();
    }
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

    struct sockaddr_in serv_addr, clin_addr;
    socklen_t len = sizeof(serv_addr);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(net_opt.rport);
    if( net_opt.rhost == "localhost") { net_opt.rhost = "127.0.0.1"; }
    serv_addr.sin_addr.s_addr = inet_addr(net_opt.rhost.c_str());
    clin_addr = serv_addr;
    

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

    /*
        For sending msg of config -- Using TCP first
    */
    if( send(TCP_client_socket, msg_int, sizeof(int)*8, 0) == INVALID_SOCKET){
        std::cerr << "Failed to send data. Error: " << WSAGetLastError() << std::endl;
        closesocket(TCP_client_socket);
        return;
    }
    
    int buf[net_opt.pktsize/4]; // Buffer to store data
    int port;
    /*
        For receiving port from server
    */
    int retries = 3;
    while (retries > 0) {
        int by_re = recv(TCP_client_socket, (char*)&port, sizeof(port), 0);
        if (by_re > 0) break;
        Sleep(1);
        retries--;
    }
    if (retries == 0) {
        cout << "Failed to receive port after multiple attempts" << endl;
    }
    serv_addr.sin_port = htons(port);      //Change to the port that server send back
    closesocket(TCP_client_socket);

    if(net_opt.proto == "TCP"){
        SOCKET data_client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (data_client_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create client socket. Error: " << WSAGetLastError() << std::endl;
            return;
        }
        if (connect(data_client_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to connect to server. Error: " << WSAGetLastError() << std::endl;
            closesocket(data_client_socket);
            return;
        }
        cout << "connected to server, port " << port << endl;


    
    /*
        Client Send Mode 
    */
    if(net_opt.mode == Net_opt::SEND){
        float SendDelay = 0.0;
        if(net_opt.pktrate != 0) {
            SendDelay = 1000000*(float)net_opt.pktsize/net_opt.pktrate;} //Set Delay for packet rate
        if (net_opt.sbufsize) setsockopt(data_client_socket, SOL_SOCKET, SO_SNDBUF, &net_opt.sbufsize, sizeof(net_opt.sbufsize)); // Set OS buffer size
        
        int stat_index = 1,pkt_send = 0;
        struct timeval StartTime, SentTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time   
        double stat_time = net_opt.stat*1000;
            for(int i=0;i<net_opt.pktnum;i++){
                memset(buf, i, sizeof(buf));
                if(net_opt.pktrate != 0)gettimeofday(&StartTime, NULL);     // Get start time if rate is set
                if (send(data_client_socket, buf, sizeof(buf), 0) == SOCKET_ERROR) {
                    std::cerr << "Failed to send data" << std::endl;closesocket(data_client_socket);
                    return;}
                if(net_opt.pktrate != 0){
                    gettimeofday(&SentTime, NULL);      // Get sent time if rate is set
                    double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                    stat_time = net_opt.stat*1000;  //Set stat time in us
                    while (elapsed < SendDelay) {    
                        elapsed += 10000; //10ms 
                        Sleep(10);
                        if(stat_time < elapsed){
                            stat_cout(Net_opt::SEND,(double)net_opt.stat/1000,0,0,0,net_opt.pktrate,0,stat_index);
                            stat_time += stat_time;
                            stat_index++;}}
                }
                if (net_opt.pktrate == 0){      //Infinte pktrate
                    gettimeofday(&SentTime, NULL);
                    double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                    stat_time =(double)net_opt.stat/1000;
                    if( stat_time*1000000 < elapsed){
                        pkt_send = i - pkt_send;
                        double rate = (pkt_send*net_opt.pktsize)/(elapsed/1000000);
                        stat_cout(Net_opt::SEND,stat_time,0,0,0,rate,0,stat_index);
                        pkt_send = i;     // pkt_send is the number of packets sent
                        stat_index++;
                        gettimeofday(&StartTime, NULL);}
                }
            }
    }

    /*
        Client Recv Mode
    */
    else if(net_opt.mode == Net_opt::RECV){
        if (net_opt.rbufsize) setsockopt(data_client_socket, SOL_SOCKET, SO_SNDBUF, &net_opt.rbufsize, sizeof(net_opt.rbufsize)); // Set OS buffer size
        int pkt_index = 0, byte_loss = 0, pkt_loss = 0;        //For checking loss
        int pkt_recv = 0, stat_index = 1, total_bytes = 0;
        int NumItv = 0; double MeanJitter = 0, MeanRecvItv = 0; // For jitter calculation
        struct timeval StartTime, SentTime;
        struct timeval RecvTime, LastRecvTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time
        double stat_time =(double)net_opt.stat*1000;

        for (int i=0;i<net_opt.pktnum;i++){
            int r = recv(data_client_socket, &buf, net_opt.pktsize,0);
            if(r < 0){
                std::cerr << "Failed to receive data " << strerror(errno) <<std::endl;
                closesocket(data_client_socket);
                return;
            }pkt_recv++;           //Pkt num suggests nth pkt sent
            total_bytes += r;                   //Sum up r to total bytes
            gettimeofday(&SentTime, NULL);      // Get sent time

            if (buf[0] != pkt_index){
                pkt_loss++; //Check loss     
            }else{
                gettimeofday(&RecvTime, NULL);
                double RecvItv = (double)(RecvTime.tv_sec - LastRecvTime.tv_sec)*1000 +
                (double)(RecvTime.tv_usec - LastRecvTime.tv_usec)/1000; 
                gettimeofday(&LastRecvTime, NULL);
                ++NumItv;   
                if (NumItv) MeanJitter = (MeanJitter*NumItv + (((RecvItv - MeanRecvItv)>=0) ? (RecvItv - MeanRecvItv) : (MeanRecvItv - RecvItv)))/(NumItv+1);
                MeanRecvItv = (MeanRecvItv*NumItv + RecvItv)/(NumItv+1);
                MeanJitter = MeanJitter/(1000000*100);
            }
            pkt_index++;

            double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
            if( stat_time < elapsed){       //Check is it time to print stat
                double rate = (pkt_recv*sizeof(buf))/elapsed;
                pkt_recv = 0;    //Reset pkt recv
                double loss_rate = pkt_loss*100/pkt_index;
                stat_cout(Net_opt::RECV,(double)net_opt.stat/1000,pkt_index,pkt_loss,loss_rate,rate,MeanJitter,stat_index);
                stat_index++;
                gettimeofday(&StartTime, NULL);
            }}
        }


        cout << "Mission Completed" <<endl;
        closesocket(data_client_socket);
    }   
    
    else if(net_opt.proto == "UDP"){
        SOCKET data_client_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (data_client_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create client socket. Error: " << WSAGetLastError() << std::endl;
            return;
        }

        if(net_opt.mode == Net_opt::SEND){
            float SendDelay = 0.0;
            if(net_opt.pktrate != 0) {
            SendDelay = 1000000*(float)net_opt.pktsize/net_opt.pktrate;} //Set Delay for packet rate
            if (net_opt.sbufsize) setsockopt(data_client_socket, SOL_SOCKET, SO_SNDBUF, &net_opt.sbufsize, sizeof(net_opt.sbufsize)); // Set OS buffer size
        
            int stat_index = 1,pkt_send = 0;
            struct timeval StartTime, SentTime;
            gettimeofday(&StartTime, NULL);         // Get intial start time   
            double stat_time = net_opt.stat*1000;

            for (int i = 0; i < net_opt.pktnum; i++) {
                
                memset(buf,i+1,sizeof(buf)); // Fill buffer 
                buf[0] = i+1;                //Set pkt index

                if(net_opt.pktrate != 0)gettimeofday(&StartTime, NULL);     // Get start time if rate is set
                if(sendto(data_client_socket, &buf, net_opt.pktsize, 0, (struct sockaddr*)&serv_addr, len) < 0){
                    std::cerr << "Failed to send data " << strerror(errno) <<std::endl;
                    closesocket(data_client_socket);return;}

                if(net_opt.pktrate != 0){
                    gettimeofday(&SentTime, NULL);      // Get sent time if rate is set
                    double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                    stat_time = net_opt.stat*1000;  //Set stat time in us
                    while (elapsed < SendDelay) {    
                            elapsed += 10000; //10ms 
                            Sleep(10);
                            if(stat_time < elapsed){
                                stat_cout(Net_opt::SEND,(double)net_opt.stat/1000,0,0,0,net_opt.pktrate,0,stat_index);
                                stat_time += stat_time;
                                stat_index++;
                    }       }

                }
                if (net_opt.pktrate == 0){    // Infinte pktrate
                    gettimeofday(&SentTime, NULL);
                    double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                    stat_time =(double)net_opt.stat/1000;
                    if( stat_time*1000000 < elapsed){
                        pkt_send = i - pkt_send;
                        double rate = (pkt_send*net_opt.pktsize)/(elapsed/1000000);    // bytes/second
                        stat_cout(Net_opt::SEND,stat_time,0,0,0,rate,0,stat_index);
                        pkt_send = i;     // pkt_send is the number of packets sent
                        stat_index++;
                        gettimeofday(&StartTime, NULL);
                    }
                
                
        }   }   }

        else if(net_opt.mode == Net_opt::RECV){
            if (::bind(data_client_socket, (struct sockaddr*)&clin_addr, sizeof(clin_addr)) == SOCKET_ERROR) {
                std::cerr << "Failed to bind socket. Error: " << WSAGetLastError() << std::endl;
                closesocket(data_client_socket);
                return;
            }

            socklen_t len = sizeof(serv_addr);
            getsockname(data_client_socket, (struct sockaddr*)&serv_addr, &len);
            int assigned_port = ntohs(serv_addr.sin_port);
            cout << "UDP socket bound to port: " << assigned_port << endl;

            if (net_opt.rbufsize)setsockopt(data_client_socket, SOL_SOCKET, SO_RCVBUF, &net_opt.rbufsize, sizeof(net_opt.rbufsize)); // Set OS buffer size
            int pkt_index = 0, pkt_loss = 0;        //For checking loss
            int pkt_recv = 0, stat_index = 1;
            int NumItv = 0; double MeanJitter = 0, MeanRecvItv = 0; // For jitter calculation
            struct timeval StartTime, SentTime;
            struct timeval RecvTime, LastRecvTime;
            gettimeofday(&StartTime, NULL);         // Get intial start time
            double stat_time =(double)net_opt.stat*1000;    //as stat is in ms
            
            for (int i=0;i<net_opt.pktnum;i++){
                socklen_t len = sizeof(clin_addr);
                int r = recvfrom(data_client_socket, &buf, net_opt.pktsize, 0, (struct sockaddr*)&clin_addr, &len);
                if (r == SOCKET_ERROR) {
                    std::cerr << "Failed to receive data. Error: " << WSAGetLastError() << std::endl;
                    closesocket(data_client_socket);
                    return;
                }
                pkt_recv++;//Pkt num suggests nth pkt sent
                gettimeofday(&SentTime, NULL);      // Get sent time
                if (buf[0] != pkt_index){
                    pkt_loss++; //Check loss
                }else{
                    gettimeofday(&RecvTime, NULL);
                    double RecvItv = (double)(RecvTime.tv_sec - LastRecvTime.tv_sec)*1000 +
                    (double)(RecvTime.tv_usec - LastRecvTime.tv_usec)/1000; 
                    gettimeofday(&LastRecvTime, NULL);
                    ++NumItv;   
                    if (NumItv) MeanJitter = (MeanJitter*NumItv + (((RecvItv - MeanRecvItv)>=0) ? (RecvItv - MeanRecvItv) : (MeanRecvItv - RecvItv)))/(NumItv+1);
                    MeanRecvItv = (MeanRecvItv*NumItv + RecvItv)/(NumItv+1);
                    MeanJitter = MeanJitter/(1000000*100);
                }

                double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                if( stat_time < elapsed){       //Check is it time to print stat
                    double rate = (pkt_recv*sizeof(buf))/elapsed;
                    pkt_recv = 0;    //Reset pkt num
                    double loss_rate = pkt_loss*100/pkt_index;
                    stat_cout(Net_opt::RECV,(double)net_opt.stat/1000,pkt_index,pkt_loss,loss_rate,rate*1000,MeanJitter,stat_index);
                    stat_index++;
                    gettimeofday(&StartTime, NULL);
                }
                pkt_index++;
            }
}}}
#ifdef WIN32
    WSACleanup();
#endif




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
                case 'k':   
                    net_opt.rbufsize = atoi(optarg);
                    break;
                default:
                    return EXIT_FAILURE;
            }
        }
        if (net_opt.mode < 0){
            std::cerr << "Error : Mode not specified" << std::endl;
            return EXIT_FAILURE;
        }
        //print net_opt info
        std::cout << "Mode:                ";
        if (net_opt.mode == Net_opt::SEND) std::cout << "SEND" << std::endl;
        else if (net_opt.mode == Net_opt::RECV) std::cout << "RECV" << std::endl;
        std::cout << "Stat:                " << net_opt.stat << " ms" << std::endl;
        std::cout << "Remote Host:         " << net_opt.rhost << std::endl;
        std::cout << "Remote Port:         " << net_opt.rport << std::endl;
        std::cout << "Protocol:            " << net_opt.proto << std::endl;
        std::cout << "Packet Size:         " << net_opt.pktsize << " bytes" << std::endl;
        std::cout << "Packet Rate:         " << net_opt.pktrate << " bytes/second" << std::endl;
        std::cout << "Packet Number:       ";
        if (net_opt.pktnum == 0) std::cout << "Infinite" << std::endl;
        else std::cout << net_opt.pktnum << std::endl;
        std::cout << "Send Buffer Size:    " << net_opt.sbufsize << " bytes" << std::endl;
        std::cout << "Receive Buffer Size: " << net_opt.rbufsize << " bytes" << std::endl;

        init_client(net_opt);
}


