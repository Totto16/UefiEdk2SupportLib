extern "C" {

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Uefi.h>
}

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <libc/main.h>


const char* EFIAPI bool_string(bool value) {
    return value ? "true" : "false";
}


static uint8_t g_global_value = 0;
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

static struct CppGlobal {
    uint8_t value = 0;
    CppGlobal() : value{ 2 } {
        DEBUG((DEBUG_ERROR, "running C++ constructor for %a\n", __func__));
    }
    ~CppGlobal() {
        DEBUG((DEBUG_ERROR, "running C++ deconstructor for %a\n", __func__));
    }
} g_cpp_global;

// see https://wiki.osdev.org/Calling_Global_Constructors#Stability_Issues


class A {
public:
    A() {
        DEBUG((DEBUG_ERROR, "running C++ constructor for %a\n", __func__));
    }
    void anything() {
        DEBUG((DEBUG_ERROR, "function on statically initialized function successfully called %a\n", __func__));
    }
    ~A() {
        DEBUG((DEBUG_ERROR, "running C++ deconstructor for %a\n", __func__));
    }
};

A g_a;

void foo(void) {
    A* p_a = &g_a;
    p_a->anything(); // <---- segfault
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
int EDK2_LIBCXX_ENTRY_NAME(IN int Argc, IN char** Argv) {

    if (g_global_value != 2) {
        DEBUG((DEBUG_ERROR, "[error] g_global_value not initialized: %d\n", g_global_value));
        ASSERT(FALSE);
    }

    if (g_cpp_global.value != 2) {
        DEBUG((DEBUG_ERROR, "[error] g_cpp_global not initialized: %d\n", g_cpp_global.value));
        ASSERT(FALSE);
    }

    foo();


    ASSERT(gST != NULL);
    ASSERT(gBS != NULL);
    ASSERT(gImageHandle != NULL);


    std::cerr << "cerr stream print\r\n";
    std::cerr << std::flush;
    std::cout << "cout stream print: " << 42 << "\r\n";
    std::cout << std::flush;

    return 0;
}
