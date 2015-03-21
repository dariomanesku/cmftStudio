/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_BGFXBINLOADER_H_HEADER_GUARD
#define CMFTSTUDIO_BGFXBINLOADER_H_HEADER_GUARD

#include "../common/common.h"
#include "loadermanager.h"

namespace bgfx
{
    int32_t read(bx::ReaderI* _reader, bgfx::VertexDecl& _decl);
}

// Bgfx bin format loader.
//-----

struct BgfxBinInData
{
};

struct BgfxBinOutData : public cs::OutDataHeader
{
    float    m_normScale;
    uint16_t m_handle;
};

bool bgfxBinLoader(Geometry& _geometry
                 , dm::ReaderSeekerI* _reader
                 , cs::StackAllocatorI* _stack
                 , void* _inData
                 , cs::OutDataHeader** _outData
                 , bx::ReallocatorI* _allocator
                 )
{
    BX_UNUSED(_inData);
    #define BGFX_CHUNK_MAGIC_VB             BX_MAKEFOURCC('V', 'B', ' ', 0x1)
    #define BGFX_CHUNK_MAGIC_IB             BX_MAKEFOURCC('I', 'B', ' ', 0x0)
    #define BGFX_CHUNK_MAGIC_PRI            BX_MAKEFOURCC('P', 'R', 'I', 0x0)
    #define CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC BX_MAKEFOURCC('M', 'S', 'H', 0x2)
    #define CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE BX_MAKEFOURCC('M', 'S', 'H', 0x3)

    Group group;
    bool done = false;
    uint32_t chunk;
    while (!done && 4 == bx::read(_reader, chunk))
    {
        switch (chunk)
        {
        case BGFX_CHUNK_MAGIC_VB:
            {
                bx::read(_reader, group.m_sphere);
                bx::read(_reader, group.m_aabb);
                bx::read(_reader, group.m_obb);

                bgfx::read(_reader, _geometry.m_decl);
                const uint16_t stride = _geometry.m_decl.getStride();

                bx::read(_reader, group.m_numVertices);

                group.m_vertexSize = group.m_numVertices*stride;
                group.m_vertexData = BX_ALLOC(cs::g_mainAlloc, group.m_vertexSize);
                bx::read(_reader, group.m_vertexData, group.m_vertexSize);
            }
        break;

        case BGFX_CHUNK_MAGIC_IB:
            {
                bx::read(_reader, group.m_numIndices);

                group.m_indexSize = group.m_numIndices*2;
                group.m_indexData = BX_ALLOC(cs::g_mainAlloc, group.m_indexSize);
                bx::read(_reader, group.m_indexData, group.m_indexSize);
            }
        break;

        case BGFX_CHUNK_MAGIC_PRI:
            {
                uint16_t len;
                bx::read(_reader, len);

                if (len < 256)
                {
                    bx::read(_reader, group.m_materialName, len);
                    group.m_materialName[len] = '\0';
                }
                else
                {
                    cs::StackAllocScope scope(_stack);

                    void* matName = BX_ALLOC(_stack, len);

                    bx::read(_reader, matName, len);

                    memcpy(group.m_materialName, matName, Group::MaterialNameLen);
                    group.m_materialName[Group::MaterialNameLen] = '\0';

                    BX_FREE(_stack, matName);
                }

                uint16_t num;
                bx::read(_reader, num);

                for (uint32_t ii = 0; ii < num; ++ii)
                {
                    bx::read(_reader, len);

                    if (len < 256)
                    {
                        char name[256];
                        bx::read(_reader, name, len);
                    }
                    else
                    {
                        cs::StackAllocScope scope(_stack);

                        void* matName = BX_ALLOC(_stack, len);

                        bx::read(_reader, matName, len);

                        BX_FREE(_stack, matName);
                    }

                    Primitive prim;
                    bx::read(_reader, prim.m_startIndex);
                    bx::read(_reader, prim.m_numIndices);
                    bx::read(_reader, prim.m_startVertex);
                    bx::read(_reader, prim.m_numVertices);
                    bx::read(_reader, prim.m_sphere);
                    bx::read(_reader, prim.m_aabb);
                    bx::read(_reader, prim.m_obb);

                    group.m_prims.addObj(prim);
                }

                _geometry.m_groups.addObj(group);
                group.reset();
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC:
            {
                uint16_t id;
                bx::read(_reader, id);

                float normScale;
                bx::read(_reader, normScale);

                if (NULL != _outData)
                {
                    BgfxBinOutData* mesh = (BgfxBinOutData*)BX_ALLOC(_allocator, sizeof(BgfxBinOutData));
                    mesh->m_format    = cs::FileFormat::BgfxBin;
                    mesh->m_normScale = normScale;
                    mesh->m_handle    = id;

                    *_outData = (cs::OutDataHeader*)mesh;
                }
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE:
            {
                done = true;
            }
        break;

        default:
            DBG("%08x at %d", chunk, _reader->seek());
        break;
        }
    }

    return true;
}

#endif // CMFTSTUDIO_BGFXBINLOADER_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
