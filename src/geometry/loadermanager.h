/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_LOADERMANAGER_H_HEADER_GUARD
#define CMFTSTUDIO_LOADERMANAGER_H_HEADER_GUARD

#include "../common/common.h"

#include "geometry.h"        // Geometry
#include <dm/readerwriter.h> // dm::ReaderSeekerI

#define BGFX_CHUNK_MAGIC_VB             BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB             BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI            BX_MAKEFOURCC('P', 'R', 'I', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC BX_MAKEFOURCC('M', 'S', 'H', 0x2)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE BX_MAKEFOURCC('M', 'S', 'H', 0x3)

namespace cs
{
    struct FileFormat
    {
        enum
        {
            Undefined,

            BgfxBin,
            Obj,
        };
    };

    struct OutDataHeader
    {
        uint16_t m_format;
    };
    typedef bool (*GeometryLoadFn)(Geometry& _geometry
                                 , dm::ReaderSeekerI* _reader
                                 , dm::StackAllocatorI* _stack
                                 , void* _inData
                                 , OutDataHeader** _outData
                                 , bx::ReallocatorI* _outDataAlloc
                                 );

    void geometryLoaderRegister(const char* _name, const char* _ext, GeometryLoadFn _loadFunc);
    void geometryLoaderUnregisterByName(const char* _name);
    void geometryLoaderUnregisterByExt(const char* _ext);

    void    geometryLoaderGetNames(const char* _names[CS_MAX_GEOMETRY_LOADERS]);
    void    geometryLoaderGetExtensions(const char* _extensions[CS_MAX_GEOMETRY_LOADERS]);
    uint8_t geometryLoaderCount();

    bool geometryLoad(Geometry& _geometry
                    , const char* _path
                    , dm::StackAllocatorI* _stack     = dm::stackAlloc
                    , void* _inData                   = NULL
                    , OutDataHeader** _outData        = NULL
                    , bx::ReallocatorI* _outDataAlloc = dm::mainAlloc
                    );
    bool geometryLoad(Geometry& _geometry
                    , const void* _data, size_t _size, const char* _ext
                    , dm::StackAllocatorI* _stack     = dm::stackAlloc
                    , void* _inData                   = NULL
                    , OutDataHeader** _outData        = NULL
                    , bx::ReallocatorI* _outDataAlloc = dm::mainAlloc
                    );
    bool geometryLoad(Geometry& _geometry
                    , dm::ReaderSeekerI* _reader, const char* _ext
                    , dm::StackAllocatorI* _stack     = dm::stackAlloc
                    , void* _inData                   = NULL
                    , OutDataHeader** _outData        = NULL
                    , bx::ReallocatorI* _outDataAlloc = dm::mainAlloc
                    );

} // namespace cs

#endif // CMFTSTUDIO_LOADERMANAGER_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
