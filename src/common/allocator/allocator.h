/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD
#define CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

#include <bx/allocator.h>
#include <dm/misc.h>

#define DM_ALLOC   BX_ALLOC
#define DM_FREE    BX_FREE
#define DM_REALLOC BX_REALLOC

namespace dm
{
    struct BX_NO_VTABLE StackAllocatorI : public bx::ReallocatorI
    {
        virtual void push() = 0;
        virtual void pop() = 0;
    };

    inline void push(StackAllocatorI* _stackAllocator)
    {
        _stackAllocator->push();
    }

    inline void pop(StackAllocatorI* _stackAllocator)
    {
        _stackAllocator->pop();
    }

    struct StackAllocScope : dm::NoCopyNoAssign
    {
        StackAllocScope(StackAllocatorI* _stackAlloc) : m_stack(_stackAlloc)
        {
            push(m_stack);
        }

        ~StackAllocScope()
        {
            pop(m_stack);
        }

    private:
        StackAllocatorI* m_stack;
    };

    extern bx::ReallocatorI* crtAlloc;      // C-runtime allocator.
    extern StackAllocatorI*  crtStackAlloc; // C-runtime stack allocator.

    extern bx::ReallocatorI* staticAlloc; // Allocated memory is released on exit.
    extern StackAllocatorI*  stackAlloc;  // Used for temporary allocations.
    extern bx::ReallocatorI* mainAlloc;   // Default allocator.

    bool             allocInit();
    bool             allocContains(void* _ptr);
    size_t           allocSizeOf(void* _ptr);
    size_t           allocRemainingStaticMemory();
    StackAllocatorI* allocCreateStack(size_t _size);
    StackAllocatorI* allocSplitStack(size_t _awayfromStackPtr, size_t _preferedSize);
    void             allocFreeStack(StackAllocatorI* _stackAlloc);
    void             allocPrintStats();
} //namespace dm

#include "../config.h" // g_config.m_memorySize

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
} //namespace cs

#endif // CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
