#include "epollServer.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LISTEN_PORTS_NUM 5
unsigned short ports[LISTEN_PORTS_NUM] = { 5670, 5671, 5672, 5673, 5674 };

// echo callback, just send data back to client
void recv_echo_callback(int cliFd, const char* data, unsigned int len)
{
    send(cliFd, data, len, 0);
}

void* tcp_server_run(void* data)
{
    epoll_server_t* evs;
    unsigned short* port = data;

    evs = epoll_tcp_server_init(*port, recv_echo_callback, 1024);
    if (NULL == evs) {
        printf("epoll_server_init failed.\n");
        return NULL;
    }
    printf("listen port %hu ...\n", *port);
    epoll_server_start(evs); // loop forever, unless exception is occured
    epoll_server_delete(evs);

    return NULL;
}

int create_server_thread(unsigned short* port)
{
    pthread_t tid;
    int ret;

    ret = pthread_create(&tid, NULL, tcp_server_run, port);
    if (ret < 0) {
        printf("create server thread fail, ret = %d\n", ret);
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    for (int i = 0; i < LISTEN_PORTS_NUM; i++) {
        create_server_thread(&ports[i]);
    }

    while (1) {
        sleep(1);
    }

    return 0;
}
