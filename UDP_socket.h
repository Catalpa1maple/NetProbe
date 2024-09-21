#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H


#include <string>

enum Mode { SEND, RECV };

void UDP_socket(Mode mode, const std::string& rhost, int rport);

#endif