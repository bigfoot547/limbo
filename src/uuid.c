#include "uuid.h"
#include "md5.h"
#include "endianutils.h"

#include <string.h>
#include <stdio.h>

int uuid_version(struct uuid *id) {
    return (id->mostsig >> 12) & 0xF;
}

void uuid_gen_name(struct uuid *dest, const char *ns, const char *name) {
    MD5_CTX ctx;
    unsigned char digest[16];


    MD5Init(&ctx);
    MD5Update(&ctx, (const unsigned char *)ns, strlen(ns));
    MD5Update(&ctx, (const unsigned char *)name, strlen(name));
    MD5Final(digest, &ctx);

    digest[6] &= 0x0F;
    digest[6] |= 0x30; // md5 name-based UUID

    digest[8] &= 0x3F;
    digest[8] |= 0x80; // ITEF variant

    dest->mostsig = eu_htobeu64(((uint64_t *)digest)[0]);
    dest->leastsig = eu_htobeu64(((uint64_t *)digest)[1]);
}

int uuid_parse(struct uuid *id, const char *str, bool lenient) {
    size_t len = strlen(str);
    if (len != 36 && (!lenient || len != 32)) return -1;

    if (len == 36) {
        uint64_t time_low, time_mid, time_hi, clock_seq, node;
        if (sscanf(str, "%8lx-%4lx-%4lx-%4lx-%12lx", &time_low, &time_mid, &time_hi, &clock_seq, &node) != 5) return -1;

        id->mostsig = (time_low << 32) | ((time_mid & 0xFFFF) << 16) | (time_hi & 0xFFFF);
        id->leastsig = ((clock_seq & 0xFFFF) << 48) | (node & 0xFFFFFFFFFFFF);
    } else { // lenient UUID (no hyphens)
        uint64_t hi, lo;
        if (sscanf(str, "%16lx%16lx", &hi, &lo) != 2) return -1;

        id->mostsig = hi;
        id->leastsig = lo;
    }
    return 0;
}

int uuid_format(struct uuid *id, char *str, size_t len) {
    if (len < UUID_STRLEN) return -1;
    return snprintf(str, len, "%8.8lx-%4.4lx-%4.4lx-%4.4lx-%12.12lx",
                              (id->mostsig  >> 32) & 0xFFFFFFFF,
                              (id->mostsig  >> 16) & 0xFFFF,
                               id->mostsig         & 0xFFFF,
                              (id->leastsig >> 48) & 0xFFFF,
                               id->leastsig        & (uint64_t)0xFFFFFFFFFFFF);
}
