#include "../include/clock.h"

#include <errno.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Uefi.h>

#if defined(_OOPETRIS_SUPPORT_PKG_USE_TIMERLIB)
#define SUPPORT_PERFORMANCE_COUNTER_PROPS GetPerformanceCounterProperties
#define SUPPORT_PERFORMANCE_COUNTER_GET GetPerformanceCounter
#else
#define SUPPORT_PERFORMANCE_COUNTER_PROPS Custom_GetPerformanceCounterProperties
#define SUPPORT_PERFORMANCE_COUNTER_GET Custom_GetPerformanceCounter

int Custom_GetPerformanceCounterProperties() {
    // partially from EmulatorPkg/Library/DxeTimerLib/DxeTimerLib.c
    //TODO
    return 1;
}

int Custom_GetPerformanceCounter() {
    // partially from EmulatorPkg/Library/DxeTimerLib/DxeTimerLib.c
    //TODO
    return 1;
}
#endif

//TODO
#if defined(_OOPETRIS_SUPPORT_PKG_USE_TIMERLIB)

static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    UINT64 Counter;
    UINT64 Start;
    UINT64 End;
    UINT64 Frequency;

    if (Ts == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Frequency = SUPPORT_PERFORMANCE_COUNTER_PROPS(&Start, &End);

    if (Frequency == 0) {
        return EFI_UNSUPPORTED;
    }

    Counter = SUPPORT_PERFORMANCE_COUNTER_GET();

    Ts->tv_sec = (INT64) (Counter / Frequency);

    Ts->tv_nsec = (INT64) (((Counter % Frequency) * 1000000000ULL) / Frequency);

    return EFI_SUCCESS;
}

#else


static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    //TODO
    return EFI_UNSUPPORTED;
}


#endif
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
                errno = status;
                return -1;
            }
            return 0;
        default:
            errno = ENOTSUP;
            return -1;
    }
}
