#include <SupportLib/clock.h>

#include <errno.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Uefi.h>

#include <stdbool.h>

typedef struct MonotonicClockAdvancedLast {
    time_t last_time;
    UINT64 last_counter;
} MonotonicClockAdvancedLast;


typedef struct MonotonicClockMode {
    MonotonicClockAdvancedLast last;
    UINT64 seconds_till_overflow;
    UINT64 counter_base;
    bool raw;
} MonotonicClockMode;


static EFI_STATUS ClockGetTimeMonotonicRaw(OUT struct timespec* Ts) {
    if (Ts == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT64 Start;
    UINT64 End;
    UINT64 Frequency = GetPerformanceCounterProperties(&Start, &End);

    if (Frequency == 0) {
        return EFI_UNSUPPORTED;
    }

    UINT64 Counter = GetPerformanceCounter();

    Ts->tv_sec = (time_t) (Counter / Frequency);

    Ts->tv_nsec = (LONG32) (((Counter % Frequency) * 1000000000ULL) / Frequency);

    return EFI_SUCCESS;
}

static MonotonicClockMode gMonotonicClockMode = {};


#include <Library/DebugLib.h>

static EFI_STATUS ClockGetTimeMonotonicAdvanced(OUT struct timespec* Ts) {

    if (Ts == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT64 Start;
    UINT64 End;
    UINT64 Frequency = GetPerformanceCounterProperties(&Start, &End);

    if (Frequency == 0) {
        return EFI_UNSUPPORTED;
    }

    UINT64 RawCounter = GetPerformanceCounter();

    time_t raw_time = time(NULL);

    if (gMonotonicClockMode.last.last_time == 0) {
        gMonotonicClockMode.last.last_time = raw_time;
        gMonotonicClockMode.last.last_counter = RawCounter;
    }

    if (raw_time < gMonotonicClockMode.last.last_time) {
        // UEFI can't really adjust the time, so if it would happen, we error
        return EFI_DEVICE_ERROR;
    }

    UINT64 time_diff = (UINT64) (raw_time - gMonotonicClockMode.last.last_time);

    //  DEBUG((DEBUG_ERROR, "time_diff:  %llu   %llu %llu \r\n", gMonotonicClockMode.last.last_time, raw_time, time_diff));


    if (gMonotonicClockMode.last.last_counter >= RawCounter) {
        // we overflew the counter, so adjust the counter base

        UINT64 amount = 1ULL;

        if (time_diff > gMonotonicClockMode.seconds_till_overflow) {
            //TODO: we need to be more precise, if amount  > 1 the rounding error we get is wrong, use another method with less rounding errors

            amount = (UINT64) (time_diff / gMonotonicClockMode.seconds_till_overflow);
        }


        gMonotonicClockMode.counter_base += (amount * (End + 1));
    } else if (time_diff > gMonotonicClockMode.seconds_till_overflow) {
        // we overflew the counter (but the counter stayed greater), so adjust the counter base
        // this could happen like this: 10 -> 20, but we skip one loop


        //TODO: not implemented yet
        ASSERT(FALSE);
    }

    UINT64 Counter = gMonotonicClockMode.counter_base + RawCounter;

    gMonotonicClockMode.last.last_time = raw_time;
    gMonotonicClockMode.last.last_counter = RawCounter;

    Ts->tv_sec = (time_t) (Counter / Frequency);

    Ts->tv_nsec = (LONG32) (((Counter % Frequency) * 1000000000ULL) / Frequency);

    return EFI_SUCCESS;
}


static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    if (gMonotonicClockMode.raw) {
        return ClockGetTimeMonotonicRaw(Ts);
    }

    return ClockGetTimeMonotonicAdvanced(Ts);
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

#include <Library/DebugLib.h>

EFI_STATUS
EFIAPI
OOpetrisSupportLibConstructorSupportClockTimerDefault(void) {

    UINT64 Start;
    UINT64 End;
    UINT64 Frequency = GetPerformanceCounterProperties(&Start, &End);

    if (Frequency == 0) {
        return EFI_UNSUPPORTED;
    }

    double seconds_till_overflow = (double) (End + 1) / (double) Frequency;

    if (seconds_till_overflow < 1.0) {
        // this is the worst case scenario, just return an error
        return EFI_DEVICE_ERROR;
    }

    //TODO: this is hacky, use other means, alias only good counters or use another thing for monotnic clocks, or perfomance counter sin sdl2 / my app, as there i expected steady_clock to have a good resolution, at least ms

    // if we wrap every year, we take that and just use the raw value, otherwise we use another method for the monotonic clock

    if (seconds_till_overflow > (double) (365 * 24 * 60 * 60)) {
        gMonotonicClockMode = (MonotonicClockMode){ .last = {},
                                                    .seconds_till_overflow = (UINT64) seconds_till_overflow,
                                                    .counter_base = 0,
                                                    .raw = true };
    } else {
        gMonotonicClockMode = (MonotonicClockMode){
            .last = (MonotonicClockAdvancedLast){ .last_time = 0, .last_counter = 0 },
            .seconds_till_overflow = (UINT64) seconds_till_overflow,
            .counter_base = 0,
            .raw = false,
        };
    }


    return EFI_SUCCESS;
}
