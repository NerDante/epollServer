#ifndef __EPOLLSERVER_H__
#define __EPOLLSERVER_H__
#include <netinet/in.h>
#include "util.h"

#define MAXEPOLLSIZE 10000
#define MAXLINE 10240

typedef void (*tcp_recv_callback)(int, const char*, unsigned int);
typedef void (*tcp_msg_callback)(int, const char*, unsigned int);

typedef int (*tcp_msg_parser)(const char* data, unsigned int len, unsigned int* msg_len);
typedef void (*udp_recv_callback)(int, struct sockaddr_in*, const char*, unsigned int);

typedef enum {
    TRANS_PTCP = 0,
    TRANS_PUDP = 1,
} trans_protocol_t;

typedef struct epoll_server_t  epoll_server_t;

epoll_server_t* epoll_tcp_server_init(unsigned port, tcp_recv_callback handler, int max_client);
epoll_server_t* epoll_tcp_server_init_ex(unsigned port, tcp_msg_callback msg_handler, tcp_msg_parser parser, int max_client);
int epoll_server_send(int fd, const char* data, unsigned int len);
epoll_server_t* epoll_udp_server_init(unsigned port, udp_recv_callback handler, int max_client);
void epoll_server_start(epoll_server_t* server);
void epoll_server_delete(epoll_server_t* server);
void epoll_server_stop(epoll_server_t* server);

#endif //__EPOLLSERVER_H__
