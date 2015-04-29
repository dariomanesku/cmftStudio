/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD
#define CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

#include <dm/allocator/allocator.h>

namespace cs
{
    extern bx::AllocatorI*   delayedFree; // Used for memory that is referenced and passed to bgfx.
    extern bx::ReallocatorI* bgfxAlloc;   // Bgfx allocator.
    void allocGc();
    void allocDestroy();
} //namespace cs

#endif // CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
