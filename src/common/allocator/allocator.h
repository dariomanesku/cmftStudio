/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD
#define CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

#include <bx/allocator.h>
#include <dm/misc.h>

namespace cs
{
    struct BX_NO_VTABLE StackAllocatorI : bx::ReallocatorI
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

    extern bx::ReallocatorI* g_crtAlloc;    // Default C-runtime allocator.
    extern bx::ReallocatorI* g_staticAlloc; // Allocated memory is released on exit.
    extern StackAllocatorI*  g_stackAlloc;  // Used for temporary allocations.
    extern bx::ReallocatorI* g_mainAlloc;   // Default allocator.
    extern bx::AllocatorI*   g_delayedFree; // Used for memory that is referenced and passed to bgfx.
    extern bx::ReallocatorI* g_bgfxAlloc;   // Bgfx allocator.

    bool             allocInit();
    size_t           allocRemainingStaticMemory();
    StackAllocatorI* allocCreateStack(size_t _size);
    StackAllocatorI* allocSplitStack(size_t _awayfromStackPtr, size_t _preferedSize);
    void             allocFreeStack(StackAllocatorI* _stackAlloc);
    void             allocGc();
    void             allocDestroy();

} //namespace cs

#endif // CMFTSTUDIO_ALLOCATOR_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
