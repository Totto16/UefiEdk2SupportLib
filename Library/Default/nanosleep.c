
#include <SupportLib/nanosleep.h>

#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Timer.h>

#include <Library/DebugLib.h>

#include "./global.h"

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
