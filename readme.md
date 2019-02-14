# Simple Epoll-based  TCP Server
Because based epoll interface, only support linx environment.
## Features
- pure c implement
- used buffer can extend it's self, user dosen't need care about it
- easy interface to use
- use callback to handle recv data

## Build
``` shell
make
make install
```
the library will installed in install/lib of source dir

## Example
simple echo server:
``` c
#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char *argv[])
{
    epoll_server_t *evs;

    if(argc < 2){
       printf("Usage: %s [port]\n", argv[0]);
       return -1;
                                    
    }

    // define your own callback function, now just use echo callback, send data back to client. 
    evs = epoll_server_init(atoi(argv[1]), recv_echo_callback, 1024);
    if(NULL == evs){
        printf("epoll_server_init failed.\n");
        return -1;
    }

    epoll_server_start(evs); //start a loop
    epoll_server_delete(evs);

    return 0;
}

```

## Contact
> liuyongqiangx@hotmail.com
