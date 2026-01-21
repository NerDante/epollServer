/*
大小自动扩展的buff实现
*/

#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VecBuff_t* vecbuf_create()
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
    vecBuf->capacity = BUFF_BASE_SIZE;

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
    void* tmp = NULL;

    tmp = realloc(vecBuf->data, vecBuf->capacity * 2);
    if(NULL == tmp)
    {
        printf("buff capacity extend error\n");
        return -1;
    }
    vecBuf->data = tmp;

    vecBuf->capacity *= 2;
    //printf("buff capacity success, now capacity = %u\n", vecBuf->capacity);

    return 0;
}

int vecbuf_add_tail(VecBuff_t* vecBuf, const char* data, unsigned int len)
{
    int ret;
    int failCount = 0;

    if (NULL == vecBuf || NULL == data) {
        printf("null pointer\n");
        return -1;
    }

    while (len > (vecBuf->capacity - vecBuf->used)) {
        ret = vecbuf_capacity_extend(vecBuf);
        if (ret < 0) {
            failCount++;
            printf("expand capacity fail, failtimes = %d\n", failCount);
        }else{
            failCount = 0;
        }

        if (failCount > 10) {
            printf("memery alloc abnormal!!!!!\n");
            return -2;
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

int vecbuf_consume_head(VecBuff_t* vecBuf, unsigned int len)
{
    if (NULL == vecBuf) {
        printf("null pointer\n");
        return -1;
    }

    if (len >= vecBuf->used) {
        vecBuf->used = 0;
        return 0;
    }

    memmove(vecBuf->data, vecBuf->data + len, vecBuf->used - len);
    vecBuf->used -= len;

    return 0;
}
