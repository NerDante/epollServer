/*
大小自动扩展的buff实现
*/

#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VecBuff_t* vecbuf_init()
{
    VecBuff_t* vecBuf = NULL;

    vecBuf = malloc(sizeof(VecBuff_t));
    if (NULL == vecBuf) {
        return NULL;
    }

    vecBuf->data = malloc(BUFF_BASE_SIZE);
    if (NULL == vecBuf->data) {
        goto ERR;
    }

    vecBuf->used = 0;

    return vecBuf;

ERR:
    free(vecBuf);
    return NULL;
}

void vecbuf_free(VecBuff_t* vecBuf)
{
    free(vecBuf->data);
    free(vecBuf);
}

static int vecbuf_capacity_extend(VecBuff_t* vecBuf)
{
    char* tmp;

    /* 保存原地址，用于realloc失败的恢复 */
    tmp = vecBuf->data;

    vecBuf->data = realloc(vecBuf->data, vecBuf->capacity * 2);
    if (NULL == vecBuf->data) {
        printf("buff capacity extend error\n");
        vecBuf->data = tmp;

        return -1;
    }

    vecBuf->capacity *= 2;

    return 0;
}

int vecbuf_add_tail(VecBuff_t* vecBuf, const char* data, unsigned int len)
{
    int ret;
    static int failCount;

    if (NULL == vecBuf || NULL == data) {
        printf("null pointer\n");
        return -1;
    }

    while (len > (vecBuf->capacity - vecBuf->used)) {
        ret = vecbuf_capacity_extend(vecBuf);
        if (ret < 0) {
            failCount++;
            printf("expand capacity fail, failtimes = %d\n", failCount);
        }

        if (failCount > 10) {
            printf("memery alloc abnormal,exit!!!!!\n");
            exit(-1);
        }
    }

    memcpy(vecBuf->data + vecBuf->used, data, len);
    vecBuf->used += len;

    return 0;
}

int vecbuf_read(VecBuff_t* vecBuf, unsigned int offset, char* data, unsigned int len)
{
    unsigned int validLen;
    unsigned int readLen;

    if (NULL == vecBuf || NULL == data) {
        printf("null pointer\n");
        return -1;
    }

    if (offset > vecBuf->used) {
        printf("offset is beyond valid data\n");
        return -1;
    }

    /* len > 实际数据长度，返回实际长度 */
    validLen = vecBuf->used - offset;
    readLen = (len > validLen) ? validLen : len;

    memcpy(data, vecBuf->data + offset, readLen);

    return readLen;
}
