
#include <SupportLib/nanosleep.h>

#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Timer.h>

#include <Library/DebugLib.h>


static UINT64 gTimerPeriod = 0;
static EFI_TIMER_ARCH_PROTOCOL* gTimerAp = NULL;
static EFI_EVENT gTimerEvent = NULL;
static VOID* gRegistration = NULL;


UINTN
EFIAPI
Custom_NanoSecondDelay(IN UINTN NanoSeconds) {
    // partially from EmulatorPkg/Library/DxeTimerLib/DxeTimerLib.c
    EFI_STATUS Status;
    UINT64 HundredNanoseconds;
    UINTN Index;

    //TODO: donm't use asserts, rather return an error value
    ASSERT(gTimerPeriod != 0);

    ASSERT((EfiGetCurrentTpl() == TPL_APPLICATION));

    ASSERT((UINT64) NanoSeconds > gTimerPeriod);

    //
    // This stall is long, so use gBS->WaitForEvent () to yield CPU to DXE Core
    //

    HundredNanoseconds = DivU64x32(NanoSeconds, 100);
    Status = gBS->SetTimer(gTimerEvent, TimerRelative, HundredNanoseconds);
    ASSERT_EFI_ERROR(Status);

    Status = gBS->WaitForEvent(sizeof(gTimerEvent) / sizeof(EFI_EVENT), &gTimerEvent, &Index);
    ASSERT_EFI_ERROR(Status);


    return NanoSeconds;
}

//TODO: there is another possible implementation:
/*EFI_EVENT Event;

 gBS->CreateEvent (
    EVT_TIMER,
    TPL_CALLBACK,
    NULL,
    NULL,
    &Event
);

gBS->SetTimer (
    Event,
    TimerRelative,
    10 * 1000 * 10
);
 */


int nanosleep(const struct timespec* __req, struct timespec* __rem) {
    // The nanosleep() function is not available on uefi. Therefore, we will call
    // NanoSecondDelay

    if (__req == NULL) {
        return -1;
    }

    UINT64 ns = (UINT64) __req->tv_sec * 1000000000ULL + (UINT64) __req->tv_nsec;

    Custom_NanoSecondDelay(ns);

    if (__rem != NULL) {
        __rem->tv_sec = 0;
        __rem->tv_nsec = 0;
    }

    return 0;
}


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
