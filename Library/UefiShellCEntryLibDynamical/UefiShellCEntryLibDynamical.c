/** @file
  Provides application point extension for "C" style main function

  Copyright (c) 2009 - 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellInterface.h>
#include <Protocol/ShellParameters.h>

#include <Library/ShellCEntryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Library/DebugLib.h>

static void debug_return_status_to_string_shell_impl(INTN result){

  if(!RETURN_ERROR(result)){
     DEBUG ((DEBUG_INFO, "<C exit code>: %d", (UINT8)result));
     return;
  }

  DEBUG ((DEBUG_INFO, "<EFI exit code>: %r", (EFI_STATUS)result));


}

/**
  UEFI entry point for an application that will in turn call the
  ShellAppMain function which has parameters similar to a standard C
  main function.

  An application that uses UefiShellCEntryLib must have a ShellAppMain
  function as prototyped in Include/Library/ShellCEntryLib.h.

  Note that the Shell uses POSITIVE integers for error values, while UEFI
  uses NEGATIVE values.  If the application is to be used within a script,
  it needs to return one of the SHELL_STATUS values defined in Protocol/Shell.h.

  @param  ImageHandle  The image handle of the UEFI Application.
  @param  SystemTable  A pointer to the EFI System Table.

  @retval  EFI_SUCCESS               The application exited normally.
  @retval  Other                     An error occurred.

**/
EFI_STATUS
EFIAPI
ShellCEntryLibDynamical (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  INTN                           ReturnFromMain;
  EFI_SHELL_PARAMETERS_PROTOCOL  *EfiShellParametersProtocol;
  EFI_SHELL_INTERFACE            *EfiShellInterface;
  EFI_STATUS                     Status;

  ReturnFromMain             = -1;
  EfiShellParametersProtocol = NULL;
  EfiShellInterface          = NULL;

  Status = SystemTable->BootServices->OpenProtocol (
                                        ImageHandle,
                                        &gEfiShellParametersProtocolGuid,
                                        (VOID **)&EfiShellParametersProtocol,
                                        ImageHandle,
                                        NULL,
                                        EFI_OPEN_PROTOCOL_GET_PROTOCOL
                                        );
  if (!EFI_ERROR (Status)) {
    //
    // use shell 2.0 interface
    //
    ReturnFromMain = ShellAppMain (
                       EfiShellParametersProtocol->Argc,
                       EfiShellParametersProtocol->Argv
                       );
  } else {
    //
    // try to get shell 1.0 interface instead.
    //
    Status = SystemTable->BootServices->OpenProtocol (
                                          ImageHandle,
                                          &gEfiShellInterfaceGuid,
                                          (VOID **)&EfiShellInterface,
                                          ImageHandle,
                                          NULL,
                                          EFI_OPEN_PROTOCOL_GET_PROTOCOL
                                          );
    if (!EFI_ERROR (Status)) {
      //
      // use shell 1.0 interface
      //
      ReturnFromMain = ShellAppMain (
                         EfiShellInterface->Argc,
                         EfiShellInterface->Argv
                         );
    } else {
      // this is the only difference to the normal (non Dynamical ShellCEntryLib)
      DEBUG ((DEBUG_INFO, "Launching Shell app as UEFI start app: no arguments available\n"));

      ReturnFromMain = ShellAppMain(0, NULL);

      DEBUG ((DEBUG_INFO, "Shell app returned with: "));
      debug_return_status_to_string_shell_impl(ReturnFromMain);
      DEBUG ((DEBUG_INFO, "\n"));

      DEBUG ((DEBUG_INFO, "Shutting down device\n"));

      // shutdown device, as we have no shell to return
      gRT->ResetSystem(
        EfiResetShutdown,
        ReturnFromMain,
        0,
        NULL
      );

      DEBUG ((DEBUG_ERROR, "ResetSystem did return ?!\n"));
      ASSERT(FALSE);

    }
  }

  return ReturnFromMain;
}
