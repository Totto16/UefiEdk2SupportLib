

#include "./global.h"


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


/**
  Register for the Timer AP protocol.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
OOpetrisSupportLibConstructorDefault(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    EfiCreateProtocolNotifyEvent(
            &gEfiTimerArchProtocolGuid, TPL_CALLBACK, RegisterTimerArchProtocol, NULL, &gRegistration
    );

    return EFI_SUCCESS;
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
