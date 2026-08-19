// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___SUPPORT_UEFI_NANOSLEEP_H
#define _LIBCPP___SUPPORT_UEFI_NANOSLEEP_H

#ifndef __UEFI__
#  error "Only supported on UEFI"
#endif

#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

int nanosleep(const struct timespec* __req, struct timespec* __rem);

#ifdef __cplusplus
}
#endif

#endif // _LIBCPP___SUPPORT_UEFI_NANOSLEEP_H
