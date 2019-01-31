#ifndef __VECBUF_H__
#define __VECBUF_H__

#define BUFF_BASE_SIZE        4096

typedef struct
{
    unsigned int capacity;
    unsigned int used;

    char *data;
}VecBuff_t;

VecBuff_t *vecbuf_init();
void vecbuf_free(VecBuff_t *vecBuf);
int vecbuf_add_tail(VecBuff_t *vecBuf, const char *data, unsigned int len);
int vecbuf_read(VecBuff_t *vecBuf, unsigned int offset, char *data, unsigned int len);

#endif //__VECBUF_H__