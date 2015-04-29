/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD
#define CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

#include <dm/allocator/allocator.h>
#include "config.h" // g_config.m_memorySize

namespace cs
{
    #define DM_MEM_SIZE_FUNC cs::memSize
    static inline size_t memSize()
    {
        configFromDefaultPaths(g_config);
        return size_t(g_config.m_memorySize);
    }

    extern bx::AllocatorI*   delayedFree; // Used for memory that is referenced and passed to bgfx.
    extern bx::ReallocatorI* bgfxAlloc;   // Bgfx allocator.
    void allocGc();
    void allocDestroy();
} //namespace cs

#endif // CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
