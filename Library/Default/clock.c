#include <SupportLib/clock.h>

#include <errno.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi.h>


static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    //TODO
    return EFI_UNSUPPORTED;
}


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
            EFI_STATUS status = ClockGetTimeMonotonic(tp);
            if (EFI_ERROR(status)) {
                errno = status & ~MAX_BIT;
                return -1;
            }
            return 0;
        default:
            errno = ENOTSUP;
            return -1;
    }
}
