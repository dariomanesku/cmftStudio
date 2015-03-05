/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_GEOMETRY_H_HEADER_GUARD
#define CMFTSTUDIO_GEOMETRY_H_HEADER_GUARD

#include <stdint.h> //uint32_t
#include <bounds.h> //Aabb, Obb, Sphere

#include "common/tinystl.h" //stl::vector

namespace bx
{
    struct WriterI;
    struct WriterSeekerI;
    struct ReallocatorI;
}
namespace bgfx
{
    struct VertexDecl;
}

struct Primitive
{
    uint32_t m_startIndex;
    uint32_t m_numIndices;
    uint32_t m_startVertex;
    uint32_t m_numVertices;

    Sphere m_sphere;
    Aabb m_aabb;
    Obb m_obb;

    char m_name[128];
};

typedef stl::vector<Primitive> PrimitiveArray;

void write(bx::WriterI* _writer, const void* _vertices, uint32_t _numVertices, uint32_t _stride);
void write(bx::WriterI* _writer
         , const uint8_t* _vertices
         , uint32_t _numVertices
         , const bgfx::VertexDecl& _decl
         , const uint16_t* _indices
         , uint32_t _numIndices
         , const char* _material
         , const PrimitiveArray& _primitives
         );

uint32_t objToBin(const char* _filePath
                , bx::WriterSeekerI* _writer
                , uint32_t _packUv     = 0
                , uint32_t _packNormal = 0
                , bool _ccw            = false
                , bool _flipV          = false
                , bool _hasTangent     = false
                , float _scale         = 1.0f
                , uint32_t _obbSteps   = 17
                );
void objToBin(const char* _filePath
            , void*& _outData
            , uint32_t& _outDataSize
            , bx::ReallocatorI* _allocator
            , uint32_t _packUv     = 0
            , uint32_t _packNormal = 0
            , bool _ccw            = false
            , bool _flipV          = false
            , bool _hasTangent     = false
            , float _scale         = 1.0f
            , uint32_t _obbSteps   = 17
            );

#endif // CMFTSTUDIO_GEOMETRY_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
