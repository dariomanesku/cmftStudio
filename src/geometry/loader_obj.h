/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_LOADEROBJ_H_HEADER_GUARD
#define CMFTSTUDIO_LOADEROBJ_H_HEADER_GUARD

#include "../common/common.h"
#include "loadermanager.h"

#include "objtobin.h"       // objToBin()
#include "loader_bgfxbin.h" // bgfxBinLoader()

// Wavefront Obj loader.
//-----

struct ObjInData
{
    float m_scale;
    uint32_t m_packUv;
    uint32_t m_packNormal;
    bool m_ccw;
    bool m_flipV;
    bool m_calcTangent;
};

struct ObjOutData : public cs::OutDataHeader
{
};

static bool loaderObj(Geometry& _geometry
                    , dm::ReaderSeekerI* _reader
                    , dm::StackAllocatorI* _stack
                    , void* _inData
                    , cs::OutDataHeader** _outData
                    , bx::ReallocatorI* _allocator
                    )
{
    BX_UNUSED(_outData, _allocator);

    ObjInData* inputParams;
    ObjInData defaultValues;
    if (NULL != _inData)
    {
        inputParams = (ObjInData*)_inData;
    }
    else
    {
        defaultValues.m_scale       = 1.0f;
        defaultValues.m_packUv      = 1;
        defaultValues.m_packNormal  = 1;
        defaultValues.m_ccw         = false;
        defaultValues.m_flipV       = false;
        defaultValues.m_calcTangent = true;

        inputParams = &defaultValues;
    }

    dm::StackAllocScope scope(_stack);

    uint8_t* objData = NULL;
    if (_reader->getType() == dm::ReaderWriterTypes::MemoryReader)
    {
        dm::MemoryReader* memory = (dm::MemoryReader*)_reader;
        objData = (uint8_t*)memory->getDataPtr() + _reader->seek();
    }
    else // (_reader->getType() == dm::ReaderWriterTypes::CrtFileReader).
    {
        uint32_t objSize = (uint32_t)bx::getSize(_reader);
        objData = (uint8_t*)BX_ALLOC(_stack, objSize+1);
        objSize = bx::read(_reader, objData, objSize);
        objData[objSize] = '\0';
    }

    void* data;
    uint32_t dataSize;
    objToBin(objData
           , data
           , dataSize
           , _stack
           , inputParams->m_packUv
           , inputParams->m_packNormal
           , inputParams->m_ccw
           , inputParams->m_flipV
           , inputParams->m_calcTangent
           , inputParams->m_scale
           );

    dm::MemoryReader reader(data, dataSize);

    return loaderBgfxBin(_geometry, &reader, _stack, NULL, NULL, NULL);
}

#endif // CMFTSTUDIO_LOADEROBJ_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */

