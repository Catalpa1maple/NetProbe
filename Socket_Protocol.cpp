#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <sys/time.h>
#include <format>
#include <iomanip>
#include "Socket_Protocol.h"

void stat_cout(int mode, double stat, int pkts, int lossnum, double lossrate, int rate, int jitter, int index){
    //stat unit is second
    double elap = (double)stat*index;
    std::string rate_unit = "Bps";
    if(mode == RECV){
        std::cout << "Elapsed " << std::fixed 
            << std::setprecision(1) << elap << 
            "s Pkts " << pkts << " Lost "<< lossnum
            << ", " << lossrate << "% Rate " << rate
            << rate_unit << " Jitter " <<  jitter << "ms"
        << std::endl;
    }
    else if(mode == SEND){
        std::cout << "Elapsed " << std::fixed
            << std::setprecision(1)
            << elap << "s Rate " 
            << rate << rate_unit 
        << std::endl;
    }
}


void TCP_socket(int mode,int stat, std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize){
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
    
    char buf[pktsize];
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
        struct timeval StartTime, SentTime; 
        float SendDelay = 0.0;
        
        if(pktrate != 0) {
            SendDelay = 1000000*(float)pktsize/pktrate;} //Set Delay for packet rate
        // std::cout << SendDelay << std::endl;
        if (!bufsize) setsockopt(TCP_Socket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)); // Set OS buffer size
        
        memset(&buf, 'A', pktsize); // Fill buffer with data
        std::cout << pktsize*pktnum << " bytes data to be sent" << std::endl;
        int stat_index = 1;

        for (int i = 0; i < pktnum; i++) {
            gettimeofday(&StartTime, NULL);     // Get start time
            if(send(TCP_Socket, &buf, pktsize, 0) < 0){
                std::cerr << "Failed to send data " << strerror(errno) <<std::endl;
                close(TCP_Socket);return;}
            // std::cout << "pkt " << i << " sent" << std::endl;

            gettimeofday(&SentTime, NULL);      // Get sent time
            
            double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
            double stat_time = stat*1000;  //Set stat time in us   

            while (elapsed < SendDelay) {    
                    elapsed += 10000; //10ms 
                    usleep(10000);
                    if(stat_time < elapsed){
                        stat_cout(SEND,(double)stat/1000,0,0,0,pktrate,0,stat_index);
                        stat_time += stat_time;
                        stat_index++;
            }} 
        }
        std::cout << "Data sent: " << sizeof(buf)*pktnum << " bytes"<< std::endl;
        close(TCP_Socket);
    }

    // Close the socket after use
    close(TCP_Socket);
}


void UDP_socket(int mode,int stat, std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize){
    host = getHost(host);
    // struct hostent* addr = gethostbyname(host.c_str());
    // std::cout << addr->h_name << std::endl;
    struct sockaddr_in UDP_Addr;
    memset (&UDP_Addr, 0, sizeof(UDP_Addr)); 
    UDP_Addr.sin_family = AF_INET;
    UDP_Addr.sin_port = htons(port); 
    UDP_Addr.sin_addr.s_addr = inet_addr(host.c_str());

    int UDP_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket
    if (UDP_Socket < 0) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return;
    }

    int buf[pktsize]; // Buffer to store data
    if(mode == RECV){ //As RECV mode be consider as Server
        if (bind(UDP_Socket, (struct sockaddr*)&UDP_Addr, sizeof(UDP_Addr)) < 0) {
            std::cerr << "Failed to bind UDP socket" <<  std::endl;
            close(UDP_Socket);
            return;
        }    
        std::cout << "UDP socket created and bound to " << host << ":" << port <<  std::endl;
    
        if (!bufsize)setsockopt(UDP_Socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)); // Set OS buffer size
        socklen_t len = sizeof(UDP_Addr);
        int pkt_index = 0, pkt_loss = 0;        //For checking loss
        int pkt_num = 0, stat_index = 1;
        struct timeval StartTime, SentTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time
        double stat_time =(double)stat/1000;
        while (true)
        {
            if(recvfrom(UDP_Socket, &buf, pktsize, 0, (struct sockaddr*)&UDP_Addr, &len) < 0){
                std::cerr << "Failed to receive data " << strerror(errno) <<std::endl;
                close(UDP_Socket);
                return;
            }pkt_index++; pkt_num++;//Pkt num suggests nth pkt sent

            if (buf[0] != pkt_index) pkt_loss++; //Check loss
            // std::cout << "pkt " << pkt_index << std::endl;
            // std::cout << "buf " << buf[0] << std::endl;

            gettimeofday(&SentTime, NULL);      // Get sent time
            double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                if( stat_time*1000000 < elapsed){       //Check is it time to print stat
                    // std::cout << pkt_index << " " << pkt_num  << std::endl;
                    double rate = pkt_num*sizeof(buf)/(double)stat_time;
                    pkt_num = 0;    //Reset pkt num
                    double loss_rate = pkt_loss*100/pkt_index;
                    stat_cout(RECV,stat_time,pkt_index,pkt_loss,loss_rate,rate,0,stat_index);
                    stat_index++;
                    gettimeofday(&StartTime, NULL);
                }
        }
    }


    else if(mode == SEND){ //As SEND mode be consider as Client
         
        float SendDelay = 0.0;
        if(pktrate != 0) {
            SendDelay = 1000000*(float)pktsize/pktrate;} //Set Delay for packet rate
        // std::cout << SendDelay << std::endl;
        if (!bufsize) setsockopt(UDP_Socket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)); // Set OS buffer size
        
        // std::cout << pktsize*pktnum << " bytes data to be sent" << std::endl;
        int stat_index = 1,pkt_num = 0;
        struct timeval StartTime, SentTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time   
        double stat_time = stat*1000;

        for (int i = 0; i < pktnum; i++) {
            
            memset(buf,i+1,sizeof(buf)); // Fill buffer 
            buf[0] = i+1;                //Set pkt index
            // std::cout << buf[0] << std::endl;

            if(pktrate != 0)gettimeofday(&StartTime, NULL);     // Get start time if rate is set
            if(sendto(UDP_Socket, &buf, pktsize, 0, (struct sockaddr*)&UDP_Addr, sizeof(UDP_Addr)) < 0){
                std::cerr << "Failed to send data " << strerror(errno) <<std::endl;
                close(UDP_Socket);return;}
            
            if(pktrate != 0){
                gettimeofday(&SentTime, NULL);      // Get sent time if rate is set
                double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                stat_time = stat*1000;  //Set stat time in us
                while (elapsed < SendDelay) {    
                        elapsed += 10000; //10ms 
                        usleep(10000);
                        if(stat_time < elapsed){
                            stat_cout(SEND,(double)stat/1000,0,0,0,pktrate,0,stat_index);
                            stat_time += stat_time;
                            stat_index++;}}
            }
            if (pktrate == 0){
                gettimeofday(&SentTime, NULL);
                double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                stat_time =(double)stat/1000;
                if( stat_time*1000000 < elapsed){
                    pkt_num = i - pkt_num + 1;
                    double rate = pkt_num*1000000/(double)stat_time;
                    stat_cout(SEND,stat_time,0,0,0,rate,0,stat_index);
                    stat_index++;
                    gettimeofday(&StartTime, NULL);
                }
            } 
        }
        // std::cout << "Data sent: " << sizeof(buf)*pktnum << " bytes"<< std::endl;
        close(UDP_Socket);
    }
    
    // Close the socket after use
    close(UDP_Socket);
}


char getHost(std::string& host){
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
        return EXIT_FAILURE;
    }

    char ipstr[INET_ADDRSTRLEN];
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        void* addr = &((struct sockaddr_in*)p->ai_addr)->sin_addr;
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        std::cout << host << " resolved to: " << ipstr << std::endl;
    }
    return *ipstr;
}