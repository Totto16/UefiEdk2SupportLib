#include <SupportLib/clock.h>

#include <errno.h>

#include <Base.h>
#include <IndustryStandard/Acpi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PciLib.h>
#include <Uefi.h>


#include <Library/BaseMemoryLib.h>

#include "./global.h"


// parts are from: PcAtChipsetPkg/Library/AcpiTimerLib/AcpiTimerLib.c

GUID mFrequencyHobGuid = {
    0x3fca54f6,
    0xe1a2,
    0x4b20,
    { 0xbe, 0x76, 0x92, 0x6b, 0x4b, 0x48, 0xbf, 0xaa }
};


/**
  Internal function to retrieves the 64-bit frequency in Hz.

  Internal function to retrieves the 64-bit frequency in Hz.

  @return The frequency in Hz.

**/
UINT64
InternalGetPerformanceCounterFrequency(VOID);


/**
  Internal function to retrieve the ACPI I/O Port Base Address.

  Internal function to retrieve the ACPI I/O Port Base Address.

  @return The 16-bit ACPI I/O Port Base Address.

**/
UINT16
InternalAcpiGetAcpiTimerIoPort(VOID) {
    UINT16 Port;

    Port = PcdGet16(PcdAcpiIoPortBaseAddress);

    //
    // If the register offset to the BAR for the ACPI I/O Port Base Address is not 0x0000, then
    // read the PCI register for the ACPI BAR value in case the BAR has been programmed to a
    // value other than PcdAcpiIoPortBaseAddress
    //
    if (PcdGet16(PcdAcpiIoPciBarRegisterOffset) != 0x0000) {
        Port = PciRead16(PCI_LIB_ADDRESS(
                PcdGet8(PcdAcpiIoPciBusNumber), PcdGet8(PcdAcpiIoPciDeviceNumber), PcdGet8(PcdAcpiIoPciFunctionNumber),
                PcdGet16(PcdAcpiIoPciBarRegisterOffset)
        ));
    }

    return (Port & PcdGet16(PcdAcpiIoPortBaseAddressMask)) + PcdGet16(PcdAcpiPm1TmrOffset);
}


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


/**
  The constructor function enables ACPI IO space.

  If ACPI I/O space not enabled, this function will enable it.
  It will always return RETURN_SUCCESS.

  @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.

**/
RETURN_STATUS
EFIAPI
AcpiTimerLibConstructor(VOID) {
    UINTN Bus;
    UINTN Device;
    UINTN Function;
    UINTN EnableRegister;
    UINT8 EnableMask;

    //
    // ASSERT for the invalid PCD values. They must be configured to the real value.
    //
    ASSERT(PcdGet16(PcdAcpiIoPciBarRegisterOffset) != 0xFFFF);
    ASSERT(PcdGet16(PcdAcpiIoPortBaseAddress) != 0xFFFF);

    //
    // If the register offset to the BAR for the ACPI I/O Port Base Address is 0x0000, then
    // no PCI register programming is required to enable access to the ACPI registers
    // specified by PcdAcpiIoPortBaseAddress
    //
    if (PcdGet16(PcdAcpiIoPciBarRegisterOffset) == 0x0000) {
        return RETURN_SUCCESS;
    }

    //
    // ASSERT for the invalid PCD values. They must be configured to the real value.
    //
    ASSERT(PcdGet8(PcdAcpiIoPciDeviceNumber) != 0xFF);
    ASSERT(PcdGet8(PcdAcpiIoPciFunctionNumber) != 0xFF);
    ASSERT(PcdGet16(PcdAcpiIoPciEnableRegisterOffset) != 0xFFFF);

    //
    // Retrieve the PCD values for the PCI configuration space required to program the ACPI I/O Port Base Address
    //
    Bus = PcdGet8(PcdAcpiIoPciBusNumber);
    Device = PcdGet8(PcdAcpiIoPciDeviceNumber);
    Function = PcdGet8(PcdAcpiIoPciFunctionNumber);
    EnableRegister = PcdGet16(PcdAcpiIoPciEnableRegisterOffset);
    EnableMask = PcdGet8(PcdAcpiIoBarEnableMask);

    //
    // If ACPI I/O space is not enabled yet, program ACPI I/O base address and enable it.
    //
    if ((PciRead8(PCI_LIB_ADDRESS(Bus, Device, Function, EnableRegister)) & EnableMask) != EnableMask) {
        PciWrite16(
                PCI_LIB_ADDRESS(Bus, Device, Function, PcdGet16(PcdAcpiIoPciBarRegisterOffset)),
                PcdGet16(PcdAcpiIoPortBaseAddress)
        );
        PciOr8(PCI_LIB_ADDRESS(Bus, Device, Function, EnableRegister), EnableMask);
    }

    return RETURN_SUCCESS;
}

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
