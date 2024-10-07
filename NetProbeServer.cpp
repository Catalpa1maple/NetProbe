#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "tinythread.h"
using namespace tthread;

int main(int argc, char *argv[]){
    int subfsize;
    int rbufsize;
    std::string lhost;
    int lport = 4180;
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
                    break;
                case 'b':
                    std::cout << "lport: " << optarg << std::endl;
                    break;
                case 'c':
                    std::cout << "sbufsize: " << optarg << std::endl;
                    break;
                case 'd':
                    std::cout << "rbufsize: " << optarg << std::endl;
                    break;
                default:
                    std::cout << "Invalid option" << std::endl;
                    return EXIT_FAILURE;
}
}
}