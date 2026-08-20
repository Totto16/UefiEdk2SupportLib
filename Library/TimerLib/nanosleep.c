
#include <SupportLib/nanosleep.h>

#include <Library/TimerLib.h>


int nanosleep(const struct timespec* __req, struct timespec* __rem) {
    // The nanosleep() function is not available on uefi. Therefore, we will call
    // NanoSecondDelay

    if (__req == NULL) {
        return -1;
    }

    UINT64 ns = (UINT64) __req->tv_sec * 1000000000ULL + (UINT64) __req->tv_nsec;

    // thgis stalls, so it isn't a good solution,
    //TODO: use the nanosleep from  the default helper, that uses timers, when the time is large
    NanoSecondDelay(ns);

    if (__rem != NULL) {
        __rem->tv_sec = 0;
        __rem->tv_nsec = 0;
    }

    return 0;
}
