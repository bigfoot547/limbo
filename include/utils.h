#ifndef LIMBO_UTILS_H_INCLUDED
#define LIMBO_UTILS_H_INCLUDED

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

int sprintf_alloc(char **dest, const char *fmt, ...);
int vsprintf_alloc(char **dest, const char *fmt, va_list va);

struct auto_buffer {
    size_t capacity, limit;
    unsigned char *buf, *readcur, *writecur;
};

int ab_init(struct auto_buffer *ab, size_t capacity, size_t limit);
void ab_free(struct auto_buffer *ab);
int ab_push(struct auto_buffer *ab, const void *buf, size_t sz);
#define ab_push2(ab, buf, sz) (ab_push((ab), (buf), (sz) * sizeof(*buf)))
int ab_expect(struct auto_buffer *ab, size_t newsz);

#define AB_REWIND_RD (0)
#define AB_REWIND_RDWR (1)
void ab_rewind(struct auto_buffer *ab, unsigned which);
void ab_read(struct auto_buffer *ab, void *out, size_t *len);
size_t ab_getrdcur(struct auto_buffer *ab);
size_t ab_getwrcur(struct auto_buffer *ab);

#endif // include guard
