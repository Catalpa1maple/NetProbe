#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "TCP_socket.h"
#include "UDP_socket.h"

using namespace std;  
enum mode {NONE,SEND,RECV,HOST};

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
    int pktnum = 0; // Default 0 (infinite)
    int sbufsize;
    int rbufsize;
    string hostname = "localhost"; // Default localhost

    static struct option options[] = {
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
            {"host",     required_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

    string mode_str = argv[1];//Read the mode from the command line
    optind = 2;

    if (mode_str == "-send") {
        mode = SEND;
        while ((opt = getopt_long(argc, argv, "s:r:p:c:z:a:n:u:", options, NULL)) != -1) {
            switch (opt) {
                case 's':
                    // cout << argv[optind-1] << endl;
                    stat = atoi(argv[optind]);
                    cout << "stat: " << stat << endl;
                    break;
                case 'r':
                    rhost = argv[optind];
                    cout << "rhost: " << rhost << endl;
                    break;
                case 'p':
                    rport = atoi(argv[optind]);
                    cout << "rport: " << rport << endl;
                    break;
                case 'c':
                    proto = argv[optind];
                    cout << "proto: " << proto << endl;
                    break;
                case 'z':
                    pktsize = atoi(argv[optind]);
                    cout << "pktsize: " << pktsize << endl;
                    break;
                case 'a':
                    pktrate = atoi(argv[optind]);
                    cout << "pktrate: " << pktrate << endl;
                    break;
                case 'n':
                    pktnum = atoi(argv[optind]);
                    cout << "pktnum: " << pktnum << endl;
                    break;
                case 'u':  sbufsize = atoi(argv[optind]);
                    cout << "sbufsize: " << sbufsize << endl;
                    break;
                default:
                    cout << "Invalid send option" << endl;
                    return EXIT_FAILURE;
            }
        }
        TCP_socket(mode, rhost, rport);
    } else if (mode_str == "-recv") {
        mode = RECV;
        while ((opt = getopt_long(argc, argv, "s:l:q:c:k:e:", options, NULL)) != -1) {
            switch (opt) {
                case 's':
                    stat = atoi(argv[optind]);
                    cout << "stat: " << stat << endl;
                    break;
                case 'l':
                    lhost = argv[optind];
                    cout << "lhost: " << lhost << endl;
                    break;
                case 'q':
                    lport = atoi(argv[optind]);
                    cout << "lport: " << lport << endl;
                    break;
                case 'c':
                    proto = argv[optind];
                    cout << "proto: " << proto << endl;
                    break;
                case 'k':
                    pktsize = atoi(argv[optind]);
                    cout << "pktsize: " << pktsize << endl;
                    break;
                case 'e':
                    rbufsize = atoi(argv[optind]);
                    cout << "rbufsize: " << rbufsize << endl;
                    break;
                default:
                    cout << "Invalid recv option" << endl;
                    return EXIT_FAILURE;
            }
        }
    } else if (mode_str == "-host") {
        mode = HOST;
         while ((opt = getopt_long(argc, argv, "h:", options, NULL)) != -1) {
            switch (opt) {
                case 'h':
                    rhost = argv[optind];
                    break;
                default:
                    cout << "Invalid host option" << endl;
                    return EXIT_FAILURE;
                }
            }
    } else {
        cout << "Invalid mode" << endl;
        return EXIT_FAILURE;
    }
}