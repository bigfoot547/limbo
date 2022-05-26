#include "sched.h"
#include "log.h"

#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

bool sched_timer_check_monotonic() {
#if !defined(_POSIX_MONOTONIC_CLOCK) || _POSIX_MONOTONIC_CLOCK == 0
    static int timer_monotonic_supported = -1;
    if (timer_monotonic_supported >= 0) return !!timer_monotonic_supported;

    errno = 0;
    long res = sysconf(_SC_MONOTONIC_CLOCK);
    if (res < 0) {
        if (errno) {
            log_error("sysconf(_SC_MONOTONIC_CLOCK) failed: %s", strerror(errno));
            log_error("Unable to check if the monotonic clock is available. Aborting.");
            abort();
        } else {
            timer_monotonic_supported = 0;
            return false;
        }
    }

    timer_monotonic_supported = 1;
    return true;
#elif _POSIX_MONOTONIC_CLOCK == -1
    return false;
#else
    // supported
    return true;
#endif
}

int sched_timer_wgettime(clockid_t clk, struct timespec *tp) {
    if (!sched_timer_check_monotonic()) {
        log_error("Monotonic clock not supported. Aborting.");
        abort();
    }

    return clock_gettime(clk, tp);
}

int sched_timer_init(timer_state_t *ts, time_t per_sec, long per_nanos) {
    ts->period.tv_sec = per_sec;
    ts->period.tv_nsec = per_nanos;
    return sched_timer_wgettime(CLOCK_MONOTONIC, &ts->last_iter);
}

int sched_timer_wait(timer_state_t *ts) {
    struct timespec now, diff, slp;
    int res;
    if (sched_timer_wgettime(CLOCK_MONOTONIC, &now) < 0) return -1;

    sched_timespec_sub(&now, &ts->last_iter, &diff);
    if (sched_timespec_cmp(&diff, &ts->period) > 0) {
        struct timespec fallbehind;
        sched_timespec_sub(&diff, &ts->period, &fallbehind);
        log_warn("sched_timer_wait: Unable to keep up with timer! Falling behind %ld secs, %ld nanos", fallbehind.tv_sec, fallbehind.tv_nsec);

        sched_timer_wgettime(CLOCK_MONOTONIC, &slp);
        goto waitcomplete;
    }

    sched_timespec_add(&ts->last_iter, &ts->period, &slp);

    errno = 0;
    do {
        res = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &slp, NULL);
        if (res < 0 && errno != EINTR) return -1;
    } while (errno == EINTR);

waitcomplete:
    memcpy(&ts->last_iter, &slp, sizeof(struct timespec));
    return 0;
}

void sched_timespec_add(const struct timespec *summand1, const struct timespec *summand2, struct timespec *res) {
    res->tv_sec = summand1->tv_sec + summand2->tv_sec;
    res->tv_nsec = summand1->tv_nsec + summand2->tv_nsec;
    while (res->tv_nsec > 1000000000l) {
        res->tv_sec += 1;
        res->tv_nsec -= 1000000000l;
    }
}

void sched_timespec_sub(const struct timespec *minuend, const struct timespec *subtrahend, struct timespec *res) {
    res->tv_sec = minuend->tv_sec - subtrahend->tv_sec;
    res->tv_nsec = minuend->tv_nsec - subtrahend->tv_nsec;
    if (res->tv_sec > 0) { // carry situation
        while (res->tv_nsec < 0) {
            res->tv_sec -= 1;
            res->tv_nsec += 1000000000l;
        }
    }
}

int sched_timespec_cmp(const struct timespec *rval, const struct timespec *lval) {
    if (rval->tv_sec > lval->tv_sec) return 1;
    else if (rval->tv_sec < lval->tv_sec) return -1;

    if (rval->tv_nsec > lval->tv_nsec) return 1;
    else if (rval->tv_nsec < lval->tv_nsec) return -1;
    return 0;
}
