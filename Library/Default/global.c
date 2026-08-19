

#include "./global.h"

#include <Library/DebugLib.h>

#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/TimerLib.h>
#include <PiDxe.h>


UINT64 gTimerPeriod = 0;
EFI_TIMER_ARCH_PROTOCOL* gTimerAp = NULL;
EFI_EVENT gTimerEvent = NULL;
VOID* gRegistration = NULL;


VOID EFIAPI RegisterTimerArchProtocol(IN EFI_EVENT Event, IN VOID* Context) {
    EFI_STATUS Status;

    Status = gBS->LocateProtocol(&gEfiTimerArchProtocolGuid, NULL, (VOID**) &gTimerAp);
    if (!EFI_ERROR(Status)) {
        Status = gTimerAp->GetTimerPeriod(gTimerAp, &gTimerPeriod);
        ASSERT_EFI_ERROR(Status);

        // Convert to Nanoseconds.
        gTimerPeriod = MultU64x32(gTimerPeriod, 100);

        if (gTimerEvent == NULL) {
            Status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &gTimerEvent);
            ASSERT_EFI_ERROR(Status);
        }
    }
}


UINT64 mAcpiTimerLibTscFrequency = 0;

/**
  The constructor function enables ACPI IO space, and caches PerformanceCounterFrequency.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.

**/
EFI_STATUS
EFIAPI
OOpetrisSupportLibConstructorSupportClockDefault(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    return CommonAcpiTimerLibConstructor();
}


/**
  The constructor function enables ACPI IO space.

  If ACPI I/O space not enabled, this function will enable it.
  It will always return RETURN_SUCCESS.

  @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.

**/
RETURN_STATUS
EFIAPI
AcpiTimerLibConstructor(VOID);

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
InternalCalculateTscFrequency(VOID);

/**
  Internal function to retrieves the 64-bit frequency in Hz.

  Internal function to retrieves the 64-bit frequency in Hz.

  @return The frequency in Hz.

**/
UINT64
InternalGetPerformanceCounterFrequency(VOID) {
    return mAcpiTimerLibTscFrequency;
}


/**
  The constructor function enables ACPI IO space, and caches PerformanceCounterFrequency.

  @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.

**/
EFI_STATUS
CommonAcpiTimerLibConstructor(VOID) {
    EFI_HOB_GUID_TYPE* GuidHob;

    //
    // Enable ACPI IO space.
    //
    AcpiTimerLibConstructor();

    //
    // Initialize PerformanceCounterFrequency
    //
    GuidHob = GetFirstGuidHob(&mFrequencyHobGuid);
    if (GuidHob != NULL) {
        mAcpiTimerLibTscFrequency = *(UINT64*) GET_GUID_HOB_DATA(GuidHob);
    } else {
        mAcpiTimerLibTscFrequency = InternalCalculateTscFrequency();
    }

    return EFI_SUCCESS;
}


/**
  Register for the Timer AP protocol.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
OOpetrisSupportLibConstructorSupportNanosleepDefault(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    EfiCreateProtocolNotifyEvent(
            &gEfiTimerArchProtocolGuid, TPL_CALLBACK, RegisterTimerArchProtocol, NULL, &gRegistration
    );


    return EFI_SUCCESS;
}
