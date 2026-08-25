#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Uefi.h>

#include <stdio.h>

#include <libc/main.h>


const char* EFIAPI bool_string(bool value) {
    return value ? "true" : "false";
}

static int g_global_value = 0;
static __attribute__((constructor(101))) void initializeGlobalValue1(void) {
    if (g_global_value != 0) {
        DEBUG((DEBUG_ERROR, "[error] g_global_value constructors not run in correct order: %d\n", g_global_value));
        ASSERT(FALSE);
    }

    g_global_value = 1;
    DEBUG((DEBUG_ERROR, "running constructor %a\n", __func__));
}

static __attribute__((constructor(102))) void initializeGlobalValue2(void) {
    if (g_global_value != 1) {
        DEBUG((DEBUG_ERROR, "[error] g_global_value constructors not run in correct order: %d\n", g_global_value));
        ASSERT(FALSE);
    }

    g_global_value = 2;
    DEBUG((DEBUG_ERROR, "running constructor %a\n", __func__));
}

static __attribute__((destructor(101))) void finishGlobalValue1(void) {
    if (g_global_value != 3) {
        DEBUG((DEBUG_ERROR, "[error] g_global_value deconstructors not run in correct order: %d\n", g_global_value));
        ASSERT(FALSE);
    }


    g_global_value = 0;
    DEBUG((DEBUG_ERROR, "running destructor %a\n", __func__));
}

static __attribute__((destructor(102))) void finishGlobalValue2(void) {
    if (g_global_value != 2) {
        DEBUG((DEBUG_ERROR, "[error] g_global_value deconstructors not run in correct order: %d\n", g_global_value));
        ASSERT(FALSE);
    }


    g_global_value = 3;
    DEBUG((DEBUG_ERROR, "running destructor %a\n", __func__));
}

/***
  Demonstrates basic workings of the main() function by displaying a
  welcoming message.

  Note that the UEFI command line is composed of 16-bit UCS2 wide characters.
  The easiest way to access the command line parameters is to cast Argv as:
      wchar_t **wArgv = (wchar_t **)Argv;

  @param[in]  Argc    Number of argument tokens pointed to by Argv.
  @param[in]  Argv    Array of Argc pointers to command line tokens.

  @retval  0         The application exited normally.
  @retval  Other     An error occurred.
***/
int EDK2_LIBC_ENTRY_NAME(IN int Argc, IN char** Argv) {

    if (g_global_value != 2) {
        DEBUG((DEBUG_ERROR, "[error] g_global_value not initialized: %d\n", g_global_value));
        ASSERT(FALSE);
    }

    ASSERT(gST != NULL);
    ASSERT(gBS != NULL);
    ASSERT(gImageHandle != NULL);


    fprintf(stderr, "stderr print\r\n");
    fflush(stderr);
    printf("stdout print: %d\r\n", 42);
    fflush(stdout);

    return 0;
}
