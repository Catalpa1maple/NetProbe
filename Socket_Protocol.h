#ifndef Socket_Protocol
#define Socket_Protocol
#include <string>
enum mode {NONE,SEND,RECV,HOST};

void TCP_socket(int mode,int stat, const std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize);
void UDP_socket(int mode,int stat, const std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize);
#endif