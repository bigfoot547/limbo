#ifndef LIMBO_LOG_H_INCLUDED
#define LIMBO_LOG_H_INCLUDED

#define LOG_DEBUG (0)
#define LOG_INFO  (1)
#define LOG_WARN  (2)
#define LOG_ERROR (3)

#define LOG_MAX LOG_ERROR

typedef unsigned log_level_t;

void log_log(log_level_t level, const char *fmt, ...);
log_level_t log_getlevel();
log_level_t log_setlevel(log_level_t newlevel);

#define log_debug(fmt, ...) log_log(LOG_DEBUG, (fmt), ## __VA_ARGS__)
#define log_info(fmt, ...)  log_log(LOG_INFO , (fmt), ## __VA_ARGS__)
#define log_warn(fmt, ...)  log_log(LOG_WARN , (fmt), ## __VA_ARGS__)
#define log_error(fmt, ...) log_log(LOG_ERROR, (fmt), ## __VA_ARGS__)

#endif // include guard
