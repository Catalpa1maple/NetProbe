#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <string>

enum Mode { SEND, RECV };

void TCP_socket(int mode,int stat, const std::string& host, int port, int pktsize, int pktrate, int pktnum, int bufsize);

#endif