// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#ifndef __UEFI__
#error "Only supported on UEFI"
#endif

#include <sys/time.h>
#include <time.h>
#include <unistd.h>


#ifdef __cplusplus
extern "C" {
#endif

int nanosleep(const struct timespec* __req, struct timespec* __rem);

#ifdef __cplusplus
}
#endif
