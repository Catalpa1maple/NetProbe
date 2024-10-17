#ifndef NETPROBE_H
#define NETPROBE_H

#define infinite 1000000000
class Net_opt {
    public:
        enum mode {SEND, RECV} mode;
        int stat = 500; // Default 500 ms
        std::string rhost = "localhost"; // Default localhost
        std::string lhost = "IN_ADDR_ANY"; // Default Late Binding
        int rport = 4180, lport = 4180; // Default 4180
        std::string proto = "UDP"; // Default UDP
        int pktsize = 1000; // Default 1000 bytes
        int pktrate = 1000; // Default 1000 bytes/second
        int pktnum = infinite; // Default 0 (infinite)
        int sbufsize = 0;
        int rbufsize = 0;
};

class ClientInfo {
    public:
        int socket;
        struct sockaddr_in addr;
        int msg[6]; //0: mode, 1: stat, 2: proto, 3: pktsize, 4: pktrate, 5: pktnum
        int snr_buf[2];
        std::string host;
        int port;
};


#endif // NETPROBE_H