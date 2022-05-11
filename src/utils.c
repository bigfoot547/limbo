#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

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
