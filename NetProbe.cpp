#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "Socket_Protocol.h"
#define infinite 1000000000

using namespace std;  

int main(int argc, char *argv[]) {
    int opt;
    enum mode mode = NONE;
    int stat = 500; // Default 500 ms
    string rhost = "localhost"; // Default localhost
    string lhost = "IN_ADDR_ANY"; // Default Late Binding
    int rport, lport= 4180; // Default 4180
    string proto = "UDP"; // Default UDP
    int pktsize = 1000; // Default 1000 bytes
    int pktrate = 1000; // Default 1000 bytes/second
    int pktnum = infinite; // Default 0 (infinite)
    int sbufsize = 0;
    int rbufsize = 0;
    string hostname = "localhost"; // Default localhost

    static struct option options[] = {
            {"send",     no_argument, 0, '1'},
            {"recv",     no_argument, 0, '2'},
            {"host",     no_argument, 0, '3'},
            {"stat",     required_argument, 0, 's'},
            {"rhost",    required_argument, 0, 'r'},
            {"rport",    required_argument, 0, 'p'},
            {"proto",    required_argument, 0, 'c'},
            {"pktsize",  required_argument, 0, 'z'},
            {"pktrate",  required_argument, 0, 'a'},
            {"pktnum",   required_argument, 0, 'n'},
            {"sbufsize", required_argument, 0, 'u'},
            {"lhost",    required_argument, 0, 'l'},
            {"lport",    required_argument, 0, 'q'},
            {"rbufsize", required_argument, 0, 'e'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        while ((opt = getopt_long_only(argc, argv, "", options, &option_index)) != -1) {
            switch (opt) { 
                case '1':
                    mode = SEND;
                    break;
                case '2':
                    mode = RECV;
                    break;
                case '3':
                    mode = HOST;
                    if(argv[2] == NULL) {
                        cout << "Invalid option" << endl;
                        return EXIT_FAILURE;
                    }
                    hostname = argv[3];
                    break;
                case 's':
                    stat = atoi(optarg);
                    cout << "stat: " << stat << endl;
                    break;
                case 'r':
                    rhost = optarg;
                    cout << "rhost: " << rhost << endl;
                    break;
                case 'p':
                    rport = atoi(optarg);
                    cout << "rport: " << rport << endl;
                    break;
                case 'c':
                    proto = optarg;
                    if(proto != "TCP" && proto != "UDP") {
                        cout << "Invalid protocol" << endl;
                        return EXIT_FAILURE;
                    }
                    cout << "proto: " << proto << endl;
                    break;
                case 'z':
                    pktsize = atoi(optarg);
                    cout << "pktsize: " << pktsize << endl;
                    break;
                case 'a':
                    pktrate = atoi(optarg);
                    cout << "pktrate: " << pktrate << endl;
                    break;
                case 'n':
                    pktnum = atoi(optarg);
                    cout << "pktnum: " << pktnum << endl;
                    break;
                case 'u':  sbufsize = atoi(optarg);
                    cout << "sbufsize: " << sbufsize << endl;
                    break;
                case 'l':
                    lhost = optarg;
                    cout << "lhost: " << lhost << endl;
                    break;
                case 'q':
                    lport = atoi(optarg);
                    cout << "lport: " << lport << endl;
                    break;
                case 'k':
                    pktsize = atoi(optarg);
                    cout << "pktsize: " << pktsize << endl;
                    break;
                case 'e':
                    rbufsize = atoi(optarg);
                    cout << "rbufsize: " << rbufsize << endl;
                    break;
                case 'h':
                    hostname = optarg;
                    break;
                default:
                    cout << "Invalid option" << endl;
                    return EXIT_FAILURE;
            }
        }
        if (mode == SEND) {
            if (proto == "TCP") TCP_socket(mode, stat, rhost, rport, pktsize, pktrate, pktnum, sbufsize);
            else if (proto == "UDP")UDP_socket(mode, stat, rhost, rport, pktsize, pktrate, pktnum, sbufsize);
        }
        else if (mode == RECV) {
            if (proto == "TCP") TCP_socket(mode, stat, lhost, lport, pktsize, pktrate, pktnum, rbufsize);
            else if (proto == "UDP")UDP_socket(mode, stat, lhost, lport, pktsize, pktrate, pktnum, rbufsize);
        }
        else if (mode == HOST) {
            hostname = getHost(hostname);
        }
        else {
            cout << "Invalid mode" << endl;
            return EXIT_FAILURE;
        }
        
   
    return 0;

}