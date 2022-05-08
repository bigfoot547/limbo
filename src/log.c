#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

log_level_t log_level = LOG_INFO;

#define TIMEBUF_SIZE (64)
char timebuf[TIMEBUF_SIZE];

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_log(log_level_t level, const char *fmt, ...) {
    va_list va;

    pthread_mutex_lock(&log_mutex);
    if (level < log_level) goto log_done;

    FILE *target = level == LOG_ERROR ? stderr : stdout;
    time_t ts = time(NULL);
    struct tm info;
    localtime_r(&ts, &info);
    strftime(timebuf, TIMEBUF_SIZE, "%H:%M:%S", &info);

    switch (level) {
#define LL(_lev)                                                 \
        case _lev:                                               \
            fprintf(target, "[%s] [%s] ", timebuf, (#_lev) + 4); \
            break;

        LL(LOG_DEBUG)
        LL(LOG_INFO)
        LL(LOG_WARN)
        LL(LOG_ERROR)

#undef LL
        default:
            goto log_done; // drop logs with invalid levels
    }

    va_start(va, fmt);
    vfprintf(target, fmt, va);
    va_end(va);
    fputc('\n', target);

log_done:
    pthread_mutex_unlock(&log_mutex);
}

log_level_t log_getlevel() {
    pthread_mutex_lock(&log_mutex);
    log_level_t level = log_level;
    pthread_mutex_unlock(&log_mutex);
    return level;
}

log_level_t log_setlevel(log_level_t level) {
    if (level > LOG_MAX) return log_getlevel();

    pthread_mutex_lock(&log_mutex);
    log_level_t old = log_level;
    log_level = level;
    pthread_mutex_unlock(&log_mutex);
    return old;
}
