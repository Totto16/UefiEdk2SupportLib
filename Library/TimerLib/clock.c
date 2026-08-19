#include <SupportLib/clock.h>

#include <errno.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Uefi.h>


static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    UINT64 Counter;


    if (Ts == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT64 Start;
    UINT64 End;
    UINT64 Frequency = GetPerformanceCounterProperties(&Start, &End);

    if (Frequency == 0) {
        return EFI_UNSUPPORTED;
    }

    Counter = GetPerformanceCounter();

    Ts->tv_sec = (INT64) (Counter / Frequency);

    Ts->tv_nsec = (INT64) (((Counter % Frequency) * 1000000000ULL) / Frequency);

    return EFI_SUCCESS;
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


int clock_getres(clockid_t clockid, struct timespec* res) {
    switch (clockid) {
        case CLOCK_REALTIME:

            if (res == NULL) {
                return 0;
            }

            res->tv_sec = 1;
            res->tv_nsec = 0;
            return 0;
        case CLOCK_MONOTONIC:
            UINT64 Start;
            UINT64 End;
            UINT64 Frequency = GetPerformanceCounterProperties(&Start, &End);

            if (Frequency == 0) {
                errno = ENOTSUP;
                return -1;
            }

            if (Frequency >= 1000000000ULL) {
                res->tv_sec = 0;
                res->tv_nsec = 1;
                return 0;
            }

            if (Frequency == 1) {
                res->tv_sec = 1;
                res->tv_nsec = 0;
                return 0;
            }

            res->tv_sec = 0;
            res->tv_nsec = 1000000000ULL / Frequency;
            return 0;
        default:
            errno = ENOTSUP;
            return -1;
    }
}
