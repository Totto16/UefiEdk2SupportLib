#include <SupportLib/clock.h>

#include <errno.h>


int clock_gettime(clockid_t clockid, struct timespec* tp) {
    switch (clockid) {
        case CLOCK_REALTIME:

            if (tp == NULL) {
                return -1;
            }

            time_t t = time(NULL);

            tp->tv_sec = t;
            tp->tv_nsec = 0;
            return 0;
        case CLOCK_MONOTONIC:
            errno = ENOTSUP;
            return -1;
        default:
            errno = ENOTSUP;
            return -1;
    }
}
