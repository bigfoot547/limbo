#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sprintf_alloc(char **dest, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int res = vsprintf_alloc(dest, fmt, va);
    va_end(va);
    return res;
}

int vsprintf_alloc(char **dest, const char *fmt, va_list va) {
    va_list vac;
    size_t sz;
    int n;
    char *dest2;

    va_copy(vac, va);
    n = vsnprintf(NULL, 0, fmt, vac);
    va_end(vac);
    if (n < 0) return -1;

    sz = (size_t)n + 1;
    dest2 = malloc(sz);
    if (!dest2) return -1;

    vsnprintf(dest2, sz, fmt, va);
    *dest = dest2;
    return 0;
}

int ab_init(struct auto_buffer *ab, size_t capacity, size_t limit) {
    if (capacity > 0) {
        unsigned char *newbuf = malloc(capacity);
        if (!newbuf) return -1;
        ab->readcur = ab->writecur = ab->buf = newbuf;
    } else {
        ab->buf = ab->readcur = ab->writecur = NULL;
    }
    ab->capacity = capacity;
    ab->limit = limit;
    return 0;
}

void ab_free(struct auto_buffer *ab) {
    ab->capacity = 0;
    free(ab->buf);
    ab->buf = ab->readcur = ab->writecur = NULL;
}

int ab_push(struct auto_buffer *ab, const void *buf, size_t sz) {
    size_t used = ab->writecur - ab->buf;
    size_t readused = ab->readcur - ab->buf;
    size_t spcremain = ab->capacity - used;
    if (sz > spcremain) {
        size_t newcap = ab->capacity;
        while (sz > (newcap - used)) {
            newcap = newcap ? newcap * 2 : 1;
        }

        if (ab->limit > 0 && newcap > ab->limit) {
            if (sz > (ab->limit - used)) return -2;
            newcap = ab->limit;
        }

        unsigned char *newbuf = realloc(ab->buf, newcap);
        if (!newbuf) return -1;
        ab->buf = newbuf;
        ab->writecur = ab->buf + used;
        ab->readcur = ab->buf + readused;
        ab->capacity = newcap;
    }

    memcpy(ab->writecur, buf, sz);
    ab->writecur += sz;
    return 0;
}

int ab_expect(struct auto_buffer *ab, size_t newsz) {
    if (newsz <= ab->capacity) return 0;
    size_t wused = ab->writecur - ab->buf;
    size_t rused = ab->readcur - ab->buf;

    unsigned char *newbuf = realloc(ab->buf, newsz);
    if (!newbuf) return -1;
    ab->buf = newbuf;
    ab->writecur = ab->buf + wused;
    ab->readcur = ab->buf + rused;
    ab->capacity = newsz;
    return 0;
}

void ab_rewind(struct auto_buffer *ab, unsigned which) {
    switch (which) {
        case AB_REWIND_RDWR:
            ab->writecur = ab->buf;
            // fallthrough
        case AB_REWIND_RD:
            ab->readcur = ab->buf;
    }
}

void ab_read(struct auto_buffer *ab, void *out, size_t *len) {
    size_t remaining = ab->writecur - ab->readcur;
    size_t readlen = *len < remaining ? remaining : *len;
    if (readlen > 0) memcpy(out, ab->readcur, readlen);
    *len = readlen;
}

size_t ab_getrdcur(struct auto_buffer *ab) {
    return ab->readcur - ab->buf;
}

size_t ab_getwrcur(struct auto_buffer *ab) {
    return ab->writecur - ab->buf;
}
