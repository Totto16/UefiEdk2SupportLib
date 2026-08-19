#pragma once


#ifndef __UEFI__
#error "Only supported on UEFI"
#endif

#ifdef __cplusplus
#error "only supported in C"
#endif


#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>


#include <Protocol/Timer.h>

extern UINT64 gTimerPeriod;
extern EFI_TIMER_ARCH_PROTOCOL* gTimerAp;
extern EFI_EVENT gTimerEvent;
extern VOID* gRegistration;
