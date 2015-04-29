/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "staticres.h"

#include "inflatedeflate.h"
#include <dm/misc.h>

// Resource headers.
#define SR_INCLUDE
#include "staticres_res.h"

// Declarations.
#define SR_DESC(_name, _dataArray) \
    void*    g_ ## _name;          \
    uint32_t g_ ## _name ## Size;
#include "staticres_res.h"

#define SR_DESC_COMPR(_name, _dataArray) \
    void*    g_ ## _name;                \
    uint32_t g_ ## _name ## Size;
#include "staticres_res.h"

static inline void readCompressedData(void*& _out, uint32_t& _outSize, const void* _inData, uint32_t _inDataSize)
{
    // Take all available static memory.
    const uint32_t available = (uint32_t)dm::allocRemainingStaticMemory();
    void* mem = DM_ALLOC(dm::staticAlloc, available);
    bx::StaticMemoryBlockWriter memBlock(mem, available);

    // Read and decompress data.
    bx::MemoryReader reader(_inData, _inDataSize);
    const bool result = cs::readInflate(&memBlock, &reader, _inDataSize, dm::stackAlloc);
    CS_CHECK(result, "cs::readInflate() failed!");
    BX_UNUSED(result);

    // Return back unused memory.
    const uint32_t size = (uint32_t)memBlock.seek(0, bx::Whence::Current);
    void* data = DM_REALLOC(dm::staticAlloc, mem, size);

    _out     = data;
    _outSize = size;
}

void initStaticResources()
{
    #define SR_DESC(_name, _dataArray)        \
        g_##_name       = (void*)&_dataArray; \
        g_##_name##Size = sizeof(_dataArray);
    #include "staticres_res.h"

    #define SR_DESC_COMPR(_name, _dataArray) \
        readCompressedData(g_##_name, g_##_name##Size, (void*)_dataArray, sizeof(_dataArray));
    #include "staticres_res.h"
}

/* vim: set sw=4 ts=4 expandtab: */
