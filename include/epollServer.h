#ifndef __EPOLLSERVER_H__
#define __EPOLLSERVER_H__

#define    MAXEPOLLSIZE      10000
#define    MAXLINE 10240

typedef void(*recv_callback)(int, const char *, unsigned int); 

typedef struct epoll_server_t
{
    int epfd;
    int listen;

    recv_callback recv_handle;
}epoll_server_t;
int raw_dump(const char *buff, int len);
epoll_server_t *epoll_server_init(unsigned port, recv_callback handler, int max_client);
void epoll_server_start(epoll_server_t *server);
void epoll_server_delete(epoll_server_t *server);

#endif //__EPOLLSERVER_H__