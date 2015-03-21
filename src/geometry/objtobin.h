/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_OBJTOBIN_H_HEADER_GUARD
#define CMFTSTUDIO_OBJTOBIN_H_HEADER_GUARD

#include <stdint.h>

namespace bx
{
    struct WriterSeekerI;
    struct ReallocatorI;
}

uint32_t objToBin(const char* _filePath
                , bx::WriterSeekerI* _writer
                , uint32_t _packUv     = 0
                , uint32_t _packNormal = 0
                , bool _ccw            = false
                , bool _flipV          = false
                , bool _hasTangent     = false
                , float _scale         = 1.0f
                );
uint32_t objToBin(const char* _filePath
                , void*& _outData
                , uint32_t& _outDataSize
                , bx::ReallocatorI* _allocator
                , uint32_t _packUv     = 0
                , uint32_t _packNormal = 0
                , bool _ccw            = false
                , bool _flipV          = false
                , bool _hasTangent     = false
                , float _scale         = 1.0f
                );
uint32_t objToBin(const uint8_t* _objData
                , bx::WriterSeekerI* _writer
                , uint32_t _packUv     = 0
                , uint32_t _packNormal = 0
                , bool _ccw            = false
                , bool _flipV          = false
                , bool _hasTangent     = false
                , float _scale         = 1.0f
                );
uint32_t objToBin(const uint8_t* _objData
                , void*& _outData
                , uint32_t& _outDataSize
                , bx::ReallocatorI* _allocator
                , uint32_t _packUv     = 0
                , uint32_t _packNormal = 0
                , bool _ccw            = false
                , bool _flipV          = false
                , bool _hasTangent     = false
                , float _scale         = 1.0f
                );

#endif // CMFTSTUDIO_OBJTOBIN_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
