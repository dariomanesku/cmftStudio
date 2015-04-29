/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_CMFT_H_HEADER_GUARD
#define CMFTSTUDIO_CMFT_H_HEADER_GUARD

#include "allocator.h"

// These must be defined before including cmft/allocator.h
#define CMFT_STACK_PUSH() cs::allocStackPush()
#define CMFT_STACK_POP()  cs::allocStackPop()
#include <cmft/allocator.h>
#include <cmft/image.h>
#include <cmft/clcontext.h>
#include <cmft/cubemapfilter.h>

#endif // CMFTSTUDIO_CMFT_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
