#include "epollServer.h"
#include "buffer.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

typedef struct connection_t {
    int fd;
    VecBuff_t* recvbuf;
    VecBuff_t* sendbuf;
    pthread_mutex_t send_lock;
    time_t last_active;
} connection_t;

typedef struct job_t {
    int fd;
    char* data;
    unsigned int len;
    struct job_t* next;
} job_t;

struct epoll_server_t {
    int epfd;
    trans_protocol_t trans;
    int listen;
    int stopFlag;

    tcp_recv_callback tcp_handle;
    tcp_msg_callback tcp_msg_handle;
    tcp_msg_parser parser;
    udp_recv_callback udp_handle;
    connection_t** conns;
    int maxfd;
    int idle_timeout;
    pthread_t workers[4];
    int worker_num;
    struct {
        pthread_mutex_t lock;
        pthread_cond_t cond;
        job_t* head;
        job_t* tail;
    } jq;
};

static struct epoll_server_t* g_server = NULL;

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

static connection_t* connection_create(int fd)
{
    connection_t* conn = calloc(1, sizeof(connection_t));
    if (!conn) {
        return NULL;
    }
    conn->fd = fd;
    conn->recvbuf = vecbuf_create();
    if (!conn->recvbuf) {
        free(conn);
        return NULL;
    }
    conn->sendbuf = vecbuf_create();
    if (!conn->sendbuf) {
        vecbuf_free(conn->recvbuf);
        free(conn);
        return NULL;
    }
    pthread_mutex_init(&conn->send_lock, NULL);
    conn->last_active = time(NULL);
    return conn;
}

static void connection_free(connection_t* conn)
{
    if (!conn) return;
    if (conn->recvbuf) vecbuf_free(conn->recvbuf);
    if (conn->sendbuf) vecbuf_free(conn->sendbuf);
    pthread_mutex_destroy(&conn->send_lock);
    free(conn);
}

static void server_tcp_do_accept(epoll_server_t* server)
{
    int cliFd, ret;
    struct sockaddr_in cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event ev;

    while (1) {
        cliFd = accept(server->listen, (struct sockaddr*)&cliaddr, &socklen);
        if (cliFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("accept");
            break;
        }

        if (setnonblocking(cliFd) < 0) {
            perror("setnonblocking error");
            close(cliFd);
            continue;
        }

        connection_t* conn = connection_create(cliFd);
        if (!conn) {
            close(cliFd);
            continue;
        }

        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.ptr = conn;
        ret = epoll_ctl(server->epfd, EPOLL_CTL_ADD, cliFd, &ev);
        if (ret < 0) {
            perror("epoll_ctl add client");
            connection_free(conn);
            close(cliFd);
            continue;
        }
        if (cliFd < server->maxfd) {
            server->conns[cliFd] = conn;
        }
    }
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

static void enqueue_job(epoll_server_t* server, int fd, const char* data, unsigned int len)
{
    job_t* j = (job_t*)malloc(sizeof(job_t));
    if (!j) return;
    j->fd = fd;
    j->len = len;
    j->data = (char*)malloc(len);
    if (!j->data) { free(j); return; }
    memcpy(j->data, data, len);
    j->next = NULL;
    pthread_mutex_lock(&server->jq.lock);
    if (server->jq.tail) {
        server->jq.tail->next = j;
        server->jq.tail = j;
    } else {
        server->jq.head = server->jq.tail = j;
    }
    pthread_cond_signal(&server->jq.cond);
    pthread_mutex_unlock(&server->jq.lock);
}

static job_t* dequeue_job(epoll_server_t* server)
{
    job_t* j = NULL;
    pthread_mutex_lock(&server->jq.lock);
    while (!server->jq.head && !server->stopFlag) {
        pthread_cond_wait(&server->jq.cond, &server->jq.lock);
    }
    if (server->jq.head) {
        j = server->jq.head;
        server->jq.head = j->next;
        if (!server->jq.head) server->jq.tail = NULL;
    }
    pthread_mutex_unlock(&server->jq.lock);
    return j;
}

static void* worker_loop(void* arg)
{
    epoll_server_t* server = (epoll_server_t*)arg;
    while (!server->stopFlag) {
        job_t* j = dequeue_job(server);
        if (!j) continue;
        if (server->tcp_msg_handle) {
            server->tcp_msg_handle(j->fd, j->data, j->len);
        }
        free(j->data);
        free(j);
    }
    return NULL;
}

static void handle_client_write(connection_t* conn, epoll_server_t* server)
{
    struct epoll_event ev;
    int sent;
    while (1) {
        pthread_mutex_lock(&conn->send_lock);
        if (conn->sendbuf->used == 0) {
            pthread_mutex_unlock(&conn->send_lock);
            break;
        }
        sent = send(conn->fd, conn->sendbuf->data, conn->sendbuf->used, 0);
        if (sent > 0) {
            vecbuf_consume_head(conn->sendbuf, (unsigned int)sent);
            conn->last_active = time(NULL);
            pthread_mutex_unlock(&conn->send_lock);
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pthread_mutex_unlock(&conn->send_lock);
            break;
        }
        pthread_mutex_unlock(&conn->send_lock);
        ev.data.ptr = conn;
        epoll_ctl(server->epfd, EPOLL_CTL_DEL, conn->fd, &ev);
        close(conn->fd);
        if (conn->fd < server->maxfd) server->conns[conn->fd] = NULL;
        connection_free(conn);
        break;
    }
    pthread_mutex_lock(&conn->send_lock);
    if (conn->sendbuf->used == 0) {
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.ptr = conn;
        epoll_ctl(server->epfd, EPOLL_CTL_MOD, conn->fd, &ev);
    }
    pthread_mutex_unlock(&conn->send_lock);
}

static void handle_client_message(connection_t* conn, epoll_server_t* server)
{
    char recvbuf[BUFF_BASE_SIZE];
    int len;
    struct epoll_event ev;
    unsigned int msg_len;

    while (1) {
        len = recv(conn->fd, recvbuf, sizeof(recvbuf), 0);
        if (len > 0) {
            vecbuf_add_tail(conn->recvbuf, recvbuf, len);
            conn->last_active = time(NULL);
            if (server->parser && server->tcp_msg_handle) {
                while (1) {
                    if (server->parser(conn->recvbuf->data, conn->recvbuf->used, &msg_len) != 0) {
                        break;
                    }
                    if (msg_len == 0 || msg_len > conn->recvbuf->used) {
                        break;
                    }
                    enqueue_job(server, conn->fd, conn->recvbuf->data, msg_len);
                    vecbuf_consume_head(conn->recvbuf, msg_len);
                }
            } else if (server->tcp_handle) {
                server->tcp_handle(conn->fd, recvbuf, (unsigned int)len);
            }
            continue;
        } else if (len == 0) {
            ev.data.ptr = conn;
            epoll_ctl(server->epfd, EPOLL_CTL_DEL, conn->fd, &ev);
            close(conn->fd);
            if (conn->fd < server->maxfd) server->conns[conn->fd] = NULL;
            connection_free(conn);
            printf("a client disconnect, fd = %d.\n", conn->fd);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            printf("recv error:%s\n", strerror(errno));
            ev.data.ptr = conn;
            epoll_ctl(server->epfd, EPOLL_CTL_DEL, conn->fd, &ev);
            close(conn->fd);
            if (conn->fd < server->maxfd) server->conns[conn->fd] = NULL;
            connection_free(conn);
            break;
        }
    }
}

epoll_server_t* epoll_tcp_server_init(unsigned port, tcp_recv_callback handler, int max_client)
{
    epoll_server_t* evs = NULL;
    int ret = 0;
    struct epoll_event ev;

    signal(SIGPIPE, SIG_IGN);

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
    evs->tcp_msg_handle = NULL;
    evs->parser = NULL;
    evs->maxfd = 65536;
    evs->conns = (connection_t**)calloc(evs->maxfd, sizeof(connection_t*));
    evs->idle_timeout = 120;
    evs->listen = create_tcp_listen(port);
    if (evs->listen < 0) {
        printf("listen and bind.\n");
        goto Error1;
    }
    g_server = evs;

    ev.events = EPOLLIN | EPOLLET;
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

    signal(SIGPIPE, SIG_IGN);

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
    evs->maxfd = 65536;
    evs->conns = (connection_t**)calloc(evs->maxfd, sizeof(connection_t*));
    evs->idle_timeout = 120;
    evs->listen = create_udp_listen(port);
    if (evs->listen < 0) {
        printf("listen and bind.\n");
        goto Error1;
    }
    g_server = evs;

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

epoll_server_t* epoll_tcp_server_init_ex(unsigned port, tcp_msg_callback msg_handler, tcp_msg_parser parser, int max_client)
{
    epoll_server_t* evs = NULL;
    int ret = 0;
    struct epoll_event ev;

    signal(SIGPIPE, SIG_IGN);

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
    evs->tcp_handle = NULL;
    evs->tcp_msg_handle = msg_handler;
    evs->parser = parser;
    evs->maxfd = 65536;
    evs->conns = (connection_t**)calloc(evs->maxfd, sizeof(connection_t*));
    evs->idle_timeout = 120;
    evs->listen = create_tcp_listen(port);
    if (evs->listen < 0) {
        printf("listen and bind.\n");
        goto Error1;
    }
    g_server = evs;
    pthread_mutex_init(&evs->jq.lock, NULL);
    pthread_cond_init(&evs->jq.cond, NULL);
    evs->jq.head = evs->jq.tail = NULL;
    evs->worker_num = 4;
    for (int i = 0; i < evs->worker_num; ++i) {
        pthread_create(&evs->workers[i], NULL, worker_loop, evs);
    }

    ev.events = EPOLLIN | EPOLLET;
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

static void server_maintain(epoll_server_t* server)
{
    time_t now = time(NULL);
    for (int fd = 0; fd < server->maxfd; ++fd) {
        connection_t* conn = server->conns[fd];
        if (!conn) continue;
        if (now - conn->last_active > server->idle_timeout) {
            struct epoll_event ev;
            ev.data.ptr = conn;
            epoll_ctl(server->epfd, EPOLL_CTL_DEL, conn->fd, &ev);
            close(conn->fd);
            server->conns[fd] = NULL;
            connection_free(conn);
        }
    }
}

void epoll_server_start(epoll_server_t* server)
{
    int ready;
    struct epoll_event events[MAXEPOLLSIZE];

    while (1) {
        if (server->stopFlag) {
            printf("server's stopFlag is set to true, exit\n");
            break;
        }
        ready = epoll_wait(server->epfd, events, MAXEPOLLSIZE, 1000);
        if (ready < 0) {
            perror("epoll_wait.");
            return;
        } else if (ready == 0) {
            /* timeout, no ready */
            server_maintain(server);
            continue;
        } else {
            for (int i = 0; i < ready; i++) {
                if (events[i].data.fd == server->listen) {
                    if (server->trans == TRANS_PTCP) {
                        server_tcp_do_accept(server);
                    } else {
                        server_udp_handle(server);
                    }
                } else {
                    connection_t* conn = (connection_t*)events[i].data.ptr;
                    if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                        struct epoll_event ev;
                        ev.data.ptr = conn;
                        epoll_ctl(server->epfd, EPOLL_CTL_DEL, conn->fd, &ev);
                        close(conn->fd);
                        if (conn->fd < server->maxfd) server->conns[conn->fd] = NULL;
                        connection_free(conn);
                        continue;
                    }
                    if (events[i].events & EPOLLIN) {
                        handle_client_message(conn, server);
                    }
                    if (events[i].events & EPOLLOUT) {
                        handle_client_write(conn, server);
                    }
                }
            }
            server_maintain(server);
        }
    }

}

void epoll_server_delete(epoll_server_t* server)
{
    if (NULL == server) {
        return;
    }
    server->stopFlag = 1;
    pthread_cond_broadcast(&server->jq.cond);
    for (int i = 0; i < server->worker_num; ++i) {
        pthread_join(server->workers[i], NULL);
    }
    pthread_mutex_destroy(&server->jq.lock);
    pthread_cond_destroy(&server->jq.cond);
    for (int fd = 0; fd < server->maxfd; ++fd) {
        connection_t* conn = server->conns[fd];
        if (conn) {
            struct epoll_event ev;
            ev.data.ptr = conn;
            epoll_ctl(server->epfd, EPOLL_CTL_DEL, conn->fd, &ev);
            close(conn->fd);
            connection_free(conn);
            server->conns[fd] = NULL;
        }
    }
    free(server->conns);
    close(server->epfd);
    free(server);
}

void epoll_server_stop(epoll_server_t* server)
{
    server->stopFlag = 1;
}

int epoll_server_send(int fd, const char* data, unsigned int len)
{
    if (!g_server || !data || len == 0) return -1;
    if (fd < 0 || fd >= g_server->maxfd) return -1;
    connection_t* conn = g_server->conns[fd];
    if (!conn) return -1;
    pthread_mutex_lock(&conn->send_lock);
    int ret = vecbuf_add_tail(conn->sendbuf, data, len);
    conn->last_active = time(NULL);
    pthread_mutex_unlock(&conn->send_lock);
    if (ret < 0) return ret;
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLOUT;
    ev.data.ptr = conn;
    epoll_ctl(g_server->epfd, EPOLL_CTL_MOD, conn->fd, &ev);
    return 0;
}
