#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <string>

enum Mode { SEND, RECV };

void TCP_socket(int mode, const std::string& rhost, int rport);

#endif