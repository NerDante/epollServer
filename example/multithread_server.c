#include "epollServer.h"
#include <arpa/inet.h>
#include <bits/types/sigset_t.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LISTEN_PORTS_NUM 5
unsigned short ports[LISTEN_PORTS_NUM] = { 5670, 5671, 5672, 5673, 5674 };
sigset_t mask;

// echo callback, just send data back to client
void recv_echo_callback(int cliFd, const char* data, unsigned int len)
{
    int ret;
    ret = send(cliFd, data, len, 0);
    printf("ret = %d\n", ret);
    sleep(3);
    ret = send(cliFd, data, len, 0);
    printf("ret = %d, errno = %d\n", ret, errno);
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

void* sig_thread_catcher(void* arg)
{
    sigset_t* set = arg;
    int ret, sig;

    printf("sig catcher...\n");
    for (;;) {
        printf("block...\n");
        ret = sigwait(set, &sig);
        if (ret != 0) {
            printf("hshsssh\n");
            exit(1);
        }
        printf("signal %d catched\n", sig);
    }
}

void catch_signal()
{
    int ret;
    pthread_t tid;

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGUSR1);
    ret = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (ret != 0) {
        printf("error in pthread_mask\n");
    }

    ret = pthread_create(&tid, NULL, &sig_thread_catcher, &mask);
    if (ret != 0) {
        printf("create thread fail\n");
    }
}
static void*
sig_thread(void* arg)
{
    sigset_t* set = arg;
    int s, sig;

    for (;;) {
        s = sigwait(set, &sig);
        if (s != 0)
            exit(-1);
        printf("Signal handling thread got signal %d\n", sig);
    }
}

int main(int argc, char* argv[])
{
    pthread_t thread;
    sigset_t set;
    int s;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGPIPE);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (s != 0)
        exit(1);

    s = pthread_create(&thread, NULL, &sig_thread, &set);
    if (s != 0)
        exit(1);

    for (int i = 0; i < LISTEN_PORTS_NUM; i++) {
        create_server_thread(&ports[i]);
    }

    while (1) {
        sleep(1);
    }

    return 0;
}
