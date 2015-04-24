/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "allocator_p.h"

void* alloc(size_t _size)
{
    uint8_t* curr = getStackPtr();

    // Determine required space for header.
    const uint8_t* aligned    = (uint8_t*)dm::alignPtrNext(curr, CS_NATURAL_ALIGNMENT);
    const uint8_t  spaceAvail = uint8_t(aligned-curr);
    const uint8_t  headerSize = spaceAvail < Header ? uint8_t(HeaderAligned) : spaceAvail;

    // Check for availability.
    const int64_t advance = _size + headerSize;
    if (advance > available())
    {
        CS_PRINT_STACK("Stack alloc: Stack full. Requested: %llu.%lluMB Available: %llu.%llu", dm::U_UMB(_size), dm::U_UMB(available()));
        return NULL;
    }

    // Advance stack.
    adjustStackPtr(advance);

    // Setup pointer.
    void* ptr = curr + headerSize;
    writeSize(ptr, _size);

    // Keep track of last allocation.
    m_last = ptr;

    CS_PRINT_STACK("Stack alloc: %llu.%lluMB / %llu.%lluMB - (0x%p)", dm::U_UMB(advance), dm::U_UMB(available()), ptr);

    return ptr;
}

void* realloc(void* _ptr, size_t _size)
{
    if (NULL == _ptr)
    {
        return this->alloc(_size);
    }
    else if (_ptr == m_last)
    {
        // Determine difference in size.
        const size_t allocSize = readSize(_ptr);
        const int64_t diff = int64_t(_size - allocSize);

        // Check availability.
        if (diff > available())
        {
            CS_PRINT_STACK("Stack realloc: Stack full. Realloc requested: %llu.%lluMB Available: %llu.%llu", dm::U_UMB(diff), dm::U_UMB(available()));
            return NULL;
        }

        // Reposition stack.
        adjustStackPtr(diff);

        // Write new size.
        writeSize(_ptr, _size);

        CS_PRINT_STACK("Stack realloc: %llu.%lluMB / %llu.%lluMB - (0x%p - 0x%p)", dm::U_UMB(diff), dm::U_UMB(available()), m_last, getStackPtr());

        return _ptr;
    }
    else if (this->contains(_ptr))
    {
        CS_PRINT_STACK("Stack realloc: Called on a pointer other than the last one! (0x%p).", _ptr);

        // Make a new allocation on the stack.
        void* newPtr = this->alloc(_size);
        if (NULL == newPtr)
        {
            // Not enugh space on the stack.
            return NULL;
        }

        // Copy data.
        const size_t oldSize = readSize(_ptr);
        memcpy(newPtr, _ptr, oldSize);

        return newPtr;
    }
    else
    {
        CS_PRINT_STACK("Stack realloc: External pointer (0x%p).", _ptr);

        return NULL;
    }
}

void push()
{
    CS_CHECK(m_currFrame < MaxFrames, "Stack::push | Max stack allocations reached!");

    m_frames[m_currFrame++] = getStackPtr();
    m_last = getStackPtr();

    CS_PRINT_STACK("Stack push: > %d \t %llu.%lluMB", m_currFrame, dm::U_UMB(available()));
}

void pop()
{
    CS_CHECK(m_currFrame > 0, "Stack::pop | Nothing left to pop!");

    if (m_currFrame > 0)
    {
        --m_currFrame;
        setStackPtr((uint8_t*)m_frames[m_currFrame]);
        m_last = getStackPtr();

        CS_PRINT_STACK("Stack pop:  %d < \t %llu.%lluMB", m_currFrame, dm::U_UMB(available()));
    }
}

bool contains(void* _ptr) const
{
    return (m_beg <= _ptr && _ptr < getStackPtr());
}

size_t getSize(void* _ptr) const
{
    return readSize(_ptr);
}

void* begin() const
{
    return m_beg;
}

int64_t available() const
{
    return getEnd() - getStackPtr();
}

size_t getUsage()
{
    return getStackPtr() - m_beg;
}

size_t total()
{
    return getEnd() - m_beg;
}

#if CS_ALLOC_PRINT_STATS
void printStats()
{
    const size_t size = getStackPtr() - m_beg;
    printf("Stack:\n");
    printf("\tPosition: %d, Size: %llu.%lluMB\n\n", m_currFrame, dm::U_UMB(size));
}
#endif //CS_ALLOC_PRINT_STATS

static inline size_t readSize(void* _ptr)
{
    size_t* _dst = (size_t*)_ptr - 1;
    return *_dst;
}

private:
void init()
{
    m_last = getStackPtr();
    m_beg  = getStackPtr();
    m_currFrame = 0;
}

static inline void writeSize(void* _ptr, size_t _size)
{
    size_t* _dst = (size_t*)_ptr - 1;
    *_dst = _size;
}

enum
{
    MaxFrames = 128,

    Header        = sizeof(size_t),
    HeaderAligned = (((Header-1)/CS_NATURAL_ALIGNMENT)+1)*CS_NATURAL_ALIGNMENT,
};

void*     m_last;
uint8_t*  m_beg;
uint16_t  m_currFrame;
void*     m_frames[MaxFrames];

/* vim: set sw=4 ts=4 expandtab: */
