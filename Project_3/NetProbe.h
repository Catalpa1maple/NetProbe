
#ifndef NETPROBE_H
#define NETPROBE_H

#include <vector>
#include <stdlib.h>
#include "pipe.h"
#define QUEUE_SIZE 10
#define infinite 1000000000
using namespace std;
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

typedef struct {
    void (*function) (void*);
    void *argument;
} Task;

typedef struct {
    Task task_queue[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    mtx_t lock;
    cnd_t notify;
    vector<thrd_t> threads; //For dynamic thread creation
    int stop;
} ThreadPool;

int thread_do_work(void *arg);
void threadpool_add_task(ThreadPool *pool, void
(*function)(void*), void *argument);
void threadpool_init(ThreadPool *pool);
void threadpool_destroy(ThreadPool *pool);
void handle_client(void *arg);

#endif // NETPROBE_H


