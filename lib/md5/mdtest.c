#include "md5.h"
#include <stdio.h>

int main(void) {
    MD5_CTX ctx;
    uint8_t val[16];
    MD5Init(&ctx);
    MD5Update(&ctx, "OfflinePlayer:", 14);
    MD5Update(&ctx, "figboot", 7);
    MD5Final(val, &ctx);

    for (int i = 0; i < 16; ++i) {
        printf("%2.2x", val[i]);
    }
    putchar('\n');

    return 0;
}
