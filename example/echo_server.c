#include "epollServer.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

void recv_data_print(int cliFd, const char* data, unsigned int len)
{
    printf("recv msg hex print:\n");
    raw_dump(data, len);
}

void recv_echo_callback(int cliFd, const char* data, unsigned int len)
{
    send(cliFd, data, len, 0);
}

int line_parser(const char* data, unsigned int len, unsigned int* msg_len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
        if (data[i] == '\n') {
            *msg_len = i + 1;
            return 0;
        }
    }
    return -1;
}

void recv_echo_msg_callback(int cliFd, const char* data, unsigned int len)
{
    epoll_server_send(cliFd, data, len);
}

void udp_echo_callback(int fd, struct sockaddr_in* cliaddr, const char* data, unsigned int len)
{
    printf(">>client address = %s:%d\n", inet_ntoa(cliaddr->sin_addr), ntohs(cliaddr->sin_port));
    printf("Recv: %s\n", data);
    sendto(fd, data, len, 0, (struct sockaddr*)cliaddr, sizeof(struct sockaddr_in));
}

int main(int argc, char* argv[])
{
    epoll_server_t* evs;

    if (argc < 2) {
        printf("Usage: %s [port]\n", argv[0]);
        return -1;
    }

    evs = epoll_tcp_server_init_ex(atoi(argv[1]), recv_echo_msg_callback, line_parser, 1024);
    if (NULL == evs) {
        printf("epoll_server_init failed.\n");
        return -1;
    }

    epoll_server_start(evs); // loop forever, unless exception is occured
    epoll_server_delete(evs);

    return 0;
}
