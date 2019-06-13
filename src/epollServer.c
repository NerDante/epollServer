#include "epollServer.h"
#include "buffer.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int raw_dump(const char* buff, int len)
{
    int i = 0;

    if (NULL == buff) {
        printf("NULL param\n");
        return -1;
    }

    for (i = 0; i < len; i++) {
        if (i != 0 && i % 16 == 0) {
            printf("\n");
        }
        printf("%02x ", buff[i]);
    }
    printf("\n");

    return 0;
}

int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1) {
        printf("set nonblock fail\n");
        return -1;
    }
    return 0;
}

int create_tcp_listen(short port)
{
    int sockFd, ret;
    struct sockaddr_in seraddr;

    bzero(&seraddr, sizeof(struct sockaddr_in));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = INADDR_ANY;
    seraddr.sin_port = htons(port);

    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        perror("socket error");
        return -1;
    }

    if (setnonblocking(sockFd) < 0) {
        perror("setnonblock error");
    }

    ret = bind(sockFd, (struct sockaddr*)&seraddr, sizeof(struct sockaddr));
    if (ret < 0) {
        perror("bind");
        return -1;
    }

    ret = listen(sockFd, 1000);
    if (ret < 0) {
        perror("listen");
        return -1;
    }

    return sockFd;
}

int create_udp_listen(short port)
{
    int sockFd, ret;
    struct sockaddr_in seraddr;

    bzero(&seraddr, sizeof(struct sockaddr_in));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = INADDR_ANY;
    seraddr.sin_port = htons(port);

    sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0) {
        perror("socket error");
        return -1;
    }

    if (setnonblocking(sockFd) < 0) {
        perror("setnonblock error");
    }

    ret = bind(sockFd, (struct sockaddr*)&seraddr, sizeof(struct sockaddr));
    if (ret < 0) {
        perror("bind");
        return -1;
    }

    return sockFd;
}

void server_tcp_do_accept(int listenFd, int epfd)
{
    int cliFd;
    struct sockaddr_in cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event ev;

    cliFd = accept(listenFd, (struct sockaddr*)&cliaddr, &socklen);
    if (cliFd < 0) {
        perror("accept");
        return;
    }

    if (setnonblocking(cliFd) < 0) {
        perror("setnonblocking error");
        return;
    }

    ev.events = EPOLLIN;
    ev.data.fd = cliFd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, cliFd, &ev);
}

void server_udp_handle(epoll_server_t* server)
{
    char recvbuf[65536] = { 0 }; //65536
    int len;
    struct sockaddr_in clientaddr;
    socklen_t clilen = sizeof(struct sockaddr);

    len = recvfrom(server->listen, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&clientaddr, &clilen);
    if (len > 0) {
        server->udp_handle(server->listen, &clientaddr, recvbuf, len);
    }
}

void handle_client_message(int fd, epoll_server_t* server)
{
    VecBuff_t* vecBuf;
    char recvbuf[BUFF_BASE_SIZE];
    int len;
    struct epoll_event ev;

    vecBuf = vecbuf_init();
    if (NULL == vecBuf) {
        printf("vec_buff_init fail\n");
        return;
    }

    while ((len = recv(fd, recvbuf, sizeof(recvbuf), 0)) > 0) {
        vecbuf_add_tail(vecBuf, recvbuf, len);
    }

    if (len == 0) { //client closed.
        ev.data.fd = fd;
        epoll_ctl(server->epfd, EPOLL_CTL_DEL, fd, &ev);
        close(fd);
        printf("a client disconnect, fd = %d.\n", fd);

    } else if (len < 0 && errno != EAGAIN) {
        printf("recv error:%s\n", strerror(errno));
    }

    //handle the data
    if (vecBuf->used > 0) {
        server->tcp_handle(fd, vecBuf->data, vecBuf->used);
    }

    vecbuf_free(vecBuf);
}

epoll_server_t* epoll_tcp_server_init(unsigned port, tcp_recv_callback handler, int max_client)
{
    epoll_server_t* evs = NULL;
    int ret = 0;
    struct epoll_event ev;

    evs = calloc(1, sizeof(epoll_server_t));
    if (NULL == evs) {
        printf("calloc fail.\n");
        goto Error0;
    }

    evs->epfd = epoll_create(MAXEPOLLSIZE);
    if (evs->epfd < 0) {
        perror("epoll create");
        goto Error1;
    }

    evs->trans = TRANS_PTCP;
    evs->tcp_handle = handler;
    evs->listen = create_tcp_listen(port);
    if (evs->listen < 0) {
        printf("listen and bind.\n");
        goto Error1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = evs->listen;
    ret = epoll_ctl(evs->epfd, EPOLL_CTL_ADD, evs->listen, &ev);
    if (ret < 0) {
        perror("epoll_ctl");
        goto Error1;
    }

    return evs;

Error1:
    free(evs);
Error0:
    return NULL;
}

epoll_server_t* epoll_udp_server_init(unsigned port, udp_recv_callback handler, int max_client)
{
    epoll_server_t* evs = NULL;
    int ret = 0;
    struct epoll_event ev;

    evs = calloc(1, sizeof(epoll_server_t));
    if (NULL == evs) {
        printf("calloc fail.\n");
        goto Error0;
    }

    evs->epfd = epoll_create(MAXEPOLLSIZE);
    if (evs->epfd < 0) {
        perror("epoll create");
        goto Error1;
    }

    evs->trans = TRANS_PUDP;
    evs->udp_handle = handler;
    evs->listen = create_udp_listen(port);
    if (evs->listen < 0) {
        printf("listen and bind.\n");
        goto Error1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = evs->listen;
    ret = epoll_ctl(evs->epfd, EPOLL_CTL_ADD, evs->listen, &ev);
    if (ret < 0) {
        perror("epoll_ctl");
        goto Error1;
    }

    return evs;

Error1:
    free(evs);
Error0:
    return NULL;
}

void epoll_server_start(epoll_server_t* server)
{
    int ready;
    struct epoll_event events[MAXEPOLLSIZE];

    while (1) {
        ready = epoll_wait(server->epfd, events, MAXEPOLLSIZE, 1000);
        if (ready < 0) {
            perror("epoll_wait.");
            return;
        } else if (ready == 0) {
            /* timeout, no ready */
            continue;
        } else {
            for (int i = 0; i < ready; i++) {
                if (events[i].data.fd == server->listen) {
                    if (server->trans == TRANS_PTCP) {
                        server_tcp_do_accept(server->listen, server->epfd);
                    } else {
                        server_udp_handle(server);
                    }
                } else {
                    handle_client_message(events[i].data.fd, server);
                }
            }
        }
    }
}

void epoll_server_delete(epoll_server_t* server)
{
    if (NULL == server) {
        return;
    }
    close(server->epfd);
    free(server);
}
