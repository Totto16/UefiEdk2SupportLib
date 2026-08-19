#include <SupportLib/clock.h>

#include <errno.h>

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Uefi.h>


#include <Library/BaseMemoryLib.h>


// some parts here are from: MdePkg/Library/SecPeiDxeTimerLibCpu/X86TimerLib.c


#define APIC_SVR 0x0f0
#define APIC_LVTERR 0x370
#define APIC_TMICT 0x380
#define APIC_TMCCT 0x390
#define APIC_TDCR 0x3e0

//
// The following array is used in calculating the frequency of local APIC
// timer. Refer to IA-32 developers' manual for more details.
//
static CONST UINT8 mTimerLibLocalApicDivisor[] = { 0x02, 0x04, 0x08, 0x10, 0x02, 0x04, 0x08, 0x10,
                                                   0x20, 0x40, 0x80, 0x01, 0x20, 0x40, 0x80, 0x01 };


/**
  Internal function to retrieve the base address of local APIC.

  This function will ASSERT if:
  The local APIC is not globally enabled.
  The local APIC is not working under XAPIC mode.
  The local APIC is not software enabled.

  @return The base address of local APIC

**/
static UINTN EFIAPI InternalX86GetApicBase(VOID) {
    UINTN MsrValue;
    UINTN ApicBase;

    MsrValue = (UINTN) AsmReadMsr64(27);
    ApicBase = MsrValue & 0xffffff000ULL;

    //
    // Check the APIC Global Enable bit (bit 11) in IA32_APIC_BASE MSR.
    // This bit will be 1, if local APIC is globally enabled.
    //
    ASSERT((MsrValue & BIT11) != 0);

    //
    // Check the APIC Extended Mode bit (bit 10) in IA32_APIC_BASE MSR.
    // This bit will be 0, if local APIC is under XAPIC mode.
    //
    ASSERT((MsrValue & BIT10) == 0);

    //
    // Check the APIC Software Enable/Disable bit (bit 8) in Spurious-Interrupt
    // Vector Register.
    // This bit will be 1, if local APIC is software enabled.
    //
    ASSERT((MmioRead32(ApicBase + APIC_SVR) & BIT8) != 0);

    return ApicBase;
}

/**
  Internal function to return the frequency of the local APIC timer.

  @param  ApicBase  The base address of memory mapped registers of local APIC.

  @return The frequency of the timer in Hz.

**/
UINT32
EFIAPI
InternalX86GetTimerFrequency(IN UINTN ApicBase) {
    return PcdGet32(PcdFSBClock) / mTimerLibLocalApicDivisor[MmioBitFieldRead32(ApicBase + APIC_TDCR, 0, 3)];
}


/**
  Internal function to read the current tick counter of local APIC.

  @param  ApicBase  The base address of memory mapped registers of local APIC.

  @return The tick counter read.

**/
static INT32 EFIAPI InternalX86GetTimerTick(IN UINTN ApicBase) {
    return MmioRead32(ApicBase + APIC_TMCCT);
}

static UINT64 EFIAPI GetPerformanceCounterPropertiesFreq(void) {
    UINTN ApicBase = InternalX86GetApicBase();

    return (UINT64) InternalX86GetTimerFrequency(ApicBase);
}

/**
  Retrieves the current value of a 64-bit free running performance counter.

  The counter can either count up by 1 or count down by 1. If the physical
  performance counter counts by a larger increment, then the counter values
  must be translated. The properties of the counter can be retrieved from
  GetPerformanceCounterProperties().

  @return The current value of the free running performance counter.

**/
static UINT64 EFIAPI GetPerformanceCounterImpl(VOID) {
    return (UINT64) (UINT32) InternalX86GetTimerTick(InternalX86GetApicBase());
}

static EFI_STATUS ClockGetTimeMonotonic(OUT struct timespec* Ts) {
    UINT64 Counter;


    if (Ts == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT64 Frequency = GetPerformanceCounterPropertiesFreq();

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
