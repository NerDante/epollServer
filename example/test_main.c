#include <stdio.h>
#include <stdlib.h>
#include "epollServer.h"

int main(int argc, char *argv[])
{
    epoll_server_t *evs;

    if(argc < 2){
       printf("Usage: %s [port]\n", argv[0]);
       return -1;
                                    
    }

    evs = epoll_server_init(atoi(argv[1]), recv_echo_callback, 1024);
    if(NULL == evs){
        printf("epoll_server_init failed.\n");
        return -1;
    }

    epoll_server_start(evs);
    epoll_server_delete(evs);

    return 0;
}

