/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_STACK_H_HEADER_GUARD
#define CMFTSTUDIO_STACK_H_HEADER_GUARD

#include "../appconfig.h"
#include "allocator_config.h"

#include <stdint.h>
#include <dm/misc.h> //U_UMB, dm::alignPtrNext()

struct DynamicStack
{
    void init(uint8_t** _stackPtr, uint8_t** _stackLimit)
    {
        setExternal(_stackPtr, _stackLimit);

        this->init();
    }

    void setExternal(uint8_t** _stackPtr, uint8_t** _stackLimit)
    {
        m_ptr = _stackPtr;
        m_end = _stackLimit;
    }

    void setInternal(uint8_t* _stackPtr, uint8_t* _stackLimit)
    {
        m_internalPtr = _stackPtr;
        m_internalEnd = _stackLimit;

        m_ptr = &m_internalPtr;
        m_end = &m_internalEnd;
    }

    void setInternal(uint8_t* _stackLimit)
    {
        setInternal(*m_ptr, _stackLimit);
    }

    inline uint8_t* getStackPtr() const
    {
        return *m_ptr;
    }

    #include "stack_inline_impl.h"

    inline void setStackPtr(void* _ptr)
    {
        *m_ptr = (uint8_t*)_ptr;
    }

    inline void adjustStackPtr(int64_t _val)
    {
        *m_ptr += _val;
    }

    inline uint8_t* getEnd() const
    {
        return *m_end;
    }

    uint8_t** m_ptr;
    uint8_t** m_end;
    uint8_t* m_internalPtr;
    uint8_t* m_internalEnd;
};

struct FixedStack
{
    void init(void* _begin, size_t _size)
    {
        m_ptr = (uint8_t*)_begin;
        m_end = (uint8_t*)_begin+_size;

        this->init();
    }

    inline uint8_t* getStackPtr() const
    {
        return m_ptr;
    }

    #include "stack_inline_impl.h"

    inline void setStackPtr(void* _ptr)
    {
        m_ptr = (uint8_t*)_ptr;
    }

    inline void adjustStackPtr(int64_t _val)
    {
        m_ptr += _val;
    }

    inline uint8_t* getEnd() const
    {
        return m_end;
    }

    uint8_t* m_ptr;
    uint8_t* m_end;
};

#endif // CMFTSTUDIO_STACK_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
