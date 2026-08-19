#include <SupportLib/clock.h>

#include <errno.h>

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Uefi.h>

#include <Library/PcdLib.h>
#include <Register/Cpuid.h>

// some code is based on: UefiCpuPkg/Library/CpuTimerLib/CpuTimerLib.c

/**
  Retrieves the current value of a 64-bit free running performance counter.

  Retrieves the current value of a 64-bit free running performance counter. The
  counter can either count up by 1 or count down by 1. If the physical
  performance counter counts by a larger increment, then the counter values
  must be translated. The properties of the counter can be retrieved from
  GetPerformanceCounterProperties().

  @return The current value of the free running performance counter.

**/
UINT64
EFIAPI
GetPerformanceCounterImpl(VOID) {
    return AsmReadTsc();
}


static UINT64 mAcpiTimerLibTscFrequency = 0;


/**
  Internal function to retrieves the 64-bit frequency in Hz.

  Internal function to retrieves the 64-bit frequency in Hz.

  @return The frequency in Hz.

**/
UINT64 InternalGetPerformanceCounterFrequency(VOID) {
    return mAcpiTimerLibTscFrequency;
}


static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    UINT64 Counter;


    if (Ts == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT64 Frequency = InternalGetPerformanceCounterFrequency();

    if (Frequency == 0) {
        return EFI_UNSUPPORTED;
    }

    Counter = GetPerformanceCounterImpl();

    DEBUG((DEBUG_ERROR, "Counter: %llu\r\n", Counter));

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
            UINT64 Frequency = InternalGetPerformanceCounterFrequency();

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


/**
  CPUID Leaf 0x15 for Core Crystal Clock Frequency.

  The TSC counting frequency is determined by using CPUID leaf 0x15. Frequency in MHz = Core XTAL frequency * EBX/EAX.
  In newer flavors of the CPU, core xtal frequency is returned in ECX or 0 if not supported.
  @return The number of TSC counts per second.

**/
static UINT64 CpuidCoreClockCalculateTscFrequency(OUT EFI_STATUS* Status) {
    UINT64 TscFrequency;
    UINT64 CoreXtalFrequency;
    UINT32 RegEax;
    UINT32 RegEbx;
    UINT32 RegEcx;

    //
    // Use CPUID leaf 0x15 Time Stamp Counter and Nominal Core Crystal Clock Information
    // EBX returns 0 if not supported. ECX, if non zero, provides Core Xtal Frequency in hertz.
    // TSC frequency = (ECX, Core Xtal Frequency) * EBX/EAX.
    //
    AsmCpuid(CPUID_TIME_STAMP_COUNTER, &RegEax, &RegEbx, &RegEcx, NULL);

    //
    // If EAX or EBX returns 0, the XTAL ratio is not enumerated.
    //
    if ((RegEax == 0) || (RegEbx == 0)) {
        if (RegEax == 0) {
            *Status = EFI_UNSUPPORTED;
            return 0;
        }

        if (RegEbx == 0) {
            *Status = EFI_UNSUPPORTED;
            return 0;
        }

        *Status = EFI_SUCCESS;
        return 0;
    }

    //
    // If ECX returns 0, the XTAL frequency is not enumerated.
    // And PcdCpuCoreCrystalClockFrequency defined should base on processor series.
    //
    if (RegEcx == 0) {
        CoreXtalFrequency = PcdGet64(PcdCpuCoreCrystalClockFrequency);
    } else {
        CoreXtalFrequency = (UINT64) RegEcx;
    }

    //
    // Calculate TSC frequency = (ECX, Core Xtal Frequency) * EBX/EAX
    //
    TscFrequency = DivU64x32(MultU64x32(CoreXtalFrequency, RegEbx) + (UINT64) (RegEax >> 1), RegEax);

    *Status = EFI_SUCCESS;
    return TscFrequency;
}

//from: PcAtChipsetPkg/Library/AcpiTimerLib/AcpiTimerLib.c

/**
  Calculate TSC frequency.

  The TSC counting frequency is determined by comparing how far it counts
  during a 101.4 us period as determined by the ACPI timer.
  The ACPI timer is used because it counts at a known frequency.
  The TSC is sampled, followed by waiting 363 counts of the ACPI timer,
  or 101.4 us. The TSC is then sampled again. The difference multiplied by
  9861 is the TSC frequency. There will be a small error because of the
  overhead of reading the ACPI timer. An attempt is made to determine and
  compensate for this error.

  @return The number of TSC counts per second.

**/
/*
UINT64
InternalCalculateTscFrequency(VOID) {
    UINT64 StartTSC;
    UINT64 EndTSC;
    UINT16 TimerAddr;
    UINT32 Ticks;
    UINT64 TscFrequency;
    BOOLEAN InterruptState;

    InterruptState = SaveAndDisableInterrupts();

    TimerAddr = InternalAcpiGetAcpiTimerIoPort();
    //
    // Compute the number of ticks to wait to measure TSC frequency.
    // Use 363 * 9861 = 3579543 Hz which is within 2 Hz of ACPI_TIMER_FREQUENCY.
    // 363 counts is a calibration time of 101.4 uS.
    //
    Ticks = IoBitFieldRead32(TimerAddr, 0, 23) + 363;

    StartTSC = AsmReadTsc(); // Get base value for the TSC
    //
    // Wait until the ACPI timer has counted 101.4 us.
    // Timer wrap-arounds are handled correctly by this function.
    // When the current ACPI timer value is greater than 'Ticks',
    // the while loop will exit.
    //
    while (((Ticks - IoBitFieldRead32(TimerAddr, 0, 23)) & BIT23) == 0) {
        CpuPause();
    }

    EndTSC = AsmReadTsc(); // TSC value 101.4 us later

    TscFrequency = MultU64x32(
            (EndTSC - StartTSC), // Number of TSC counts in 101.4 us
            9861                 // Number of 101.4 us in a second
    );

    SetInterruptState(InterruptState);

    return TscFrequency;
}
    */


EFI_STATUS
EFIAPI
OOpetrisSupportLibConstructorSupportClockDefault(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {

    EFI_STATUS Status = EFI_SUCCESS;
    mAcpiTimerLibTscFrequency = CpuidCoreClockCalculateTscFrequency(&Status);

    // if (EFI_ERROR(Status)) {
    //     mAcpiTimerLibTscFrequency = InternalCalculateTscFrequency();
    // }

    return Status;
}
