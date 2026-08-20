
#include <SupportLib/nanosleep.h>

#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Timer.h>

#include <Library/DebugLib.h>

#include <errno.h>


static UINT64 gTimerPeriod = 0;
static EFI_TIMER_ARCH_PROTOCOL* gTimerAp = NULL;
static EFI_EVENT gTimerEvent = NULL;
static VOID* gRegistration = NULL;

#define TIMER_NOT_SUPPORTED 42


int TimerBasedNanosecondDelay(IN UINTN NanoSeconds, OUT UINT64* NanoSecondsRemaining) {


    // partially from EmulatorPkg/Library/DxeTimerLib/DxeTimerLib.c


    if ((gTimerPeriod == 0) || ((UINT64) NanoSeconds < gTimerPeriod) || (EfiGetCurrentTpl() != TPL_APPLICATION)) {
        *NanoSecondsRemaining = 0;
        return TIMER_NOT_SUPPORTED;
    }


    //
    // use gBS->WaitForEvent () to yield CPU to DXE Core
    // this only has a resolution of gTimerPeriod ns, so otherwise use a CPU stall, or if there was an error in initiliazing the timer, or if we are not in an application context (TPL) as the timer doesn't work correctly in that context
    //
    UINT64 HundredNanoseconds = DivU64x32(NanoSeconds, 100);
    EFI_STATUS Status = gBS->SetTimer(gTimerEvent, TimerRelative, HundredNanoseconds);
    if (EFI_ERROR(Status)) {
        *NanoSecondsRemaining = 0;
        return 1;
    }
    UINTN Index;
    Status = gBS->WaitForEvent(sizeof(gTimerEvent) / sizeof(EFI_EVENT), &gTimerEvent, &Index);
    if (EFI_ERROR(Status)) {
        *NanoSecondsRemaining = 0;
        return 1;
    }

    *NanoSecondsRemaining = NanoSeconds - (HundredNanoseconds * 100);
    return 0;
}

int StallForNanoseconds(IN UINTN NanoSeconds, OUT UINT64* NanoSecondsRemaining) {
    // stall the cpu without using timerlib

    //if we are below 1000, we just over stall
    if (NanoSeconds < 1000) {
        NanoSeconds = 1000;
    }

    UINTN Microseconds = DivU64x32(NanoSeconds, 1000);

    EFI_STATUS Status = gBS->Stall(Microseconds);
    if (EFI_ERROR(Status)) {
        *NanoSecondsRemaining = 0;
        return 1;
    }

    *NanoSecondsRemaining = NanoSeconds - (Microseconds * 1000);
    return 0;
}

int nanosleep(const struct timespec* __req, struct timespec* __rem) {
    // The nanosleep() function is not available on uefi. Therefore, we will call use some helper functions (timer or stall)

    if (__req == NULL) {
        return -1;
    }

    UINT64 NanoSeconds = (UINT64) __req->tv_sec * 1000000000ULL + (UINT64) __req->tv_nsec;

    UINT64 NanoSecondsRemaining = 0;

    int res = TimerBasedNanosecondDelay(NanoSeconds, &NanoSecondsRemaining);

    if (res == TIMER_NOT_SUPPORTED) {
        res = StallForNanoseconds(NanoSeconds, &NanoSecondsRemaining);
    }

    if (res != 0) {
        errno = EINVAL;
        return -1;
    }

    ASSERT(NanoSecondsRemaining < 1000000000ULL);


    if (__rem != NULL) {
        __rem->tv_sec = 0;
        __rem->tv_nsec = NanoSecondsRemaining;
    }

    if (NanoSecondsRemaining != 0) {
        errno = EINTR;
        return -1;
    }

    return 0;
}


VOID EFIAPI RegisterTimerArchProtocol(IN EFI_EVENT Event, IN VOID* Context) {

    EFI_STATUS Status = gBS->LocateProtocol(&gEfiTimerArchProtocolGuid, NULL, (VOID**) &gTimerAp);
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
