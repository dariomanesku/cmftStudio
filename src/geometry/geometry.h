/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_GEOMETRY_H_HEADER_GUARD
#define CMFTSTUDIO_GEOMETRY_H_HEADER_GUARD

#include "../common/common.h"

#include <bgfx/bgfx.h> // bgfx::VertexDecl
#include <bounds.h>    // Sphere, Aabb, Obb

#include <dm/readerwriter.h> //bx::WriterI
#include <dm/datastructures/objarray.h>

struct Primitive
{
    enum { NameLen = 128 };

    uint32_t m_startIndex;
    uint32_t m_numIndices;
    uint32_t m_startVertex;
    uint32_t m_numVertices;

    Sphere m_sphere;
    Aabb   m_aabb;
    Obb    m_obb;

    char m_name[NameLen];
};
typedef dm::ObjArray<Primitive> PrimitiveArray;

struct Group
{
    enum { MaterialNameLen = 128 };

    ~Group()
    {
        m_prims.destroy();
    }

    void*    m_vertexData;
    uint32_t m_vertexSize;
    uint32_t m_numVertices;

    void*    m_indexData;
    uint32_t m_indexSize;
    uint32_t m_numIndices;

    bool m_32bitIndexBuffer;

    Sphere m_sphere;
    Aabb   m_aabb;
    Obb    m_obb;

    PrimitiveArray m_prims;

    char m_materialName[MaterialNameLen];
};
typedef dm::ObjArray<Group> GroupArray;

struct Geometry
{
    bool isValid() const
    {
        return (0 != m_groups.count());
    }

    bgfx::VertexDecl m_decl;
    GroupArray m_groups;
};

void write(bx::WriterI* _writer
         , const void* _vertices
         , uint32_t _numVertices
         , uint32_t _stride
         , uint32_t _obbSteps
         );
void write(bx::WriterI* _writer
         , const uint8_t* _vertices
         , uint32_t _numVertices
         , const bgfx::VertexDecl& _decl
         , const uint16_t* _indices
         , uint32_t _numIndices
         , const char* _material
         , const Primitive* _primitives
         , uint32_t _primitiveCount
         , uint32_t _obbSteps = 17
         );

#endif // CMFTSTUDIO_GEOMETRY_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
