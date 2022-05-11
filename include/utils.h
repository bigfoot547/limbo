#ifndef LIMBO_UTILS_H_INCLUDED
#define LIMBO_UTILS_H_INCLUDED

#include <stdarg.h>

int sprintf_alloc(char **dest, const char *fmt, ...);
int vsprintf_alloc(char **dest, const char *fmt, va_list va);

#endif // include guard
