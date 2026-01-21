# Simple Epoll-based TCP/UDP Server

A simple high-concurrency TCP/UDP server library based on Linux epoll, implemented in pure C.

Current implementation supports:
- TCP/UDP server abstraction (listen, accept, epoll loop)
- Connection-level context (per-connection recv/send buffers)
- Pluggable message parser (line-based `\n` protocol example)
- Worker thread pool to process business logic
- Send queue + EPOLLOUT based non-blocking writes with basic backpressure
- Connection idle timeout and graceful shutdown

Linux only.

## Build

From project root:

```bash
make
make install
```

The static library will be installed into `install/lib`, headers are under `src/`.

## Core APIs Overview

Header: [epollServer.h](file:///home/lyq/code/epollServer/src/epollServer.h)

- Initialize TCP server (legacy mode: callback per recv chunk):

```c
typedef void (*tcp_recv_callback)(int fd, const char* data, unsigned int len);

epoll_server_t* epoll_tcp_server_init(
    unsigned port,
    tcp_recv_callback handler,
    int max_client
);
```

- Initialize TCP server (recommended: message-based mode):

```c
typedef void (*tcp_msg_callback)(int fd, const char* data, unsigned int len);
typedef int (*tcp_msg_parser)(const char* data, unsigned int len, unsigned int* msg_len);

epoll_server_t* epoll_tcp_server_init_ex(
    unsigned port,
    tcp_msg_callback msg_handler,
    tcp_msg_parser parser,
    int max_client
);
```

`parser` is responsible for extracting one complete message from the connection buffer:
- `data` / `len`: bytes currently buffered but not yet consumed.
- return value: `0` means one complete message is parsed; non-zero means not enough data or parse error.
- `*msg_len`: length of the parsed message (must be `<= len`).

On successful parse the library will:
- call `msg_handler(fd, data, msg_len)` with that message;
- consume these bytes from its internal buffer, then try to parse the next message.

- Initialize UDP server:

```c
typedef void (*udp_recv_callback)(int fd, struct sockaddr_in* cliaddr, const char* data, unsigned int len);

epoll_server_t* epoll_udp_server_init(
    unsigned port,
    udp_recv_callback handler,
    int max_client
);
```

- Server start/stop:

```c
void epoll_server_start(epoll_server_t* server);  // enter epoll loop (blocking)
void epoll_server_stop(epoll_server_t* server);   // set stop flag, exit loop
void epoll_server_delete(epoll_server_t* server); // free resources
```

- Async send API (through send queue + EPOLLOUT):

```c
int epoll_server_send(int fd, const char* data, unsigned int len);
```

This API will:
- append data to the connection's send buffer;
- enable EPOLLOUT and flush data in the write event handler;
- remove EPOLLOUT when send buffer becomes empty to avoid busy wakeups.

## TCP Echo Example (line-based protocol)

Example file: [echo_server.c](file:///home/lyq/code/epollServer/example/echo_server.c)

Key points:
- uses `epoll_tcp_server_init_ex`;
- parser is "\n-terminated line" protocol;
- once a full line is received, it is echoed back as a whole message.

Core code:

```c
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

int main(int argc, char* argv[])
{
    epoll_server_t* evs;

    if (argc < 2) {
        printf("Usage: %s [port]\n", argv[0]);
        return -1;
    }

    evs = epoll_tcp_server_init_ex(
        atoi(argv[1]),
        recv_echo_msg_callback,
        line_parser,
        1024
    );
    if (NULL == evs) {
        printf("epoll_server_init failed.\n");
        return -1;
    }

    epoll_server_start(evs);
    epoll_server_delete(evs);
    return 0;
}
```

Run example:

```bash
./build/example/echo_server 9000
nc 127.0.0.1 9000
hello world
hello world
```

## Concurrent Benchmark Example

To evaluate concurrent performance, a simple benchmark client is provided:  
[concurrent_client.c](file:///home/lyq/code/epollServer/example/concurrent_client.c)

Built executable: `build/example/concurrent_client`

Usage:

```bash
concurrent_client ip port connections messages_per_conn payload_len

# example:
concurrent_client 127.0.0.1 9000 200 100 64
```

Meaning:
- `connections`: concurrent connections
- `messages_per_conn`: number of messages per connection
- `payload_len`: length of each message (last byte is '\n')

Sample output:

```text
connections: 2000
messages_per_conn: 100
payload_len: 512
total_requests: 200000
success: 200000, fail: 0
time: 21.808 s, qps: 9170.95
```

You can adjust these parameters to test the performance under different concurrency and payload sizes.

## Contact

> liuyongqiangx@hotmail.com

