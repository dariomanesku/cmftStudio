/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "../common/common.h"
#include "geometry.h"

#include <../../src/vertexdecl.h> // bgfx::VertexDecl

#define BGFX_CHUNK_MAGIC_VB             BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB             BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI            BX_MAKEFOURCC('P', 'R', 'I', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC BX_MAKEFOURCC('M', 'S', 'H', 0x2)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE BX_MAKEFOURCC('M', 'S', 'H', 0x3)

void write(bx::WriterI* _writer, const void* _vertices, uint32_t _numVertices, uint32_t _stride, uint32_t _obbSteps)
{
    Sphere maxSphere;
    calcMaxBoundingSphere(maxSphere, _vertices, _numVertices, _stride);

    Sphere minSphere;
    calcMinBoundingSphere(minSphere, _vertices, _numVertices, _stride);

    if (minSphere.m_radius > maxSphere.m_radius)
    {
        bx::write(_writer, maxSphere);
    }
    else
    {
        bx::write(_writer, minSphere);
    }

    Aabb aabb;
    calcAabb(aabb, _vertices, _numVertices, _stride);
    bx::write(_writer, aabb);

    Obb obb;
    calcObb(obb, _vertices, _numVertices, _stride, DM_CLAMP(_obbSteps, 1, 90));
    bx::write(_writer, obb);
}

void write(bx::WriterI* _writer
         , const uint8_t* _vertices
         , uint32_t _numVertices
         , const bgfx::VertexDecl& _decl
         , const uint16_t* _indices
         , uint32_t _numIndices
         , const char* _material
         , const Primitive* _primitives
         , uint32_t _primitiveCount
         , uint32_t _obbSteps
         )
{
    using namespace bx;
    using namespace bgfx;

    uint32_t stride = _decl.getStride();
    write(_writer, BGFX_CHUNK_MAGIC_VB);
    write(_writer, _vertices, _numVertices, stride, _obbSteps);

    write(_writer, _decl);

    write(_writer, uint16_t(_numVertices) );
    write(_writer, _vertices, _numVertices*stride);

    write(_writer, BGFX_CHUNK_MAGIC_IB);
    write(_writer, _numIndices);
    write(_writer, _indices, _numIndices*2);

    write(_writer, BGFX_CHUNK_MAGIC_PRI);
    uint16_t nameLen = uint16_t(strlen(_material));
    write(_writer, nameLen);
    write(_writer, _material, nameLen);
    write(_writer, uint16_t(_primitiveCount));

    for (uint32_t ii = 0, end = _primitiveCount; ii < end; ++ii)
    {
        const Primitive& prim = _primitives[ii];
        nameLen = uint16_t(strlen(prim.m_name));
        write(_writer, nameLen);
        write(_writer, prim.m_name, nameLen);
        write(_writer, prim.m_startIndex);
        write(_writer, prim.m_numIndices);
        write(_writer, prim.m_startVertex);
        write(_writer, prim.m_numVertices);
        write(_writer, &_vertices[prim.m_startVertex*stride], prim.m_numVertices, stride, _obbSteps);
    }
}

/* vim: set sw=4 ts=4 expandtab: */
