#include <stddef.h>
#include "util.h"

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

