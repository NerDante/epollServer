#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "epollServer.h"

// print recv data in hex format
void recv_data_print(int cliFd, const char *data, unsigned int len)
{
    printf("recv msg hex print:\n");
    raw_dump(data, len);
}

// echo callback, just send data back to client
void recv_echo_callback(int cliFd, const char *data, unsigned int len)
{
    send(cliFd, data, len, 0);
}

void udp_echo_callback(int fd, struct sockaddr_in *cliaddr, const char *data, unsigned int len)
{
    printf(">>client address = %s:%d\n", inet_ntoa(cliaddr->sin_addr), ntohs(cliaddr->sin_port));
    printf("Recv: %s\n", data);
    sendto(fd, data, len, 0, (struct sockaddr *)cliaddr,sizeof(struct sockaddr_in)); 
}

int main(int argc, char *argv[])
{
    epoll_server_t *evs;

    if(argc < 2){
       printf("Usage: %s [port]\n", argv[0]);
       return -1;                              
    }
    
#if 0
    evs = epoll_tcp_server_init(atoi(argv[1]), recv_echo_callback, 1024); //tcp
#else
    evs = epoll_udp_server_init(atoi(argv[1]), udp_echo_callback, 1024); //udp
#endif
    if(NULL == evs){
        printf("epoll_server_init failed.\n");
        return -1;
    }

    epoll_server_start(evs); // loop forever, unless exception is occured 
    epoll_server_delete(evs);

    return 0;
}

