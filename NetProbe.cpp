#include <iostream>
#include <unistd.h>
#include <string>
#include <cstring>
#include <getopt.h>
#include "TCP_socket.cpp"
#include "UDP_socket.cpp"

using namespace std;  
enum mode {NONE,SEND,RECV,HOST};

int main(int argc, char *argv[]) {
    int opt;
    enum mode mode = NONE;
    int stat_interval = 500; // Default 500 ms
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

    static struct option long_options[] = {
        {"send", required_argument, 0, 's'},
        {"recv", required_argument, 0, 'r'},
        {"host", required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    while ((opt = getopt_long(argc, argv, "s:r:h:", long_options, NULL)) != -1 && mode == NONE) {
        switch (opt) {
            case 's':
                cout << "send" << endl;
                // cout << char(opt) << endl;
                mode = SEND;
                break;
            case 'r':
                cout << "recv" << endl;
                mode = RECV;
                break;
            case 'h':
                // cout << "host" << endl;
                mode = HOST;
                break;
            default:
                cout << "Please Select Mode" << endl;
                return EXIT_FAILURE;
        }
    }optind = 7;// Stop getopt()
    cout << argv[2] << endl;
    if (mode == SEND) {
        static struct option send_options[] = {
            {"stat", required_argument, 0, 't'},
            {"rhost", required_argument, 0, 'o'},
            {"rport", required_argument, 0, 'q'},
            {"proto", required_argument, 0, 'p'},
            {"pktsize", required_argument, 0, 'k'},
            {"pktrate", required_argument, 0, 'x'},
            {"pktnum", required_argument, 0, 'n'},
            {"sbufsize", required_argument, 0, 'b'},
            {0, 0, 0, 0}
        };

        while ((opt = getopt_long(argc, argv, "t:o:q:p:k:x:n:b:", send_options, NULL)) != -1) {
            switch (opt) {
                case 't':
                    stat_interval = atoi(optarg);
                    // cout << int(stat_interval) << endl;
                    break;
                case 'o':
                    rhost = optarg;
                    break;
                case 'q':
                    rport = atoi(optarg);
                    break;
                case 'p':
                    proto = optarg;
                    break;
                case 'k':
                    pktsize = atoi(optarg);
                    break;
                case 'x':
                    pktrate = atoi(optarg);
                    break;
                case 'n':
                    pktnum = atoi(optarg);
                    break;
                case 'b':
                    sbufsize = atoi(optarg);
                    break;
                default:
                    cout << "Invalid send option" << endl;
                    return EXIT_FAILURE;
            }
        }


    } else if (mode == RECV) {
        static struct option recv_options[] = {
            {"stat", required_argument, 0, 't'},
            {"lhost", required_argument, 0, 'o'},
            {"lport", required_argument, 0, 'q'},
            {"proto", required_argument, 0, 'p'},
            {"pktsize", required_argument, 0, 'k'},
            {"rbufsize", required_argument, 0, 'b'},
            {0, 0, 0, 0}
        };
        while ((opt = getopt_long(argc, argv, "t:o:q:p:k:b:", recv_options, NULL)) != -1) {
            cout << "test" << endl;
            switch (opt) {
                case 't':
                    stat_interval = atoi(optarg);
                    cout << "test" << endl;
                    break;
                case 'o':
                    lhost = optarg;
                    break;
                case 'q':
                    lport = atoi(optarg);
                    break;
                case 'p':
                    proto = optarg;
                    break;
                case 'k':
                    pktsize = atoi(optarg);
                    break;
                case 'b':
                    rbufsize = atoi(optarg);
                    break;
                default:
                    cout << "Invalid recv option" << endl;
                    return EXIT_FAILURE;
            }
        }
    } else if (mode == HOST)
    {   static struct option host_options[] = {
            {"host", required_argument, 0, 'o'},
            {0, 0, 0, 0}
        };

        while ((opt = getopt_long(argc, argv, "o:", host_options, NULL)) != -1) {
            switch (opt) {
                case 'o':
                    rhost = optarg;
                    break;
                default:
                    cout << "Invalid host option" << endl;
                    return EXIT_FAILURE;
            }
        }

    }
    









}