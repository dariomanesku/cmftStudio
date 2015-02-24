/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_MEMBLOCK_H_HEADER_GUARD
#define CMFTSTUDIO_MEMBLOCK_H_HEADER_GUARD

#include <stdint.h>
#include "appconfig.h"
#include <bx/allocator.h>
#include <bx/readerwriter.h>
#include <dm/misc.h> //dm::max

class DynamicMemoryBlock : public bx::MemoryBlockI
{
public:
    DynamicMemoryBlock(bx::ReallocatorI* _reallocator, uint32_t _initialSize)
        : m_reallocator(_reallocator)
        , m_data(NULL)
        , m_size(0)
        , m_initialSize(_initialSize)
    {
    }

    virtual ~DynamicMemoryBlock()
    {
    }

    virtual void* more(uint32_t _size = 0) BX_OVERRIDE
    {
        const uint32_t newSize = (0 == m_size) ? m_initialSize : m_size+_size*8;
        void* newData = BX_REALLOC(m_reallocator, m_data, newSize);
        if (NULL != newData)
        {
            m_data = newData;
            m_size = newSize;
        }

        return m_data;
    }

    virtual uint32_t getSize() BX_OVERRIDE
    {
        return m_size;
    }

    void* getData() const
    {
        return m_data;
    }


public:
    bx::ReallocatorI* m_reallocator;
private:
    DynamicMemoryBlock& operator=(const DynamicMemoryBlock& _rhs);
    void* m_data;
    uint32_t m_size;
    uint32_t m_initialSize;
};

class DynamicMemoryBlockWriter : public bx::MemoryWriter
{
public:
    DynamicMemoryBlockWriter(bx::ReallocatorI* _reallocator, uint32_t _initialSize)
        : MemoryWriter(&m_dmb)
        , m_dmb(_reallocator, _initialSize)
    {
        m_begin = this->seek();
    }

    virtual ~DynamicMemoryBlockWriter()
    {
    }

    void* getData()
    {
        return m_dmb.getData();
    }

    void* getDataTrim()
    {
        void*    data = this->getData();
        uint32_t size = this->getDataSize();

        return BX_REALLOC(m_dmb.m_reallocator, data, size);
    }

    void* getDataCopy(bx::AllocatorI* _allocator)
    {
        const void*    data = this->getData();
        const uint32_t size = this->getDataSize();

        void* out = BX_ALLOC(_allocator, size);
        memcpy(out, data, size);

        return out;
    }

    uint32_t getDataSize()
    {
        const int64_t curr = this->seek();
        const uint32_t diff = uint32_t(curr-m_begin);

        return diff;
    }

    uint32_t getAllocatedSize()
    {
        return (uint32_t)m_dmb.getSize();
    }

private:
    int64_t m_begin;
    DynamicMemoryBlock m_dmb;
};

#endif // CMFTSTUDIO_MEMBLOCK_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */

