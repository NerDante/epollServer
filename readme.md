# Simple Epoll Server
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
