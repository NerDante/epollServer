#include <stddef.h>
#include <stdint.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "util.h"

int raw_dump(const unsigned char* buff, int len)
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

uint16_t get_be16value(uint8_t *buf)
{
    uint16_t *ps = (uint16_t *)buf;
    return be16toh(*ps);
}

uint32_t get_be32value(uint8_t *buf)
{
    uint32_t *ps = (uint32_t *)buf;
    return be32toh(*ps);
}

void pack16be(uint8_t *buf, uint16_t host_value)
{
    uint16_t *ps;
    ps = (uint16_t *)buf;
    *ps = htobe16(host_value);
}

void pack32be(uint8_t *buf, uint32_t host_value)
{
    uint32_t *ps;
    ps = (uint32_t *)buf;
    *ps = htobe32(host_value);
}

int setnonblocking(int fd)
{
    int flags;
    if((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
        printf("get falg error\n");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        printf("set nonblock fail\n");
        return -1;
    }
    return 0;
}
