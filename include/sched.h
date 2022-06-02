#ifndef LIMBO_SCHED_H_INCLUDED
#define LIMBO_SCHED_H_INCLUDED

#include <time.h>
#include <stdint.h>

typedef struct {
    struct timespec period;
    struct timespec last_iter;
} timer_state_t;

int sched_timer_wgettime(clockid_t clk, struct timespec *ts);
int64_t sched_rt_millis();

int sched_timer_init(timer_state_t *st, time_t per_sec, long per_nanos);
int sched_timer_wait(timer_state_t *st);

void sched_timespec_add(const struct timespec *summand1, const struct timespec *summand2, struct timespec *res);
void sched_timespec_sub(const struct timespec *minuend, const struct timespec *subtrahend, struct timespec *res);
int sched_timespec_cmp(const struct timespec *rval, const struct timespec *lval);

#endif // include guard
