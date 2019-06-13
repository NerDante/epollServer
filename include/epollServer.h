#ifndef __EPOLLSERVER_H__
#define __EPOLLSERVER_H__

#define MAXEPOLLSIZE 10000
#define MAXLINE 10240

typedef void (*tcp_recv_callback)(int, const char*, unsigned int);
typedef void (*udp_recv_callback)(int, struct sockaddr_in*, const char*, unsigned int);

typedef enum {
    TRANS_PTCP = 0,
    TRANS_PUDP = 1,
} trans_protocol_t;

typedef struct epoll_server_t {
    int epfd;
    trans_protocol_t trans;
    int listen;

    tcp_recv_callback tcp_handle;
    udp_recv_callback udp_handle;
} epoll_server_t;

int raw_dump(const char* buff, int len);
epoll_server_t* epoll_tcp_server_init(unsigned port, tcp_recv_callback handler, int max_client);
epoll_server_t* epoll_udp_server_init(unsigned port, udp_recv_callback handler, int max_client);
void epoll_server_start(epoll_server_t* server);
void epoll_server_delete(epoll_server_t* server);

#endif //__EPOLLSERVER_H__
