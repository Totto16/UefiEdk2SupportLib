#include "/home/totto/Code/coder2k/oopetris_pr5/temp/edk2/OvmfPkg/Library/PlatformDebugLibIoPort/DebugLibDetect.h"

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Uefi.h>


const char* EFIAPI bool_string(bool value) {
    return value ? "true" : "false";
}

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    DEBUG((DEBUG_ERROR, "[error] HELLO WORLD.\n"));
    DEBUG((DEBUG_INFO, "[info] HELLO WORLD.\n"));
    DEBUG((DEBUG_VERBOSE, "[verbose] HELLO WORLD.\n"));
    DEBUG((DEBUG_WARN, "[warn] HELLO WORLD.\n"));

    bool plat_detected = PlatformDebugLibIoPortDetect();

    bool debug_print_enabled = DebugPrintEnabled();

    Print(L"Hello from UEFI!: plat_debug: %a debug: %a\r\n", bool_string(plat_detected),
          bool_string(debug_print_enabled));

    // this should happend by some constructor of the lib "UefiBootServicesTableLib"
    // gST = sysTable;
    // gBS = sysTable->BootServices;
    //gImageHandle = imgHandle;

    Print(L"st %p bs %p imgH: %p\r\n", gST, gBS, gImageHandle);
    ASSERT(gST != NULL);
    ASSERT(gBS != NULL);
    ASSERT(gImageHandle != NULL);

    return EFI_SUCCESS;
}
