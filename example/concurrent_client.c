#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char ip[64];
    unsigned short port;
    int connections;
    int messages_per_conn;
    int payload_len;
} bench_config_t;

typedef struct {
    bench_config_t* cfg;
    int start_index;
    int count;
    long long success;
    long long fail;
} thread_arg_t;

static long long now_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int connect_one(const char* ip, unsigned short port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void* worker(void* arg)
{
    thread_arg_t* t = (thread_arg_t*)arg;
    bench_config_t* cfg = t->cfg;
    char* buf = (char*)malloc(cfg->payload_len);
    char* rbuf = (char*)malloc(cfg->payload_len);
    if (!buf || !rbuf) {
        return NULL;
    }
    memset(buf, 'A', cfg->payload_len - 1);
    buf[cfg->payload_len - 1] = '\n';

    for (int i = 0; i < t->count; ++i) {
        int fd = connect_one(cfg->ip, cfg->port);
        if (fd < 0) {
            t->fail += cfg->messages_per_conn;
            continue;
        }
        for (int m = 0; m < cfg->messages_per_conn; ++m) {
            int left = cfg->payload_len;
            int off = 0;
            while (left > 0) {
                int n = send(fd, buf + off, left, 0);
                if (n <= 0) {
                    left = -1;
                    break;
                }
                off += n;
                left -= n;
            }
            if (left < 0) {
                t->fail++;
                break;
            }
            int recv_len = 0;
            while (recv_len < cfg->payload_len) {
                int n = recv(fd, rbuf + recv_len, cfg->payload_len - recv_len, 0);
                if (n < 0) {
                    t->fail++;
                    recv_len = -1;
                    break;
                }
                if (n == 0) {
                    t->fail++;
                    recv_len = -1;
                    break;
                }
                recv_len += n;
            }
            if (recv_len == cfg->payload_len) {
                t->success++;
            }
        }
        close(fd);
    }

    free(buf);
    free(rbuf);
    return NULL;
}

static void usage(const char* prog)
{
    printf("%s ip port connections messages_per_conn payload_len\n", prog);
    printf("example: %s 127.0.0.1 9000 100 100 64\n", prog);
}

int main(int argc, char* argv[])
{
    if (argc != 6) {
        usage(argv[0]);
        return -1;
    }
    bench_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.ip, argv[1], sizeof(cfg.ip) - 1);
    cfg.port = (unsigned short)atoi(argv[2]);
    cfg.connections = atoi(argv[3]);
    cfg.messages_per_conn = atoi(argv[4]);
    cfg.payload_len = atoi(argv[5]);
    if (cfg.connections <= 0 || cfg.messages_per_conn <= 0 || cfg.payload_len <= 1) {
        usage(argv[0]);
        return -1;
    }

    int threads = 4;
    if (cfg.connections < threads) {
        threads = cfg.connections;
    }

    pthread_t* tids = (pthread_t*)malloc(sizeof(pthread_t) * threads);
    thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t) * threads);
    if (!tids || !args) {
        return -1;
    }

    int base = 0;
    for (int i = 0; i < threads; ++i) {
        args[i].cfg = &cfg;
        args[i].start_index = base;
        int left = cfg.connections - base;
        int cnt = left / (threads - i);
        if (cnt <= 0) cnt = left;
        args[i].count = cnt;
        args[i].success = 0;
        args[i].fail = 0;
        base += cnt;
    }

    long long start = now_ms();
    for (int i = 0; i < threads; ++i) {
        pthread_create(&tids[i], NULL, worker, &args[i]);
    }
    long long total_success = 0;
    long long total_fail = 0;
    for (int i = 0; i < threads; ++i) {
        pthread_join(tids[i], NULL);
        total_success += args[i].success;
        total_fail += args[i].fail;
    }
    long long end = now_ms();
    long long total_req = (long long)cfg.connections * cfg.messages_per_conn;
    double seconds = (end - start) / 1000.0;
    double qps = seconds > 0 ? total_success / seconds : 0.0;

    printf("connections: %d\n", cfg.connections);
    printf("messages_per_conn: %d\n", cfg.messages_per_conn);
    printf("payload_len: %d\n", cfg.payload_len);
    printf("total_requests: %lld\n", total_req);
    printf("success: %lld, fail: %lld\n", total_success, total_fail);
    printf("time: %.3f s, qps: %.2f\n", seconds, qps);

    free(tids);
    free(args);
    return 0;
}

