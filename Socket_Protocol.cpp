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

void stat_cout(int mode, double stat, int pkts, int lossnum, double lossrate, double rate, int jitter, int index){
    //stat unit is second
    double elap = (double)stat*index;
    std::string rate_unit = "Kbps";
    rate = rate*8;
    // std::cout << rate << std::endl;
    if(rate > (double)1000){
        rate = (rate/1000);
        rate_unit = "Mbps";
    }


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
    if( host == "IN_ADDR_ANY") {
        TCP_Addr.sin_addr.s_addr = INADDR_ANY;
    }else{
        host = getHost(host);
        TCP_Addr.sin_addr.s_addr = inet_addr(host.c_str());
    }

    int TCP_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (TCP_Socket < 0) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return;
    }
    
    int buf[pktsize/4];
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

        int recv_Socket = accept(TCP_Socket, NULL, NULL);
        if (recv_Socket < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            close(recv_Socket);
            close(TCP_Socket);
            return;
        }
        std::cout << "Connection accepted" << std::endl;

        if(!bufsize)setsockopt(recv_Socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)); // Set OS buffer size
        socklen_t len = sizeof(recv_Socket);
        int pkt_index = 0, byte_loss = 0, pkt_loss = 0;        //For checking loss
        int pkt_num = 0, stat_index = 1, total_bytes = 0;
        int NumItv = 0; double MeanJitter = 0, MeanRecvItv = 0; // For jitter calculation
        struct timeval StartTime, SentTime;
        struct timeval RecvTime, LastRecvTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time
        double stat_time =(double)stat/1000;
        
        while (true){   
            int r = recv(recv_Socket, &buf, pktsize,0);
            // std::cout << r << std::endl;
            if(r < 0){
                std::cerr << "Failed to receive data " << strerror(errno) <<std::endl;
                close(recv_Socket);
                return;
            }pkt_index++; pkt_num++;//Pkt num suggests nth pkt sent
            total_bytes += r;       //Sum up r to total bytes

            if(r == 0){         //Check if data
                if(total_bytes){//Reset if no data received
                    std::cout << "Recevied " << total_bytes << "Bytes" << std::endl;
                    total_bytes = 0;pkt_index = 0;pkt_num=0;
                }
                continue;    
            }

            if (buf[0] != pkt_index){
                pkt_loss++; //Check loss
                gettimeofday(&RecvTime, NULL);
                double RecvItv = (double)(RecvTime.tv_sec - LastRecvTime.tv_sec)*1000 +
                (double)(RecvTime.tv_usec - LastRecvTime.tv_usec)/1000; 
                gettimeofday(&LastRecvTime, NULL);
                ++NumItv;   
                if (NumItv) MeanJitter = (MeanJitter*NumItv + (((RecvItv - MeanRecvItv)>=0) ? (RecvItv - MeanRecvItv) : (MeanRecvItv - RecvItv)))/(NumItv+1);
                MeanRecvItv = (MeanRecvItv*NumItv + RecvItv)/(NumItv+1);
            }

            gettimeofday(&SentTime, NULL);      // Get sent time
            double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                if( stat_time*1000000 < elapsed){       //Check is it time to print stat
                    // std::cout << pkt_index << " " << pkt_num  << std::endl;
                    double rate = pkt_num*sizeof(buf)/(double)stat_time;
                    pkt_num = 0;    //Reset pkt num
                    double loss_rate = pkt_loss*100/pkt_index;
                    stat_cout(RECV,stat_time,pkt_index,pkt_loss,loss_rate,rate,MeanJitter,stat_index);
                    stat_index++;
                    gettimeofday(&StartTime, NULL);
                }}
    }

    
    else if(mode == SEND){ //As SEND mode be consider as Client
        
        if (connect(TCP_Socket, (struct sockaddr*)&TCP_Addr, sizeof(TCP_Addr)) < 0) {
            std::cerr << "Failed to connect TCP socket: " << strerror(errno) << std::endl;
            close(TCP_Socket);
            return;
        }

        float SendDelay = 0.0;
        if(pktrate != 0) {
            SendDelay = 1000000*(float)pktsize/pktrate;} //Set Delay for packet rate
        // std::cout << SendDelay << std::endl;
        if (!bufsize) setsockopt(TCP_Socket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)); // Set OS buffer size
        
        // std::cout << pktsize*pktnum << " bytes data to be sent" << std::endl;
        int stat_index = 1,pkt_num = 0;
        struct timeval StartTime, SentTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time   
        double stat_time = stat*1000;

        for (int i = 0; i < pktnum; i++) {
            
            memset(buf,i+1,sizeof(buf));        // Fill buffer 
                buf[0] = i+1;                   //Set pkt index
            // std::cout << buf[0] << std::endl;

            if(pktrate != 0)gettimeofday(&StartTime, NULL);     // Get start time if rate is set
            if(send(TCP_Socket, &buf, pktsize, 0) < 0){
                std::cerr << "Failed to send data " << strerror(errno) <<std::endl;
                close(TCP_Socket);return;}
            
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
        std::cout << "Data sent: " << sizeof(buf)*pktnum << " bytes"<< std::endl;
        close(TCP_Socket);
    }
}


void UDP_socket(int mode,int stat, std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize){
    // struct hostent* addr = gethostbyname(host.c_str());
    // std::cout << addr->h_name << std::endl;
    struct sockaddr_in UDP_Addr;
    memset (&UDP_Addr, 0, sizeof(UDP_Addr)); 
    UDP_Addr.sin_family = AF_INET;
    UDP_Addr.sin_port = htons(port); 
    if( host == "IN_ADDR_ANY") {
        UDP_Addr.sin_addr.s_addr = INADDR_ANY;
        UDP_Addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    }
    else if(host == "localhost") 
        host = "0.0.0.0";
    else{
        host = getHost(host);
        UDP_Addr.sin_addr.s_addr = inet_addr(host.c_str());
    }
     

    int UDP_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket
    if (UDP_Socket < 0) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return;
    }

    int buf[pktsize/4]; // Buffer to store data
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
        int NumItv = 0; double MeanJitter = 0, MeanRecvItv = 0; // For jitter calculation
        struct timeval StartTime, SentTime;
        struct timeval RecvTime, LastRecvTime;
        gettimeofday(&StartTime, NULL);         // Get intial start time
        double stat_time =(double)stat/1000;
        while (true)
        {
            if(recvfrom(UDP_Socket, &buf, pktsize, 0, (struct sockaddr*)&UDP_Addr, &len) < 0){
                std::cerr << "Failed to receive data " << strerror(errno) <<std::endl;
                close(UDP_Socket);
                return;
            }pkt_index++; pkt_num++;//Pkt num suggests nth pkt sent

            if (buf[0] != pkt_index){
                pkt_loss++; //Check loss
                gettimeofday(&RecvTime, NULL);
                double RecvItv = (double)(RecvTime.tv_sec - LastRecvTime.tv_sec)*1000 +
                (double)(RecvTime.tv_usec - LastRecvTime.tv_usec)/1000; 
                gettimeofday(&LastRecvTime, NULL);
                ++NumItv;   
                if (NumItv) MeanJitter = (MeanJitter*NumItv + (((RecvItv - MeanRecvItv)>=0) ? (RecvItv - MeanRecvItv) : (MeanRecvItv - RecvItv)))/(NumItv+1);
                MeanRecvItv = (MeanRecvItv*NumItv + RecvItv)/(NumItv+1);
            }
            // std::cout << "pkt " << pkt_index << std::endl;
            // std::cout << "buf " << buf[0] << std::endl;

            gettimeofday(&SentTime, NULL);      // Get sent time
            double elapsed = (double)(SentTime.tv_sec - StartTime.tv_sec)*1000000 + (double)(SentTime.tv_usec - StartTime.tv_usec);
                if( stat_time*1000000 < elapsed){       //Check is it time to print stat
                    double rate = (pkt_num*1000*sizeof(buf))/elapsed;
                    pkt_num = 0;    //Reset pkt num
                    double loss_rate = pkt_loss*100/pkt_index;
                    stat_cout(RECV,stat_time,pkt_index,pkt_loss,loss_rate,rate,MeanJitter,stat_index);
                    stat_index++;
                    gettimeofday(&StartTime, NULL);
                }
        }
    }


    else if(mode == SEND){ //As SEND mode be consider as Client
        std::cout << "UDP socket created and connected to " << host << ":" << port <<  std::endl;
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
                    pkt_num = i - pkt_num;
                    double rate = (pkt_num*pktsize)/elapsed;
                    stat_cout(SEND,stat_time,0,0,0,rate*1000,0,stat_index);
                    stat_index++;
                    gettimeofday(&StartTime, NULL);
                }
            } 
        }
        std::cout << "Data sent: " << sizeof(buf)*pktnum << " bytes"<< std::endl;
        close(UDP_Socket);
    }
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