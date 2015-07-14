/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "../common/common.h"
#include "loadermanager.h"

#include <dm/datastructures/objarray.h>
#include <dm/readerwriter.h> // dm::ReaderSeekerI

namespace cs
{
    struct GeometryLoader
    {
        enum
        {
            MaxNameLen = 127+1,
            MaxExtLen  = 7+1,
        };

        char m_name[MaxNameLen];
        char m_ext[MaxExtLen];
        GeometryLoadFn m_loadFunc;
    };

    struct GeometryLoaders
    {
        void registerLoader(const char* _name, const char* _ext, GeometryLoadFn _loadFunc)
        {
            GeometryLoader* obj = m_loaders.addNew();
            dm::strscpya(obj->m_name, _name);
            dm::strscpya(obj->m_ext,  _ext);
            obj->m_loadFunc = _loadFunc;
        }

        void unregisterLoaderByName(const char* _name)
        {
            for (uint8_t ii = m_loaders.count(); ii--; )
            {
                if (0 == bx::stricmp(m_loaders[ii].m_name, _name))
                {
                    m_loaders.removeSwap(ii);
                    return;
                }
            }
        }

        void unregisterLoaderByExt(const char* _ext)
        {
            for (uint8_t ii = m_loaders.count(); ii--; )
            {
                if (0 == bx::stricmp(m_loaders[ii].m_ext, _ext))
                {
                    m_loaders.removeSwap(ii);
                    return;
                }
            }
        }

        void getNames(const char* _names[CS_MAX_GEOMETRY_LOADERS]) const
        {
            for (uint8_t ii = m_loaders.count(); ii--; )
            {
                _names[ii] = m_loaders[ii].m_name;
            }
        }

        void getExtensions(const char* _extensions[CS_MAX_GEOMETRY_LOADERS]) const
        {
            for (uint8_t ii = m_loaders.count(); ii--; )
            {
                _extensions[ii] = m_loaders[ii].m_ext;
            }
        }

        uint8_t count() const
        {
            return m_loaders.count();
        }

        bool load(Geometry& _geometry, dm::ReaderSeekerI* _reader, const char* _fileExtension, dm::StackAllocatorI* _stack, void* _inData, OutDataHeader** _outData, bx::ReallocatorI* _outDataAlloc = dm::mainAlloc)
        {
            for (uint8_t ii = m_loaders.count(); ii--; )
            {
                if (0 == bx::stricmp(m_loaders[ii].m_ext, _fileExtension))
                {
                    return m_loaders[ii].m_loadFunc(_geometry, _reader, _stack, _inData, _outData, _outDataAlloc);
                }
            }
            return false;
        }

        bool load(Geometry& _geometry, const char* _path, dm::StackAllocatorI* _stack, void* _inData, OutDataHeader** _outData, bx::ReallocatorI* _outDataAlloc = dm::mainAlloc)
        {
            dm::CrtFileReader fileReader;
            if (fileReader.open(_path))
            {
                CS_CHECK(false, "Could not open file %s for reading.", _path);
                return false;
            }

            const char* ext = dm::fileExt(_path);
            const bool result = load(_geometry, &fileReader, ext, _stack, _inData, _outData, _outDataAlloc);

            fileReader.close();

            return result;
        }

        bool load(Geometry& _geometry, const void* _data, size_t _size, const char* _ext, dm::StackAllocatorI* _stack, void* _inData, OutDataHeader** _outData, bx::ReallocatorI* _outDataAlloc = dm::mainAlloc)
        {
            dm::MemoryReader reader(_data, (uint32_t)_size); // Notice: size is limited to 32bit.
            return load(_geometry, &reader, _ext, _stack, _inData, _outData, _outDataAlloc);
        }

    private:
        dm::ObjArrayT<GeometryLoader, CS_MAX_GEOMETRY_LOADERS> m_loaders;
    };
    static GeometryLoaders s_geometryLoaders;

    void geometryLoaderRegister(const char* _name, const char* _ext, GeometryLoadFn _loadFunc)
    {
        s_geometryLoaders.registerLoader(_name, _ext, _loadFunc);
    }

    void geometryLoaderUnregisterByName(const char* _name)
    {
        s_geometryLoaders.unregisterLoaderByName(_name);
    }

    void geometryLoaderUnregisterByExt(const char* _ext)
    {
        s_geometryLoaders.unregisterLoaderByExt(_ext);
    }

    void geometryLoaderGetNames(const char* _names[CS_MAX_GEOMETRY_LOADERS])
    {
        s_geometryLoaders.getNames(_names);
    }

    void geometryLoaderGetExtensions(const char* _extensions[CS_MAX_GEOMETRY_LOADERS])
    {
        s_geometryLoaders.getExtensions(_extensions);
    }

    uint8_t geometryLoaderCount()
    {
        return s_geometryLoaders.count();
    }

    bool geometryLoad(Geometry& _geometry, const char* _path, dm::StackAllocatorI* _stack, void* _inData, OutDataHeader** _outData, bx::ReallocatorI* _outDataAlloc)
    {
        return s_geometryLoaders.load(_geometry, _path, _stack, _inData, _outData, _outDataAlloc);
    }

    bool geometryLoad(Geometry& _geometry, const void* _data, size_t _size, const char* _ext, dm::StackAllocatorI* _stack, void* _inData, OutDataHeader** _outData, bx::ReallocatorI* _outDataAlloc)
    {
        return s_geometryLoaders.load(_geometry, _data, _size, _ext, _stack, _inData, _outData, _outDataAlloc);
    }

    bool geometryLoad(Geometry& _geometry, dm::ReaderSeekerI* _reader, const char* _ext, dm::StackAllocatorI* _stack, void* _inData, OutDataHeader** _outData, bx::ReallocatorI* _outDataAlloc)
    {
        return s_geometryLoaders.load(_geometry, _reader, _ext, _stack, _inData, _outData, _outDataAlloc);
    }

} // namespace cs

/* vim: set sw=4 ts=4 expandtab: */
