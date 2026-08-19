

#pragma once

#include <sys/time.h>
#include <time.h>

#ifndef __UEFI__
#error "Only supported on UEFI"
#endif

#ifdef __cplusplus
extern "C" {
#endif

int clock_gettime(clockid_t clockid, struct timespec* tp);

#ifdef __cplusplus
}
#endif
