/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "context.h"

#include <stdio.h>
#include <string.h>            // strcpy

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "common/stb_image.h"

#include "common/timer.h"
#include "geometry/loaders.h"
#include "geometry/objtobin.h"
#include "staticres.h"

#include <bgfx_utils.h>        // loadProgram
#include <common.h>            // DBG

#include <dm/misc.h>           // DM_PATH_LEN, dm::fsize
#include <dm/readerwriter.h>
#include <dm/pi.h>

#include <bx/fpumath.h>
#include <bx/macros.h>         // BX_UNUSED

#ifndef CS_LOAD_SHADERS_FROM_DATA_SEGMENT
    #define CS_LOAD_SHADERS_FROM_DATA_SEGMENT 0
#endif //CS_LOAD_SHADERS_FROM_DATA_SEGMENT

#if CS_LOAD_SHADERS_FROM_DATA_SEGMENT
    #include "shaders/headers/shaders.h"
#endif

// Bgfx utils.
//-----

#define BGFX_SAFE_DESTROY_UNIFORM(_uniform)    \
    if (bgfx::invalidHandle != (_uniform).idx) \
    {                                          \
        bgfx::destroyUniform(_uniform);        \
        (_uniform).idx = bgfx::invalidHandle;  \
    }

#define BGFX_SAFE_DESTROY_TEXTURE(_texture)    \
    if (bgfx::invalidHandle != (_texture).idx) \
    {                                          \
        bgfx::destroyTexture(_texture);        \
        (_texture).idx = bgfx::invalidHandle;  \
    }

#define BGFX_SAFE_DESTROY_PROGRAM(_prog)    \
    if (bgfx::invalidHandle != (_prog).idx) \
    {                                       \
        bgfx::destroyProgram(_prog);        \
        (_prog).idx = bgfx::invalidHandle;  \
    }

#define CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC BX_MAKEFOURCC('M', 'S', 'H', 0x2)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE BX_MAKEFOURCC('M', 'S', 'H', 0x3)

namespace bgfx
{
    int32_t read(bx::ReaderI* _reader, bgfx::VertexDecl& _decl);
}

// cmftStudio context.
//-----

namespace cs
{
    // Programs.
    //-----

    bgfx::ProgramHandle s_programs[Program::Count];

    void initPrograms()
    {
        #if CS_LOAD_SHADERS_FROM_DATA_SEGMENT
            #define PROG_DESC(_name, _vs, _fs)                                \
            {                                                                 \
                const bgfx::Memory* _vs;                                      \
                const bgfx::Memory* _fs;                                      \
                switch (bgfx::getRendererType() )                             \
                {                                                             \
                case bgfx::RendererType::Direct3D9:                           \
                    _vs = bgfx::makeRef(_vs ## _dx9, sizeof(_vs ## _dx9));    \
                    _fs = bgfx::makeRef(_fs ## _dx9, sizeof(_fs ## _dx9));    \
                    break;                                                    \
                                                                              \
                case bgfx::RendererType::Direct3D11:                          \
                    _vs = bgfx::makeRef(_vs ## _dx11, sizeof(_vs ## _dx11));  \
                    _fs = bgfx::makeRef(_fs ## _dx11, sizeof(_fs ## _dx11));  \
                    break;                                                    \
                                                                              \
                default:                                                      \
                    _vs = bgfx::makeRef(_vs ## _glsl, sizeof(_vs ## _glsl));  \
                    _fs = bgfx::makeRef(_fs ## _glsl, sizeof(_fs ## _glsl));  \
                    break;                                                    \
                }                                                             \
                                                                              \
                bgfx::ShaderHandle vsh = bgfx::createShader(_vs);             \
                bgfx::ShaderHandle fsh = bgfx::createShader(_fs);             \
                s_programs[Program::_name] = bgfx::createProgram(vsh, fsh);   \
                bgfx::destroyShader(vsh);                                     \
                bgfx::destroyShader(fsh);                                     \
            }
        #else // Load shaders from local files.
            #define PROG_DESC(_name, _vs, _fs) s_programs[Program::_name] = loadProgram(#_vs, #_fs);
        #endif // CS_LOAD_SHADERS_FROM_DATA_SEGMENT
        #include "context_res.h"
    }

    bgfx::ProgramHandle getProgram(Program::Enum _prog)
    {
        return s_programs[_prog];
    }

    static Program::Enum programNormal(Program::Enum _program)
    {
        #define PROG_NORM(_prog, _normal) if (Program::_prog == _program) { return Program::_normal; }
        #include "context_res.h"

        return _program;
    }

    static Program::Enum programTrans(Program::Enum _program)
    {
        #define PROG_TRANS(_prog, _trans) if (Program::_prog == _program) { return Program::_trans; }
        #include "context_res.h"

        return _program;
    }

    // Uniforms.
    //-----

    static const uint8_t s_textureStage[TextureUniform::Count] =
    {
        #define TEXUNI_DESC(_enum, _stage, _name) _stage,
        #include "context_res.h"
    };

    struct UniformsImpl : public Uniforms
    {
        void init()
        {
            u_uniforms = bgfx::createUniform("u_uniforms", bgfx::UniformType::Vec4, Uniforms::Num);

            #define TEXUNI_DESC(_enum, _stage, _name) \
                u_tex[TextureUniform::_enum] = bgfx::createUniform(_name, bgfx::UniformType::Int1);
            #include "context_res.h"
        }

        void submit()
        {
            bgfx::setUniform(u_uniforms, m_data, Uniforms::Num);
        }

        void destroy()
        {
            if (bgfx::isValid(u_uniforms))
            {
                bgfx::destroyUniform(u_uniforms);
            }

            #define TEXUNI_DESC(_enum, _stage, _name) \
                BGFX_SAFE_DESTROY_UNIFORM(u_tex[TextureUniform::_enum]);
            #include "context_res.h"
        }

        bgfx::UniformHandle u_uniforms;
        bgfx::UniformHandle u_tex[TextureUniform::Count];
    };
    static UniformsImpl s_uniforms;

    void initUniforms()
    {
        s_uniforms.init();
    }

    Uniforms& getUniforms()
    {
        return *(Uniforms*)&s_uniforms;
    }

    void submitUniforms()
    {
        s_uniforms.submit();
    }

    // Resource manager.
    //-----

    template <typename Ty, typename TyImpl, typename TyHandle, uint16_t MaxElementsT>
    struct ResourceManagerT
    {
        ResourceManagerT()
        {
            m_refs.zero();
        }

        TyImpl* createObj()
        {
            TyImpl* obj = m_elements.addNew();
            return obj;
        }

        TyHandle getHandle(const TyImpl* obj)
        {
            const TyHandle handle = { m_elements.getHandleOf(obj) };
            return handle;
        }

        TyHandle read(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack = dm::stackAlloc)
        {
            TyImpl* impl = this->createObj();

            const TyHandle handle = this->getHandle(impl);
            impl->read(_reader, handle, _stack);

            return this->acquire(handle);
        }

        TyHandle acquire(TyHandle _handle)
        {
            ++m_refs[_handle.m_idx];
            return _handle;
        }

        void release(TyHandle _handle)
        {
            if (--m_refs[_handle.m_idx] <= 0)
            {
                m_refs[_handle.m_idx] = 0;
                m_cleanup.insert(_handle.m_idx);
            }
        }

        bool contains(TyHandle _handle)
        {
            return m_elements.contains(_handle.m_idx);
        }

        TyImpl* getImpl(TyHandle _handle)
        {
            return m_elements.get(_handle.m_idx);
        }

        Ty* getObj(TyHandle _handle)
        {
            return (Ty*)this->getImpl(_handle);
        }

        void setName(TyHandle _objHandle, const char* _name)
        {
            m_names.map(_objHandle, _name);
        }

        char* getName(TyHandle _handle)
        {
            return m_names.get(_handle);
        }

        void gc()
        {
            for (uint16_t ii = m_cleanup.count(); ii--; )
            {
                const uint16_t idx = m_cleanup.getValueAt(ii);
                m_elements.remove(idx);
            }
            m_cleanup.reset();
        }

        double gc(double _maxMs)
        {
            const double beginTime = timerCurrentMs();
            const double endTime = beginTime + _maxMs;

            for (uint16_t ii = m_cleanup.count(); ii--; )
            {
                const uint16_t idx = m_cleanup.getValueAt(ii);

                m_elements.remove(idx);
                m_cleanup.remove(idx);

                if (endTime < timerCurrentMs())
                {
                    return 0.0;
                }
            }

            const double elapsedTime = timerCurrentMs() - beginTime;
            return (_maxMs - elapsedTime);
        }

        uint16_t gc(uint16_t _maxNum)
        {
            uint16_t max = _maxNum;

            for (uint16_t ii = m_cleanup.count(); ii--; )
            {
                const uint16_t idx = m_cleanup.getValueAt(ii);

                m_elements.remove(idx);
                m_cleanup.remove(idx);

                if (--max)
                {
                    return 0;
                }
            }

            return max;
        }

        void destroyAll()
        {
            m_refs.zero();
            for (uint16_t ii = m_elements.count(); ii--; )
            {
                const uint16_t handle = m_elements.getHandleAt(ii);

                m_elements.remove(handle);
                m_names.unmap(handle);
            }
        }

        uint16_t count() const //debug only
        {
            return m_elements.count();
        }

    protected:
        dm::ListT<TyImpl, MaxElementsT>       m_elements;
        dm::ArrayT<int16_t, MaxElementsT>     m_refs;
        dm::SetT<MaxElementsT>                m_cleanup;
        StrHandleMapT<TyHandle, MaxElementsT> m_names;
    };

    // Resource resolver.
    //-----

    struct ResourceId
    {
        enum { Invalid = UINT16_MAX };
    };

    template <typename TyHandle, uint16_t MaxT>
    struct ResourceResolver
    {
        void map(uint16_t _id, TyHandle _handle)
        {
            m_resourceMap.insert(_id, _handle);
        }

        void resolve(uint16_t _id, TyHandle* _handle)
        {
            UnresolvedResource* ur = m_unresolved.addNew();
            ur->m_id = _id;
            ur->m_handle = _handle;
        }

        void clear()
        {
            m_resourceMap.reset();
            m_unresolved.reset();
        }

        void process()
        {
            for (uint32_t ii = m_unresolved.count(); ii--; )
            {
                const uint16_t id = m_unresolved[ii].m_id;

                if (m_resourceMap.contains(id))
                {
                    TyHandle* handle = m_unresolved[ii].m_handle;
                    *handle = m_resourceMap.valueOf(id);
                    cs::acquire(*handle);
                }
            }

            m_unresolved.reset();
        }

        struct UnresolvedResource
        {
            uint16_t  m_id;
            TyHandle* m_handle;
        };

        dm::KeyValueMapT<TyHandle, MaxT>        m_resourceMap;
        dm::ObjArrayT<UnresolvedResource, MaxT> m_unresolved;
    };
    static ResourceResolver<TextureHandle,  CS_MAX_TEXTURES>      s_textureResolver;
    static ResourceResolver<MaterialHandle, CS_MAX_MATERIALS>     s_materialResolver;
    static ResourceResolver<MeshHandle,     CS_MAX_MESHES>        s_meshResolver;
    static ResourceResolver<EnvHandle,      CS_MAX_ENVIRONMENTS>  s_envResolver;

    void resourceMap(uint16_t _id, TextureHandle _handle)
    {
        if (isValid(_handle))
        {
            s_textureResolver.map(_id, _handle);
        }
    }

    void resourceMap(uint16_t _id, MaterialHandle _handle)
    {
        if (isValid(_handle))
        {
            s_materialResolver.map(_id, _handle);
        }
    }

    void resourceMap(uint16_t _id, MeshHandle _handle)
    {
        if (isValid(_handle))
        {
            s_meshResolver.map(_id, _handle);
        }
    }

    void resourceMap(uint16_t _id, EnvHandle _handle)
    {
        if (isValid(_handle))
        {
            s_envResolver.map(_id, _handle);
        }
    }

    void resourceClearMappings()
    {
        s_textureResolver.clear();
        s_materialResolver.clear();
        s_meshResolver.clear();
        s_envResolver.clear();
    }

    void resourceResolve(TextureHandle* _handle, uint16_t _id)
    {
        if (ResourceId::Invalid != _id)
        {
            s_textureResolver.resolve(_id, _handle);
        }
    }

    void resourceResolve(MaterialHandle* _handle, uint16_t _id)
    {
        if (ResourceId::Invalid != _id)
        {
            s_materialResolver.resolve(_id, _handle);
        }
    }

    void resourceResolve(MeshHandle* _handle, uint16_t _id)
    {
        if (ResourceId::Invalid != _id)
        {
            s_meshResolver.resolve(_id, _handle);
        }
    }

    void resourceResolve(EnvHandle* _handle, uint16_t _id)
    {
        if (ResourceId::Invalid != _id)
        {
            s_envResolver.resolve(_id, _handle);
        }
    }

    void resourceResolveAll()
    {
        s_textureResolver.process();
        s_materialResolver.process();
        s_meshResolver.process();
        s_envResolver.process();
    }


    template <typename TyHandle>
    struct ReadWriteI
    {
        void read(dm::ReaderSeekerI* /*_reader*/, dm::StackAllocatorI* /*_stack*/,  TyHandle /*_handle*/)
        {
            CS_CHECK(false, "Should be overridden!");
        }

        void write(bx::WriterI* /*_writer*/, uint16_t /*_id*/) const
        {
            CS_CHECK(false, "Should be overridden!");
        }
    };

    // Texture.
    //-----

    union TextureFormatInfo
    {
        enum
        {
            ConvertFlag = 0x8000,

            FormatMask  = 0x7fff,
            FormatShift = 0,
        };

        bgfx::TextureFormat::Enum bgfxFormat() const
        {
            return (bgfx::TextureFormat::Enum)((m_data&FormatMask)>>FormatShift);
        }

        cmft::TextureFormat::Enum cmftFormat() const
        {
            return (cmft::TextureFormat::Enum)((m_data&FormatMask)>>FormatShift);
        }

        bool convert() const
        {
            return (0 != (m_data&ConvertFlag));
        }

        uint16_t m_data;
    };

    static inline TextureFormatInfo cmftToBgfx(cmft::TextureFormat::Enum _format)
    {
        static uint16_t s_table[cmft::TextureFormat::Count] =
        {
            cmft::TextureFormat::BGRA8   | TextureFormatInfo::ConvertFlag, //BGR8
            cmft::TextureFormat::BGRA8   | TextureFormatInfo::ConvertFlag, //RGB8
            cmft::TextureFormat::RGBA16  | TextureFormatInfo::ConvertFlag, //RGB16
            cmft::TextureFormat::RGBA16F | TextureFormatInfo::ConvertFlag, //RGB16F
            cmft::TextureFormat::RGBA32F | TextureFormatInfo::ConvertFlag, //RGB32F
            cmft::TextureFormat::RGBA32F | TextureFormatInfo::ConvertFlag, //RGBE

            bgfx::TextureFormat::BGRA8,                                    //BGRA8
            cmft::TextureFormat::BGRA8   | TextureFormatInfo::ConvertFlag, //RGBA8
            bgfx::TextureFormat::RGBA16,                                   //RGBA16
            bgfx::TextureFormat::RGBA16F,                                  //RGBA16F
            bgfx::TextureFormat::RGBA32F,                                  //RGBA32F
        };

        const TextureFormatInfo tif = { s_table[_format] };
        return tif;
    }

    struct TextureImpl : public Texture, public ReadWriteI<TextureHandle>
    {
        struct Type
        {
            enum Enum
            {
                Unknown,
                Tex2D,
                TexCube,
            };
        };

        TextureImpl()
        {
            m_bgfxHandle.idx = bgfx::invalidHandle;
            m_data           = NULL;
            m_size           = 0;
            m_numMips        = 0;
            m_width          = 0;
            m_height         = 0;
            m_format         = bgfx::TextureFormat::BGRA8;
            m_type           = Type::Unknown;
            m_freeData       = true;
        }

        ~TextureImpl()
        {
            destroy();
        }

        void loadRaw(const void* _data, uint32_t _size)
        {
            m_data = BX_ALLOC(dm::mainAlloc, _size);
            memcpy(m_data, _data, _size);
            m_size = _size;
            m_type = Type::Unknown;
        }

        bool load(const void* _dataOrPath, uint32_t _sizeOrInvalid)
        {
            const bool isFile = (UINT32_MAX == _sizeOrInvalid);

            const char*    path = (const char*)_dataOrPath;
            const void*    data = (const void*)_dataOrPath;
            const uint32_t size = _sizeOrInvalid;

            // Try loading the image through cmft.
            cmft::Image image;
            if (isFile)
            {
                cmft::imageLoad(image, path);
            }
            else
            {
                cmft::imageLoad(image, data, size);
            }

            if (cmft::imageIsValid(image))
            {
                cmft::ImageSoftRef imageBgfx;

                const TextureFormatInfo tfi = cmftToBgfx(image.m_format);
                if (tfi.convert())
                {
                    cmft::imageConvert(imageBgfx, tfi.cmftFormat(), image);
                    cmft::imageUnload(image);
                }
                else
                {
                    cmft::imageRef(imageBgfx, image);
                }

                m_data     = imageBgfx.m_data;
                m_size     = imageBgfx.m_dataSize;
                m_numMips  = imageBgfx.m_numMips;
                m_width    = (uint16_t)imageBgfx.m_width;
                m_height   = (uint16_t)imageBgfx.m_height;
                m_format   = cmftToBgfx(imageBgfx.m_format).bgfxFormat();
                m_type     = imageBgfx.m_numFaces == 6 ? Type::TexCube : Type::Tex2D;
                m_freeData = true;

                return true;
            }

            // Try loading the image through stb_image.
            int stbWidth, stbHeight, stbNumComponents;
            // Passing reqNumComponents as 4 forces RGBA8 in m_data.
            // After stbi_load, stbNumComponents will hold the actual # of components from the source image.
            const int reqNumComponents = 4;
            if (isFile)
            {
                m_data = (uint8_t*)stb::stbi_load(path, &stbWidth, &stbHeight, &stbNumComponents, reqNumComponents);
            }
            else
            {
                m_data = (uint8_t*)stb::stbi_load_from_memory((stb::stbi_uc*)data, (int)size, &stbWidth, &stbHeight, &stbNumComponents, reqNumComponents);
            }

            if (m_data)
            {
                m_size     = stbWidth*stbHeight*reqNumComponents;
                m_numMips  = 1;
                m_width    = (uint16_t)stbWidth;
                m_height   = (uint16_t)stbHeight;
                m_format   = bgfx::TextureFormat::RGBA8;
                m_type     = Type::Tex2D;
                m_freeData = true;

                return true;
            }

            // Read/copy raw data.

            if (isFile)
            {
                FILE* file = fopen(path, "rb");
                if (NULL != file)
                {
                    m_size = (uint32_t)dm::fsize(file);
                    m_data = BX_ALLOC(dm::mainAlloc, m_size);
                    m_type = Type::Unknown;

                    size_t read = fread(m_data, 1, m_size, file);
                    CS_CHECK(read == m_size, "Error reading file.");
                    BX_UNUSED(read);
                    fclose(file);

                    return true;
                }
            }
            else
            {
                loadRaw(data, size);

                return true;
            }

            return false;
        }

        bool load(const char* _path)
        {
            return load((void*)_path, UINT32_MAX);
        }

        void read(dm::ReaderSeekerI* _reader, TextureHandle _handle = TextureHandle::invalid(), dm::StackAllocatorI* _stack = dm::stackAlloc)
        {
            BX_UNUSED(_stack);

            this->destroy();

            uint16_t id;
            bx::read(_reader, id);
            resourceMap(id, _handle);

            bx::read(_reader, m_size);
            m_data = (uint8_t*)BX_ALLOC(dm::mainAlloc, m_size);
            bx::read(_reader, m_data, m_size);
        }

        void createGpuBuffers(uint32_t _flags = BGFX_TEXTURE_NONE)
        {
            const bgfx::Memory* mem = bgfx::makeRef(m_data, m_size);
            BGFX_SAFE_DESTROY_TEXTURE(m_bgfxHandle);

            switch (m_type)
            {
            case Type::Unknown:
                {
                    bgfx::TextureInfo info;
                    m_bgfxHandle = bgfx::createTexture(mem, _flags, 0, &info);

                    m_numMips  = info.numMips;
                    m_width    = info.width;
                    m_height   = info.height;
                    m_format   = info.format;
                    m_type     = info.cubeMap ? Type::TexCube : Type::Tex2D;
                }
            break;
            case Type::Tex2D:
                {
                    m_bgfxHandle = bgfx::createTexture2D(m_width, m_height, m_numMips, m_format, _flags, mem);
                }
            break;
            case Type::TexCube:
                {
                    m_bgfxHandle = bgfx::createTextureCube(m_width, m_numMips, m_format, _flags, mem);
                }
            break;
            };
        }

        void write(bx::WriterI* _writer, uint16_t _id = ResourceId::Invalid) const
        {
            bx::write(_writer, _id);
            bx::write(_writer, m_size);
            bx::write(_writer, m_data, m_size);
        }

        void freeMem(bool _delayed = false)
        {
            if (m_freeData && NULL != m_data)
            {
                BX_FREE(_delayed ? cs::delayedFree : dm::mainAlloc, m_data);
                m_data = NULL;
            }

            m_size = 0;
        }

        void destroy()
        {
            if (bgfx::invalidHandle != m_bgfxHandle.idx)
            {
                bgfx::destroyTexture(m_bgfxHandle);
                m_bgfxHandle.idx = bgfx::invalidHandle;
            }

            freeMem();
        }

        uint16_t m_width;
        uint16_t m_height;
        bgfx::TextureFormat::Enum m_format;
        Type::Enum m_type;
        bool m_freeData;
    };

    struct TextureResourceManager : public ResourceManagerT<Texture, TextureImpl, TextureHandle, CS_MAX_TEXTURES>
    {
        TextureHandle create()
        {
            const TextureImpl* texture = this->createObj();
            const TextureHandle handle = this->getHandle(texture);
            return this->acquire(handle);
        }

        TextureHandle loadRaw(const void* _data, uint32_t _size)
        {
            TextureImpl* texture = this->createObj();
            texture->loadRaw(_data, _size);
            texture->createGpuBuffers();

            const TextureHandle handle = this->getHandle(texture);

            return this->acquire(handle);
        }

        TextureHandle load(const void* _data, uint32_t _size)
        {
            TextureImpl* texture = this->createObj();
            texture->load(_data, _size);
            texture->createGpuBuffers();

            const TextureHandle handle = this->getHandle(texture);

            return this->acquire(handle);
        }

        TextureHandle load(const char* _path)
        {
            TextureImpl* texture = this->createObj();
            texture->load(_path);
            texture->createGpuBuffers();

            const TextureHandle handle = this->getHandle(texture);

            return this->acquire(handle);
        }
    };
    static TextureResourceManager* s_textures;

    TextureHandle loadTextureStripes()
    {
        const cs::TextureHandle tex = textureLoadRaw(g_stripesS, g_stripesSSize);
        setName(tex, "Stripes");
        return acquire(tex);
    }

    TextureHandle textureStripes()
    {
        static const cs::TextureHandle tex = loadTextureStripes();
        return tex;
    }

    TextureHandle loadTextureBricksN()
    {
        const cs::TextureHandle tex = textureLoadRaw(g_bricksN, g_bricksNSize);
        setName(tex, "Bricks normal");
        return acquire(tex);
    }

    TextureHandle textureBricksN()
    {
        static const cs::TextureHandle tex = loadTextureBricksN();
        return tex;
    }

    TextureHandle loadTextureBricksAo()
    {
        const cs::TextureHandle tex = textureLoadRaw(g_bricksAo, g_bricksAoSize);
        setName(tex, "Bricks occlusion");
        return acquire(tex);
    }

    TextureHandle textureBricksAo()
    {
        static const cs::TextureHandle tex = loadTextureBricksAo();
        return tex;
    }

    TextureHandle textureLoad(const char* _path)
    {
        return s_textures->load(_path);
    }

    TextureHandle textureLoad(const void* _data, uint32_t _size)
    {
        return s_textures->load(_data, _size);
    }

    TextureHandle textureLoadRaw(const void* _data, uint32_t _size)
    {
        return s_textures->loadRaw(_data, _size);
    }

    bgfx::TextureHandle textureGetBgfxHandle(cs::TextureHandle _handle)
    {
        if (isValid(_handle))
        {
            const Texture* tex = s_textures->getObj(_handle);
            return tex->m_bgfxHandle;
        }
        else
        {
            const bgfx::TextureHandle invalid = BGFX_INVALID_HANDLE;
            return invalid;
        }
    }

    // Materials.
    //-----

    struct MaterialImpl : public Material, public ReadWriteI<MaterialHandle>
    {
        MaterialImpl()
        {
            memset(m_data, 0, sizeof(m_data));
            m_tex[Albedo]       = TextureHandle::invalid();
            m_tex[Normal]       = TextureHandle::invalid();
            m_tex[Surface]      = TextureHandle::invalid();
            m_tex[Reflectivity] = TextureHandle::invalid();
            m_tex[Occlusion]    = TextureHandle::invalid();
            m_tex[Emissive]     = TextureHandle::invalid();

            m_uniformSample[Material::Albedo       ] = &m_albedo.sample;
            m_uniformSample[Material::Normal       ] = &m_normal.sample;
            m_uniformSample[Material::Surface      ] = &m_surface.sample;
            m_uniformSample[Material::Reflectivity ] = &m_specular.sample;
            m_uniformSample[Material::Occlusion    ] = &m_occlusionSample;
            m_uniformSample[Material::Emissive     ] = &m_emissive.sample;
        }

        ~MaterialImpl()
        {
            destroy();
        }

        void load(const float* _data
                , cs::TextureHandle _albedo           = TextureHandle::invalid()
                , cs::TextureHandle _normal           = TextureHandle::invalid()
                , cs::TextureHandle _surface          = TextureHandle::invalid()
                , cs::TextureHandle _reflectivity     = TextureHandle::invalid()
                , cs::TextureHandle _ambientOcclusion = TextureHandle::invalid()
                , cs::TextureHandle _emmisive         = TextureHandle::invalid()
                )
        {
            memcpy(m_data, _data, sizeof(m_data));
            m_tex[Albedo]       = _albedo;
            m_tex[Normal]       = _normal;
            m_tex[Surface]      = _surface;
            m_tex[Reflectivity] = _reflectivity;
            m_tex[Occlusion]    = _ambientOcclusion;
            m_tex[Emissive]     = _emmisive;
        }

        void read(dm::ReaderSeekerI* _reader, MaterialHandle _handle = MaterialHandle::invalid(), dm::StackAllocatorI* _stack = dm::stackAlloc)
        {
            BX_UNUSED(_stack);

            uint16_t id;
            bx::read(_reader, id);
            resourceMap(id, _handle);

            for (uint8_t ii = 0; ii < Material::TextureCount; ++ii)
            {
                uint16_t texId;
                bx::read(_reader, texId);

                resourceResolve(&m_tex[ii], texId);
            }
            bx::read(_reader, m_data, cs::Material::DataSize);
        }

        void write(bx::WriterI* _writer, uint16_t _id = ResourceId::Invalid) const
        {
            bx::write(_writer, _id);
            for (uint8_t ii = 0; ii < Material::TextureCount; ++ii)
            {
                bx::write(_writer, m_tex[ii].m_idx);
            }
            bx::write(_writer, m_data, cs::Material::DataSize);
        }

        void destroy()
        {
            for (uint8_t ii = 0; ii < Material::TextureCount; ++ii)
            {
                if (isValid(m_tex[ii]))
                {
                    cs::release(m_tex[ii]);
                }
            }
        }

    };

    void Material::set(Material::Texture _tex, cs::TextureHandle _handle)
    {
        // Set texture.
        if (isValid(m_tex[_tex]))
        {
            release(m_tex[_tex]);
        }
        m_tex[_tex] = _handle;

        // If valid, sample it.
        if (isValid(_handle) && bgfx::isValid(getObj(_handle).m_bgfxHandle))
        {
            acquire(_handle);
            *m_uniformSample[_tex] = 1.0f;
        }
        else
        {
            *m_uniformSample[_tex] = 0.0f;
        }
    }

    bool Material::has(Material::Texture _tex) const
    {
        return isValid(m_tex[_tex]);
    }

    cs::TextureHandle Material::get(Material::Texture _tex) const
    {
        return m_tex[_tex];
    }

    bgfx::TextureHandle Material::getBgfxHandle(Material::Texture _tex) const
    {
        return textureGetBgfxHandle(m_tex[_tex]);
    }

    struct MaterialResourceManager : public ResourceManagerT<Material, MaterialImpl, MaterialHandle, CS_MAX_MATERIALS>
    {
        MaterialHandle create()
        {
            const MaterialImpl* material = this->createObj();
            const MaterialHandle handle = this->getHandle(material);
            return acquire(handle);
        }

        MaterialHandle add(const float* _data
                         , TextureHandle _albedo           = TextureHandle::invalid()
                         , TextureHandle _normal           = TextureHandle::invalid()
                         , TextureHandle _surface          = TextureHandle::invalid()
                         , TextureHandle _reflectivity     = TextureHandle::invalid()
                         , TextureHandle _ambientOcclusion = TextureHandle::invalid()
                         , TextureHandle _emmisive         = TextureHandle::invalid()
                         )
        {
            MaterialImpl* material = this->createObj();
            material->load(_data, _albedo, _normal, _surface, _reflectivity, _ambientOcclusion, _emmisive);

            const MaterialHandle handle = this->getHandle(material);
            return acquire(handle);
        }

        MaterialHandle copy(MaterialHandle _handle)
        {
            MaterialImpl* mat = this->getImpl(_handle);

            MaterialImpl* newMat = this->createObj();
            memcpy(newMat, mat, sizeof(MaterialImpl));
            #define CS_SAFE_ACQUIRE(_tex) \
                if (isValid(_tex))        \
                {                         \
                    cs::acquire(_tex);    \
                }
            CS_SAFE_ACQUIRE(newMat->get(Material::Albedo));
            CS_SAFE_ACQUIRE(newMat->get(Material::Normal));
            CS_SAFE_ACQUIRE(newMat->get(Material::Surface));
            CS_SAFE_ACQUIRE(newMat->get(Material::Reflectivity));
            CS_SAFE_ACQUIRE(newMat->get(Material::Occlusion));
            CS_SAFE_ACQUIRE(newMat->get(Material::Emissive));
            #undef CS_SAFE_ACQUIRE

            const MaterialHandle handle = this->getHandle(newMat);
            return acquire(handle);
        }

        MaterialHandle acquire(MaterialHandle _handle)
        {
            ++m_refs[_handle.m_idx];
            return _handle;
        }
    };
    static MaterialResourceManager* s_materials;

    MaterialHandle materialCreate()
    {
        return s_materials->create();
    }

    MaterialHandle materialCreatePlain()
    {
        static const float sc_material[Material::Size] =
        {
            0.0f, 0.0f, 0.0f, 0.0f, //albedo
            0.0f, 0.0f, 0.0f, 0.0f, //specular
            0.0f, 0.0f, 0.0f, 0.0f, //emissive
            0.0f, 0.0f, 0.0f, 0.0f, //glossNormal
            0.0f, 0.0f, 1.0f, 1.2f, //surface
            0.0f, 0.0f, 1.0f, 0.0f, //misc
            0.0f, 0.0f, 1.0f, 0.0f, //ao/emissiveIntensity
            1.0f, 0.0f, 0.0f, 0.0f, //swizSurface
            1.0f, 0.0f, 0.0f, 0.0f, //swizReflectivity
            1.0f, 0.0f, 0.0f, 0.0f, //swizAo
        };

        return s_materials->add(sc_material);
    }

    MaterialHandle materialDefault()
    {
        static MaterialHandle s_material = materialCreatePlain();
        return s_material;
    }

    MaterialHandle materialCreateShiny()
    {
        static const float sc_material[Material::Size] =
        {
            1.0f, 1.0f, 1.0f, 0.0f, //albedo
            1.0f, 1.0f, 1.0f, 0.0f, //specular
            1.0f, 1.0f, 1.0f, 0.0f, //emissive
            1.0f, 0.0f, 1.0f, 0.0f, //glossNormal
            1.0f, 0.0f, 1.0f, 1.2f, //surface
            0.0f, 0.0f, 1.0f, 0.0f, //misc
            0.0f, 0.0f, 1.0f, 0.0f, //ao/emissiveIntensity
            1.0f, 0.0f, 0.0f, 0.0f, //swizSurface
            1.0f, 0.0f, 0.0f, 0.0f, //swizReflectivity
            1.0f, 0.0f, 0.0f, 0.0f, //swizAo
        };

        return s_materials->add(sc_material);
    }

    MaterialHandle materialCreateStripes()
    {
        const MaterialHandle mat = materialCreateShiny();
        setName(mat, "Stripes");

        cs::Material& obj = cs::getObj(mat);

        const TextureHandle tex = textureStripes();
        obj.set(cs::Material::Surface, tex);

        return mat;
    }

    MaterialHandle materialCreateBricks()
    {
        const MaterialHandle mat = materialCreateShiny();
        setName(mat, "Bricks");

        cs::Material& obj = cs::getObj(mat);
        obj.set(cs::Material::Normal,    textureBricksN());
        obj.set(cs::Material::Occlusion, textureBricksAo());

        return mat;
    }

    MaterialHandle materialCreate(const float* _data
                                , cs::TextureHandle _albedo
                                , cs::TextureHandle _normal
                                , cs::TextureHandle _surface
                                , cs::TextureHandle _reflectivity
                                , cs::TextureHandle _ambientOcclusion
                                , cs::TextureHandle _emmisive
                                )
    {
        return s_materials->add(_data, _albedo, _normal, _surface, _reflectivity, _ambientOcclusion, _emmisive);
    }

    MaterialHandle materialCreateFrom(MaterialHandle _handle)
    {
        return s_materials->copy(_handle);
    }

    // Mesh.
    //-----

    struct GeometryHandles
    {
        bgfx::VertexBufferHandle m_vbh;
        bgfx::IndexBufferHandle  m_ibh;
    };
    typedef dm::ObjArray<GeometryHandles> GeometryHandleArray;

    struct MeshImpl : public Mesh, public Geometry, public ReadWriteI<MeshHandle>
    {
        MeshImpl()
        {
            m_normScale = 1.0f;
        }

        ~MeshImpl()
        {
            destroy();
        }

        void read(dm::ReaderSeekerI* _reader, MeshHandle _handle = MeshHandle::invalid(), dm::StackAllocatorI* _stack = dm::stackAlloc)
        {
            load(_reader, "bin", NULL, _stack, _handle);
        }

        bool load(dm::ReaderSeekerI* _reader
                , const char* _ext
                , void* _userData = NULL
                , dm::StackAllocatorI* _stack = dm::stackAlloc
                , MeshHandle _handle = MeshHandle::invalid()
                )
        {
            dm::push(_stack);

            cs::OutDataHeader* header = NULL;
            geometryLoad(*this, _reader, _ext, _stack, _userData, &header, _stack);

            if (NULL != header)
            {
                switch (header->m_format)
                {
                case cs::FileFormat::BgfxBin:
                    {
                        BgfxBinOutData* meshData = (BgfxBinOutData*)header;

                        m_normScale = meshData->m_normScale;
                        resourceMap(meshData->m_handle, _handle);
                    }
                break;
                };
            }

            dm::pop(_stack);

            // Get normalized scale.
            if (1.0f == m_normScale)
            {
                float min = FLT_MAX;
                float max = -FLT_MAX;

                const uint32_t stride = m_decl.getStride();

                for (uint32_t ii = 0, end = m_groups.count(); ii < end; ++ii)
                {
                    Group& group = m_groups[ii];

                    Aabb aabb;
                    calcAabb(aabb, group.m_vertexData, group.m_numVertices, stride);

                    min = bx::fmin(min, aabb.m_min[0]);
                    min = bx::fmin(min, aabb.m_min[1]);
                    min = bx::fmin(min, aabb.m_min[2]);

                    max = bx::fmax(max, aabb.m_max[0]);
                    max = bx::fmax(max, aabb.m_max[1]);
                    max = bx::fmax(max, aabb.m_max[2]);
                }

                m_normScale = 1.0f/(max-min);
            }

            return true;
        }

        void createGpuBuffers()
        {
            const uint32_t numGroups = m_groups.count();
            m_bufferHandles.init(numGroups, dm::mainAlloc);
            m_bufferHandles.reserve(numGroups);

            for (uint32_t ii = 0; ii < numGroups; ++ii)
            {
                Group& group = m_groups[ii];
                const bgfx::Memory* mem;

                mem = bgfx::makeRef(group.m_vertexData, group.m_vertexSize);
                m_bufferHandles[ii].m_vbh = bgfx::createVertexBuffer(mem, m_decl);

                const uint16_t flags = (group.m_32bitIndexBuffer ? BGFX_BUFFER_INDEX32 : 0);
                mem = bgfx::makeRef(group.m_indexData, group.m_indexSize);
                m_bufferHandles[ii].m_ibh = bgfx::createIndexBuffer(mem, flags);
            }
        }

        void submit(uint8_t _view
                  , Program::Enum _prog
                  , EnvHandle _env
                  , const float* _mtx
                  , uint32_t _groupIdx
                  , MaterialHandle _material
                  , uint64_t _state
                  )
        {
            Group& group = m_groups[_groupIdx];
            for (uint16_t ii = group.m_prims.count(); ii--; )
            {
                const Primitive& prim = group.m_prims[ii];

                // Material.
                cs::MaterialHandle material = cs::isValid(_material) ? _material : cs::materialDefault();
                cs::setMaterial(material);
                const Material& currentMaterial = getObj(material);
                if (currentMaterial.has(Material::Normal) && 1.0f == currentMaterial.m_normal.sample)
                {
                    _prog = programNormal(_prog);
                }

                // Environment.
                if (cs::isValid(_env))
                {
                    cs::setEnv(_env);
                }

                // Transform.
                bgfx::setTransform(_mtx);

                // Buffers.
                bgfx::setIndexBuffer(m_bufferHandles[_groupIdx].m_ibh, prim.m_startIndex, prim.m_numIndices);
                bgfx::setVertexBuffer(m_bufferHandles[_groupIdx].m_vbh);

                // State.
                bgfx::setState(_state);

                bgfx_submit(_view, _prog);
            }
        }

        void submit(uint8_t _view
                  , Program::Enum _prog
                  , EnvHandle _env
                  , const float* _mtx
                  , const MaterialHandle* _materials
                  , uint64_t _state
                  )
        {
            for (uint32_t group = 0, end = m_groups.count(); group < end; ++group)
            {
                const MaterialHandle material = (NULL != _materials) ? _materials[group] : cs::MaterialHandle::invalid();
                this->submit(_view, _prog, _env, _mtx, group, material, _state);
            }
        }

        void submit(uint8_t _view
                  , Program::Enum _prog
                  , EnvHandle _nextEnv
                  , EnvHandle _currEnv
                  , float _progress
                  , const float* _mtx
                  , uint32_t _groupIdx
                  , MaterialHandle _material
                  , uint64_t _state
                  )
        {
            Group& group = m_groups[_groupIdx];
            for (uint16_t ii = group.m_prims.count(); ii--; )
            {
                const Primitive& prim = group.m_prims[ii];

                // Material.
                cs::MaterialHandle material = cs::isValid(_material) ? _material : cs::materialDefault();
                cs::setMaterial(material);
                const Material& currentMaterial = getObj(material);
                if (currentMaterial.has(Material::Normal) && 1.0f == currentMaterial.m_normal.sample)
                {
                    _prog = programNormal(_prog);
                }

                //Program.
                _prog = programTrans(_prog); // Use transition program.

                // Uniforms.
                cs::Uniforms& uniforms = cs::getUniforms();
                uniforms.m_envTransition = _progress;

                // Environment
                cs::setEnvTransition(_currEnv);
                cs::setEnv(_nextEnv);

                // Transform.
                bgfx::setTransform(_mtx);

                // Buffers.
                bgfx::setIndexBuffer(m_bufferHandles[_groupIdx].m_ibh, prim.m_startIndex, prim.m_numIndices);
                bgfx::setVertexBuffer(m_bufferHandles[_groupIdx].m_vbh);

                // State.
                bgfx::setState(_state);

                bgfx_submit(_view, _prog);
            }
        }

        void submit(uint8_t _view
                  , Program::Enum _prog
                  , EnvHandle _nextEnv
                  , EnvHandle _currEnv
                  , float _progress
                  , const float* _mtx
                  , const MaterialHandle* _materials
                  , uint64_t _state
                  )
        {
            for (uint32_t group = 0, end = m_groups.count(); group < end; ++group)
            {
                const MaterialHandle material = (NULL != _materials) ? _materials[group] : cs::MaterialHandle::invalid();
                this->submit(_view, _prog, _nextEnv, _currEnv, _progress, _mtx, group, material, _state);
            }
        }

        void writeBgfxFormat(bx::WriterI* _writer) const
        {
            for (uint32_t ii = 0, end = m_groups.count(); ii < end; ++ii)
            {
                const Group& group = m_groups[ii];
                ::write(_writer
                      , (const uint8_t*)group.m_vertexData
                      , group.m_numVertices
                      , m_decl
                      , (const uint16_t*)group.m_indexData
                      , group.m_numIndices
                      , group.m_materialName
                      , group.m_prims.elements()
                      , group.m_prims.count()
                      );
            }
        }

        void write(bx::WriterI* _writer, uint16_t _id = ResourceId::Invalid) const
        {
            writeBgfxFormat(_writer);

            bx::write(_writer, CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC);
            bx::write(_writer, _id);
            bx::write(_writer, m_normScale);
            bx::write(_writer, CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE);
        }

        bool hasMem() const
        {
            for (uint32_t ii = m_groups.count(); ii--; )
            {
                const Group& group = m_groups[ii];
                if (NULL == group.m_vertexData)
                {
                    return false;
                }
            }

            return true;
        }

        void freeMem(bool _delayed = false)
        {
            for (uint32_t ii = m_groups.count(); ii--; )
            {
                Group& group = m_groups[ii];

                if (NULL != group.m_vertexData)
                {
                    BX_FREE(_delayed ? cs::delayedFree : dm::mainAlloc, group.m_vertexData);
                    group.m_vertexData = NULL;
                }

                if (NULL != group.m_indexData)
                {
                    BX_FREE(_delayed ? cs::delayedFree : dm::mainAlloc, group.m_indexData);
                    group.m_indexData = NULL;
                }
            }
        }

        void destroy()
        {
            if (m_groups.isInitialized())
            {
                for (uint32_t ii = m_groups.count(); ii--; )
                {
                    if (NULL != m_groups[ii].m_vertexData)  { BX_FREE(dm::mainAlloc, m_groups[ii].m_vertexData); }
                    if (NULL != m_groups[ii].m_indexData)   { BX_FREE(dm::mainAlloc, m_groups[ii].m_indexData);  }
                }
                m_groups.reset();
            }

            if (m_bufferHandles.isInitialized())
            {
                for (uint32_t ii = m_bufferHandles.count(); ii--; )
                {
                    if (bgfx::isValid(m_bufferHandles[ii].m_vbh)) { bgfx::destroyVertexBuffer(m_bufferHandles[ii].m_vbh); }
                    if (bgfx::isValid(m_bufferHandles[ii].m_ibh)) { bgfx::destroyIndexBuffer(m_bufferHandles[ii].m_ibh);  }
                }
                m_bufferHandles.reset();
            }
        }

        GeometryHandleArray m_bufferHandles;
    };

    struct MeshResourceManager : public ResourceManagerT<Mesh, MeshImpl, MeshHandle, CS_MAX_MESHES>
    {
        MeshHandle load(const void* _data, uint32_t _size, const char* _ext)
        {
            dm::MemoryReader reader(_data, _size);

            MeshImpl* mesh = this->createObj();
            const MeshHandle handle = this->getHandle(mesh);

            const bool loaded = mesh->load(&reader, _ext);

            if (!loaded)
            {
                return MeshHandle::invalid();
            }

            return this->acquire(handle);
        }

        MeshHandle load(const char* _path, void* _userData, dm::StackAllocatorI* _stack)
        {
            const char* ext = dm::fileExt(_path);
            const bool isBinary = (0 == strcmp(ext, "bin"));

            dm::CrtFileReader reader;
            if (0 != reader.open(_path, isBinary))
            {
                return MeshHandle::invalid();
            }

            MeshImpl* mesh = this->createObj();
            const MeshHandle handle = this->getHandle(mesh);

            const bool loaded = mesh->load(&reader, ext, _userData, _stack);
            reader.close();

            if (!loaded)
            {
                return MeshHandle::invalid();
            }

            return this->acquire(handle);
        }
    };
    static MeshResourceManager* s_meshes;

    static inline MeshHandle loadSphere()
    {
        MeshHandle sphere = s_meshes->load(g_sphereMesh, g_sphereMeshSize, "bin");
        setName(sphere, "Sphere");
        createGpuBuffers(sphere);
        return sphere;
    }

    MeshHandle meshSphere()
    {
        static const MeshHandle sc_sphere = acquire(loadSphere());
        return sc_sphere;
    }

    uint32_t meshNumGroups(MeshHandle _mesh)
    {
        const MeshImpl* mesh = s_meshes->getImpl(_mesh);
        return mesh->m_groups.count();
    }

    MeshHandle meshLoad(const void* _data, uint32_t _size, const char* _ext)
    {
        return s_meshes->load(_data, _size, _ext);
    }

    MeshHandle meshLoad(const char* _filePath, void* _userData, dm::StackAllocatorI* _stack)
    {
        return s_meshes->load(_filePath, _userData, _stack);
    }

    MeshHandle meshLoad(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack)
    {
        return s_meshes->read(_reader, _stack);
    }

    bool meshSave(MeshHandle _mesh, const char* _filePath)
    {
        const MeshImpl* mesh = s_meshes->getImpl(_mesh);

        if (!mesh->hasMem())
        {
            return false;
        }

        bx::CrtFileWriter writer;
        if (0 != writer.open(_filePath))
        {
            return false;
        }

        mesh->writeBgfxFormat(&writer);
        writer.close();

        return true;
    }

    // MeshInstance.
    //-----

    MeshInstance::MeshInstance()
    {
        m_scale    = 1.0f;
        m_scaleAdj = 1.0f;
        m_rot[0]   = 0.0f;
        m_rot[1]   = 0.0f;
        m_rot[2]   = 0.0f;
        m_pos[0]   = 0.0f;
        m_pos[1]   = 0.0f;
        m_pos[2]   = 0.0f;
        m_mesh     = MeshHandle::invalid();
        m_selGroup = 0;
    }

    MeshInstance::MeshInstance(const MeshInstance& _other)
    {
        m_scale    = _other.m_scale;
        m_scaleAdj = _other.m_scaleAdj;
        m_rot[0]   = _other.m_rot[0];
        m_rot[1]   = _other.m_rot[1];
        m_rot[2]   = _other.m_rot[2];
        m_pos[0]   = _other.m_pos[0];
        m_pos[1]   = _other.m_pos[1];
        m_pos[2]   = _other.m_pos[2];
        m_mesh     = _other.m_mesh;
        m_selGroup = _other.m_selGroup;

        const uint32_t matCount = _other.m_materials.max();
        m_materials.reinit(matCount, dm::mainAlloc);

        m_materials.reserve(matCount);
        for (uint32_t ii = matCount; ii--; )
        {
            m_materials[ii] = _other.m_materials[ii];
        }

        cs::acquire(this);
    }

    MeshInstance::~MeshInstance()
    {
        cs::release(this);
    }

    void MeshInstance::set(MeshHandle _mesh)
    {
        if (isValid(m_mesh))
        {
            cs::release(m_mesh);
        }
        m_mesh = cs::acquire(_mesh);

        const uint32_t groupsCount = meshNumGroups(_mesh);

        if (!m_materials.isInitialized())
        {
            m_materials.init(groupsCount, dm::mainAlloc);
            m_materials.fillWith(cs::MaterialHandle::invalid());
        }
        else
        {
            const uint32_t currMatCount = m_materials.max();
            if (groupsCount < currMatCount) // Shrink.
            {
                m_materials.cut(groupsCount);
            }
            else // Expand.
            {
                if (groupsCount > m_materials.max())
                {
                    m_materials.resize(groupsCount);
                }

                for (uint16_t ii = groupsCount-currMatCount; ii--; )
                {
                    m_materials.add(cs::MaterialHandle::invalid());
                }
            }
        }
    }

    void MeshInstance::set(MaterialHandle _material, uint32_t _groupIdx)
    {
        if (_groupIdx < m_materials.max())
        {
            if (isValid(m_materials[_groupIdx]))
            {
                cs::release(m_materials[_groupIdx]);
            }
            m_materials[_groupIdx] = cs::acquire(_material);
        }
    }

    cs::MaterialHandle MeshInstance::getActiveMaterial() const
    {
        return m_materials[m_selGroup];
    }

    float* MeshInstance::computeMtx()
    {
        const MeshImpl* mesh = s_meshes->getImpl(m_mesh);

        bx::mtxSRT(m_mtx
                 , m_scale*m_scaleAdj*mesh->m_normScale
                 , m_scale*m_scaleAdj*mesh->m_normScale
                 , m_scale*m_scaleAdj*mesh->m_normScale
                 , m_rot[0]*dm::twoPi
                 , m_rot[1]*dm::twoPi
                 , m_rot[2]*dm::twoPi
                 , m_pos[0]
                 , m_pos[1]
                 , m_pos[2]
                 );

        return m_mtx;
    }

    MeshInstance* acquire(const MeshInstance* _inst)
    {
        if (isValid(_inst->m_mesh))
        {
            cs::acquire(_inst->m_mesh);
        }

        for (uint16_t ii = 0, end = _inst->m_materials.max(); ii < end; ++ii)
        {
            if (isValid(_inst->m_materials[ii]))
            {
                cs::acquire(_inst->m_materials[ii]);
            }
        }

        return const_cast<MeshInstance*>(_inst);
    }

    MeshInstance& acquire(const MeshInstance& _inst)
    {
        return *acquire(&_inst);
    }

    void release(const MeshInstance* _inst)
    {
        if (isValid(_inst->m_mesh))
        {
            cs::release(_inst->m_mesh);
        }

        for (uint16_t ii = 0, end = _inst->m_materials.max(); ii < end; ++ii)
        {
            if (isValid(_inst->m_materials[ii]))
            {
                cs::release(_inst->m_materials[ii]);
            }
        }
    }

    // Environment.
    //-----

    struct EnvironmentImpl : public Environment, public ReadWriteI<EnvHandle>
    {
        EnvironmentImpl()
        {
            m_cubemap[Skybox] = TextureHandle::invalid();
            m_cubemap[Pmrem]  = TextureHandle::invalid();
            m_cubemap[Iem]    = TextureHandle::invalid();

            m_origSkybox = TextureHandle::invalid();

            memset(m_lights, 0, sizeof(m_lights));
            m_edgeFixup = cmft::EdgeFixup::None;
            m_lightsNum = 0;
            for (uint8_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
            {
                m_lightUseBackgroundColor[ii] = false;
            }
        }

        ~EnvironmentImpl()
        {
            destroy();
        }

        static inline void imageToTextureRef(cs::TextureHandle& _texture, cmft::Image& _image)
        {
            if (isValid(_texture))
            {
                release(_texture);
            }
            _texture = s_textures->create();

            TextureImpl* tex = s_textures->getImpl(_texture);
            tex->m_size     = _image.m_dataSize;
            tex->m_data     = (uint8_t*)_image.m_data;
            tex->m_numMips  = _image.m_numMips;
            tex->m_width    = (uint16_t)_image.m_width;
            tex->m_height   = (uint16_t)_image.m_height;
            tex->m_format   = cmftToBgfx(_image.m_format).bgfxFormat();
            tex->m_type     = TextureImpl::Type::TexCube;
            tex->m_freeData = false;
        }

        void create(uint32_t _rgba = 0x303030ff, uint32_t _size = 128)
        {
            dm::StackAllocScope scope(dm::stackAlloc);

            // Create cubemap image.
            cmft::Image cubemap;
            cmft::imageCreate(cubemap, _size, _size, _rgba, 1, 6, cmft::TextureFormat::BGRA8, dm::stackAlloc);

            // Setup cubemaps.
            cmft::imageCopy(m_cubemapImage[Environment::Skybox], cubemap);
            cmft::imageCopy(m_cubemapImage[Environment::Pmrem ], cubemap);
            cmft::imageCopy(m_cubemapImage[Environment::Iem   ], cubemap);
            imageToTextureRef(m_cubemap[Environment::Skybox], m_cubemapImage[Environment::Skybox]);
            imageToTextureRef(m_cubemap[Environment::Pmrem],  m_cubemapImage[Environment::Pmrem]);
            imageToTextureRef(m_cubemap[Environment::Iem],    m_cubemapImage[Environment::Iem]);

            // Cleanup.
            cmft::imageUnload(cubemap, dm::stackAlloc);
        }

        // Notice this takes ownership of '_image'.
        bool load(Environment::Enum _which, cmft::Image& _image)
        {
            if (!cmft::imageIsEnvironmentMap(_image, true))
            {
                cmft::imageUnload(_image);
                return false;
            }

            // Resize if input image is very big.
            const uint32_t cubeFaceSize = cmft::imageGetCubemapFaceSize(_image);
            if (cubeFaceSize > 1024)
            {
                cmft::imageResize(_image, 1024);
            }

            const bool isCubemap = cmft::imageIsCubemap(_image);
            const bool isLatlong = cmft::imageIsLatLong(_image);
            const TextureFormatInfo tfi = cmftToBgfx(_image.m_format);

            dm::StackAllocScope scope(dm::stackAlloc);

            cmft::ImageHardRef imageRgba32f;
            cmft::imageRefOrConvert(imageRgba32f, cmft::TextureFormat::RGBA32F, _image, dm::stackAlloc);

            // Cubemap image.
            // Try to go the shortest path possible.
            if (isLatlong)
            {
                dm::StackAllocScope scope(dm::stackAlloc);

                cmft::Image tmp;
                cmft::imageCubemapFromLatLong(tmp, imageRgba32f, true, dm::stackAlloc);
                cmft::imageConvert(m_cubemapImage[_which], tfi.convert() ? tfi.cmftFormat() : _image.m_format, tmp);
                cmft::imageUnload(tmp, dm::stackAlloc);
            }
            else if (tfi.convert())
            {
                if (isCubemap)
                {
                    cmft::imageConvert(m_cubemapImage[_which], tfi.cmftFormat(), imageRgba32f);
                }
                else
                {
                    dm::StackAllocScope scope(dm::stackAlloc);

                    cmft::Image cubemap;
                    cmft::imageToCubemap(cubemap, imageRgba32f, dm::stackAlloc);
                    cmft::imageConvert(m_cubemapImage[_which], tfi.cmftFormat(), cubemap);
                    cmft::imageUnload(cubemap, dm::stackAlloc);
                }
            }
            else //if (!tfi.convert()).
            {
                if (isCubemap)
                {
                    cmft::imageMove(m_cubemapImage[_which], _image);
                }
                else
                {
                    cmft::imageToCubemap(m_cubemapImage[_which], _image);
                }
            }

            // Invalidate orig skybox image and texture.
            if (Environment::Skybox == _which)
            {
                if (isValid(m_origSkybox))
                {
                    release(m_origSkybox);
                    m_origSkybox = cs::TextureHandle::invalid();
                }
            }

            // Setup texture.
            imageToTextureRef(m_cubemap[_which], m_cubemapImage[_which]);

            // Cleanup.
            cmft::imageUnload(imageRgba32f, dm::stackAlloc);
            cmft::imageUnload(_image);

            return true;
        }

        bool load(Environment::Enum _which, const char* _path)
        {
            cmft::Image cubemap;
            if (cmft::imageLoad(cubemap, _path))
            {
                return load(_which, cubemap);
            }

            return false;
        }

        void load(const char* _skybox, const char* _pmrem, const char* _iem)
        {
            load(Environment::Skybox, _skybox);
            load(Environment::Pmrem,  _pmrem);
            load(Environment::Iem,    _iem);
        }

        bool load(Environment::Enum _which, const void* _data, uint32_t _dataSize)
        {
            cmft::Image cubemap;
            if (cmft::imageLoad(cubemap, _data, _dataSize))
            {
                return load(_which, cubemap);
            }

            return false;
        }

        void load(const void* _skyboxData, uint32_t _skyboxSize
                , const void* _pmremData,  uint32_t _pmremSize
                , const void* _iemData,    uint32_t _iemSize
                )
        {
            load(Environment::Skybox, _skyboxData, _skyboxSize);
            load(Environment::Pmrem,  _pmremData,  _pmremSize);
            load(Environment::Iem,    _iemData,    _iemSize);
        }

        void resize(Environment::Enum _which, uint32_t _faceSize)
        {
            // Resize image.
            cmft::imageResize(m_cubemapImage[_which], _faceSize, _faceSize);

            // Setup and send texture to GPU.
            imageToTextureRef(m_cubemap[_which], m_cubemapImage[_which]);
            createGpuBuffers(m_cubemap[_which]);
        }

        void transformArg(Environment::Enum _which, va_list _argList)
        {
            // Transform image.
            cmft::imageTransformArg(m_cubemapImage[_which], _argList);

            // Setup and send texture to GPU.
            imageToTextureRef(m_cubemap[_which], m_cubemapImage[_which]);
            createGpuBuffers(m_cubemap[_which]);
        }

        void convert(Environment::Enum _which, cmft::TextureFormat::Enum _format)
        {
            // Convert image.
            cmft::imageConvert(m_cubemapImage[_which], _format);

            // Setup and send texture to GPU.
            imageToTextureRef(m_cubemap[_which], m_cubemapImage[_which]);
            createGpuBuffers(m_cubemap[_which]);
        }

        void tonemapSkybox(float _gamma, float _minLum, float _lumRange)
        {
            const bool hasOrig = cmft::imageIsValid(m_origSkyboxImage);

            if (!hasOrig)
            {
                // Move skybox image and texture to orig.
                cmft::imageMove(m_origSkyboxImage, m_cubemapImage[Environment::Skybox]);
                m_origSkybox = m_cubemap[Environment::Skybox];
                m_cubemap[Environment::Skybox] = cs::TextureHandle::invalid();
            }

            // Tonemapping is done in rgba32f format.
            cmft::Image imageRgba32f;
            cmft::imageConvert(imageRgba32f, cmft::TextureFormat::RGBA32F, m_origSkyboxImage);

            // Get parameters.
            const uint32_t totalNumChannels = cmft::imageGetNumPixels(imageRgba32f)
                                            * cmft::getImageDataInfo(cmft::TextureFormat::RGBA32F).m_numChanels;
            const float* end = (const float*)imageRgba32f.m_data + totalNumChannels;

            // Tonemap.
            const float invLumRange = 1.0f/_lumRange;
            for (float* dst = (float*)imageRgba32f.m_data; dst < end; dst+=4)
            {
                dst[0] = (powf(dst[0], _gamma) - _minLum)*invLumRange;
                dst[1] = (powf(dst[1], _gamma) - _minLum)*invLumRange;
                dst[2] = (powf(dst[2], _gamma) - _minLum)*invLumRange;
                //dst[3] = leave as is.
            }

            if (cmft::TextureFormat::RGBA32F == m_cubemapImage[Environment::Skybox].m_format)
            {
                cmft::imageMove(m_cubemapImage[Environment::Skybox], imageRgba32f);
            }
            else
            {
                cmft::imageConvert(m_cubemapImage[Environment::Skybox], m_cubemapImage[Environment::Skybox].m_format, imageRgba32f);
                cmft::imageUnload(imageRgba32f);
            }

            // Setup and send texture to GPU.
            imageToTextureRef(m_cubemap[Environment::Skybox], m_cubemapImage[Environment::Skybox]);
            createGpuBuffers(m_cubemap[Environment::Skybox]);
        }

        void restoreOriginalSkybox()
        {
            // Move orig to skybox image and texture.
            if (cmft::imageIsValid(m_origSkyboxImage))
            {
                // Move image.
                cmft::imageMove(m_cubemapImage[Environment::Skybox], m_origSkyboxImage);

                // Move texture.
                if (isValid(m_cubemap[Environment::Skybox]))
                {
                    release(m_cubemap[Environment::Skybox]);
                }
                m_cubemap[Environment::Skybox] = m_origSkybox;
                m_origSkybox = cs::TextureHandle::invalid();
            }
        }

        void createGpuBuffers(cs::TextureHandle _cubemap)
        {
            enum { CubeTexFlags = BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP|BGFX_TEXTURE_W_CLAMP };

            TextureImpl* cube = s_textures->getImpl(_cubemap);
            if (!isValid(cube->m_bgfxHandle))
            {
                cube->createGpuBuffers(CubeTexFlags);
            }
        }

        void createGpuBuffers()
        {
            createGpuBuffers(m_cubemap[Environment::Skybox]);
            createGpuBuffers(m_cubemap[Environment::Pmrem]);
            createGpuBuffers(m_cubemap[Environment::Iem]);
        }

        void read(dm::ReaderSeekerI* _reader, EnvHandle _handle = EnvHandle::invalid(), dm::StackAllocatorI* _stack = dm::stackAlloc)
        {
            BX_UNUSED(_stack);

            this->destroy();

            uint16_t id;
            bx::read(_reader, id);
            resourceMap(id, _handle);

            bx::read(_reader, m_lightsNum);
            bx::read(_reader, m_edgeFixup);
            for (uint16_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
            {
                bx::read(_reader, m_lightUseBackgroundColor[ii]);
            }
            for (uint16_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
            {
                bx::read(_reader, m_lights[ii].m_colorStrenght, 4*sizeof(float));
                bx::read(_reader, m_lights[ii].m_dirEnabled,    4*sizeof(float));
            }
            for (uint16_t ii = 0; ii < Environment::Count; ++ii)
            {
                cmft::Image image;
                bx::read(_reader, image.m_width);
                bx::read(_reader, image.m_height);
                bx::read(_reader, image.m_dataSize);
                bx::read(_reader, image.m_format);
                bx::read(_reader, image.m_numMips);
                bx::read(_reader, image.m_numFaces);
                image.m_data = BX_ALLOC(dm::mainAlloc, image.m_dataSize);
                bx::read(_reader, image.m_data, image.m_dataSize);
                this->load(Environment::Enum(ii), image);
            }
        }

        void write(bx::WriterI* _writer, uint16_t _id = ResourceId::Invalid) const
        {
            bx::write(_writer, _id);
            bx::write(_writer, m_lightsNum);
            bx::write(_writer, m_edgeFixup);
            for (uint16_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
            {
                bx::write(_writer, m_lightUseBackgroundColor[ii]);
            }
            for (uint16_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
            {
                bx::write(_writer, m_lights[ii].m_colorStrenght, 4*sizeof(float));
                bx::write(_writer, m_lights[ii].m_dirEnabled,    4*sizeof(float));
            }
            for (uint16_t ii = 0; ii < Environment::Count; ++ii)
            {
                bx::write(_writer, m_cubemapImage[ii].m_width);
                bx::write(_writer, m_cubemapImage[ii].m_height);
                bx::write(_writer, m_cubemapImage[ii].m_dataSize);
                bx::write(_writer, m_cubemapImage[ii].m_format);
                bx::write(_writer, m_cubemapImage[ii].m_numMips);
                bx::write(_writer, m_cubemapImage[ii].m_numFaces);
                bx::write(_writer, m_cubemapImage[ii].m_data, m_cubemapImage[ii].m_dataSize);
            }
        }

        void freeMem()
        {
            cmft::imageUnload(m_cubemapImage[Skybox]);
            cmft::imageUnload(m_cubemapImage[Pmrem]);
            cmft::imageUnload(m_cubemapImage[Iem]);
            cmft::imageUnload(m_origSkyboxImage);
        }

        void destroy()
        {
            freeMem();

            #define CS_SAFE_TEXTURE_RELEASE(_tex) if (isValid(_tex)) { release(_tex); }
            CS_SAFE_TEXTURE_RELEASE(m_cubemap[Skybox]);
            CS_SAFE_TEXTURE_RELEASE(m_cubemap[Pmrem]);
            CS_SAFE_TEXTURE_RELEASE(m_cubemap[Iem]);

            CS_SAFE_TEXTURE_RELEASE(m_origSkybox);
            #undef CS_SAFE_TEXTURE_RELEASE
        }
    };

    struct EnvironmentResourceManager : public ResourceManagerT<Environment, EnvironmentImpl, EnvHandle, CS_MAX_ENVIRONMENTS>
    {
        EnvHandle create(uint32_t _rgba)
        {
            EnvironmentImpl* env = this->createObj();
            env->create(_rgba);
            env->createGpuBuffers();

            const EnvHandle handle = this->getHandle(env);

            return this->acquire(handle);
        }

        EnvHandle load(const char* _skyboxPath, const char* _pmremPath, const char* _iemPath)
        {
            EnvironmentImpl* env = this->createObj();
            env->load(_skyboxPath, _pmremPath, _iemPath);
            env->createGpuBuffers();

            const EnvHandle handle = this->getHandle(env);

            return this->acquire(handle);
        }

        EnvHandle load(const void* _skyboxData, uint32_t _skyboxSize
                     , const void* _pmremData,  uint32_t _pmremSize
                     , const void* _iemData,    uint32_t _iemSize
                     )
        {
            EnvironmentImpl* env = this->createObj();
            env->load(_skyboxData, _skyboxSize, _pmremData, _pmremSize, _iemData, _iemSize);
            env->createGpuBuffers();

            const EnvHandle handle = this->getHandle(env);

            return this->acquire(handle);
        }
    };
    static EnvironmentResourceManager* s_environments;

    EnvHandle envCreateCmftStudioLogo()
    {
        const EnvHandle env = s_environments->load(g_logoSkybox, g_logoSkyboxSize
                                                 , g_logoPmrem,  g_logoPmremSize
                                                 , g_logoIem,    g_logoIemSize
                                                 );
        s_environments->getImpl(env)->m_edgeFixup = cmft::EdgeFixup::Warp;
        setName(env, "CmftStudio");

        return env;
    }

    EnvHandle envCreate(uint32_t _rgba)
    {
        return s_environments->create(_rgba);
    }

    EnvHandle envCreate(const char* _skyboxPath, const char* _pmremPath, const char* _iemPath)
    {
        return s_environments->load(_skyboxPath, _pmremPath, _iemPath);
    }

    EnvHandle envCreate(dm::ReaderSeekerI* _reader)
    {
        return s_environments->read(_reader);
    }

    void envLoad(EnvHandle _handle, Environment::Enum _which, cmft::Image& _image)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->load(_which, _image);
    }

    bool envLoad(EnvHandle _handle, Environment::Enum _which, const char* _filePath)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        return env->load(_which, _filePath);
    }

    void envTransform_UseMacroInstead(EnvHandle _handle, Environment::Enum _which, ...)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);

        va_list argList;
        va_start(argList, _which);
        env->transformArg(_which, argList);
        va_end(argList);
    }

    void envResize(EnvHandle _handle, Environment::Enum _which, uint32_t _faceSize)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->resize(_which, _faceSize);
    }

    void envConvert(EnvHandle _handle, Environment::Enum _which, cmft::TextureFormat::Enum _format)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->convert(_which, _format);
    }

    void envTonemap(EnvHandle _handle, float _gamma, float _minLum, float _lumRange)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->tonemapSkybox(_gamma, _minLum, _lumRange);
    }

    void envRestoreSkybox(EnvHandle _handle)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->restoreOriginalSkybox();
    }

    cmft::Image& envGetImage(EnvHandle _handle, Environment::Enum _which)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        return env->m_cubemapImage[_which];
    }

    // Lists.
    //-----

    template <typename Ty>
    void listRemoveRelease(Ty& _list, uint16_t _idx)
    {
        release(_list[_idx]);
        _list.remove(_idx);
    }

    void listRemoveRelease(TextureList& _list, uint16_t _idx)
    {
        listRemoveRelease<TextureList>(_list, _idx);
    }

    void listRemoveRelease(MaterialList& _list, uint16_t _idx)
    {
        listRemoveRelease<MaterialList>(_list, _idx);
    }

    void listRemoveRelease(MeshList& _list, uint16_t _idx)
    {
        listRemoveRelease<MeshList>(_list, _idx);
    }

    void listRemoveRelease(EnvList& _list, uint16_t _idx)
    {
        listRemoveRelease<EnvList>(_list, _idx);
    }

    void listRemoveRelease(MeshInstanceList& _list, uint16_t _idx)
    {
        release(&_list[_idx]);
        _list.removeAt(_idx);
    }

    template <typename Ty>
    void listRemoveReleaseAll(Ty& _list)
    {
        for (uint16_t ii = 0, end = _list.count(); ii < end; ++ii)
        {
            release(_list[ii]);
        }
        _list.reset();
    }

    void listRemoveReleaseAll(TextureList& _list)
    {
        listRemoveReleaseAll<TextureList>(_list);
    }

    void listRemoveReleaseAll(MaterialList& _list)
    {
        listRemoveReleaseAll<MaterialList>(_list);
    }

    void listRemoveReleaseAll(MeshList& _list)
    {
        listRemoveReleaseAll<MeshList>(_list);
    }

    void listRemoveReleaseAll(EnvList& _list)
    {
        listRemoveReleaseAll<EnvList>(_list);
    }

    void listRemoveReleaseAll(MeshInstanceList& _list)
    {
        for (uint16_t ii = _list.count(); ii--; )
        {
            release(&_list[ii]);
            _list.removeAt(ii);
        }
    }

    // Context functions.
    //-----

    static void* m_contextMemoryBlock;

    void initContext()
    {
        // Initialize resource managers.
        enum
        {
            TexOffset = 0,
            MatOffset = TexOffset + sizeof(TextureResourceManager),
            MshOffset = MatOffset + sizeof(MaterialResourceManager),
            EnvOffset = MshOffset + sizeof(MeshResourceManager),
            TotalSize = EnvOffset + sizeof(EnvironmentResourceManager),
        };

        m_contextMemoryBlock = BX_ALLOC(dm::staticAlloc, TotalSize);

        uint8_t* mem = (uint8_t*)m_contextMemoryBlock;
        s_textures     = ::new (mem+TexOffset) TextureResourceManager();
        s_materials    = ::new (mem+MatOffset) MaterialResourceManager();
        s_meshes       = ::new (mem+MshOffset) MeshResourceManager();
        s_environments = ::new (mem+EnvOffset) EnvironmentResourceManager();

        // Initialize loaders.
        initGeometryLoaders();
    }

    void destroyContext()
    {
        s_environments->~EnvironmentResourceManager();
        s_meshes->~MeshResourceManager();
        s_materials->~MaterialResourceManager();
        s_textures->~TextureResourceManager();
        BX_FREE(dm::staticAlloc, m_contextMemoryBlock);
    }

    void freeHostMem(TextureHandle _handle)
    {
        TextureImpl* tex = s_textures->getImpl(_handle);
        tex->freeMem();
    }

    void freeHostMem(MeshHandle _handle)
    {
        MeshImpl* mesh = s_meshes->getImpl(_handle);
        mesh->freeMem();
    }

    void freeHostMem(EnvHandle _handle)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->freeMem();
    }

    void setTexture(TextureUniform::Enum _which, cs::TextureHandle _handle, uint32_t _flags)
    {
        if (isValid(_handle))
        {
            bgfx::setTexture(s_textureStage[_which], s_uniforms.u_tex[_which], textureGetBgfxHandle(_handle), _flags);
        }
    }

    void setTexture(TextureUniform::Enum _which, bgfx::TextureHandle _handle, uint32_t _flags)
    {
        if (isValid(_handle))
        {
            bgfx::setTexture(s_textureStage[_which], s_uniforms.u_tex[_which], _handle, _flags);
        }
    }

    void setTexture(TextureUniform::Enum _which, bgfx::FrameBufferHandle _handle, uint32_t _flags, uint8_t _attachment)
    {
        if (isValid(_handle))
        {
            bgfx::setTexture(s_textureStage[_which], s_uniforms.u_tex[_which], _handle, _attachment, _flags);
        }
    }

    void setMaterial(MaterialHandle _handle)
    {
        // Get material object.
        const Material& currentMaterial = getObj(_handle);

        // Set parameters.
        memcpy(s_uniforms.m_material, currentMaterial.m_data, sizeof(s_uniforms.m_material));

        // Set textures.
        setTexture(TextureUniform::Color,        currentMaterial.get(Material::Albedo));
        setTexture(TextureUniform::Normal,       currentMaterial.get(Material::Normal));
        setTexture(TextureUniform::Surface,      currentMaterial.get(Material::Surface));
        setTexture(TextureUniform::Reflectivity, currentMaterial.get(Material::Reflectivity));
        setTexture(TextureUniform::Occlusion,    currentMaterial.get(Material::Occlusion));
        setTexture(TextureUniform::Emissive,     currentMaterial.get(Material::Emissive));
    }

    void submit(uint8_t _view
              , MeshInstance& _instance
              , Program::Enum _prog
              , EnvHandle _env
              , uint64_t _state
              )
    {
        MeshImpl* mesh = s_meshes->getImpl(_instance.m_mesh);
        mesh->submit(_view, _prog, _env, _instance.computeMtx(), _instance.m_materials.elements(), _state);
    }

    void submit(uint8_t _view
              , MeshHandle _mesh
              , Program::Enum _prog
              , const float* _mtx
              , const MaterialHandle* _materials
              , EnvHandle _env
              , uint64_t _state
              )
    {
        MeshImpl* mesh = s_meshes->getImpl(_mesh);
        mesh->submit(_view, _prog, _env, _mtx, _materials, _state);
    }

    void submit(uint8_t _view
              , MeshHandle _mesh
              , Program::Enum _prog
              , const float* _mtx
              , const MaterialHandle* _materials
              , EnvHandle _nextEnv
              , EnvHandle _currEnv
              , float _progress
              , uint64_t _state
              )
    {
        MeshImpl* mesh = s_meshes->getImpl(_mesh);
        mesh->submit(_view, _prog, _nextEnv, _currEnv, _progress, _mtx, _materials, _state);
    }

    void submit(uint8_t _view
              , MeshHandle _mesh
              , Program::Enum _prog
              , const float* _mtx
              , uint32_t _groupIdx
              , MaterialHandle _material
              , EnvHandle _env
              , uint64_t _state
              )
    {
        MeshImpl* mesh = s_meshes->getImpl(_mesh);
        mesh->submit(_view, _prog, _env, _mtx, _groupIdx, _material, _state);
    }

    void setEnv(EnvHandle _handle)
    {
        enum { Flags = BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC };

        const EnvironmentImpl* env = s_environments->getImpl(_handle);
        cs::setTexture(cs::TextureUniform::Iem,    env->m_cubemap[Environment::Iem],    Flags);
        cs::setTexture(cs::TextureUniform::Pmrem,  env->m_cubemap[Environment::Pmrem],  Flags);

        cs::Uniforms& uniforms = cs::getUniforms();
        uniforms.m_mipCount  = env->m_cubemapImage[Environment::Pmrem].m_numMips;
        uniforms.m_mipSize   = dm::utof(env->m_cubemapImage[Environment::Pmrem].m_width);
        uniforms.m_edgeFixup = (cmft::EdgeFixup::Warp == env->m_edgeFixup ? 1.0f : 0.0f);
    }

    void setEnvTransition(EnvHandle _from)
    {
        enum { Flags = BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC };

        const EnvironmentImpl* env = s_environments->getImpl(_from);
        cs::setTexture(cs::TextureUniform::Skybox,    env->m_cubemap[Environment::Iem],   Flags);
        cs::setTexture(cs::TextureUniform::PmremPrev, env->m_cubemap[Environment::Pmrem], Flags);

        cs::Uniforms& uniforms = cs::getUniforms();
        uniforms.m_prevMipCount  = env->m_cubemapImage[Environment::Pmrem].m_numMips;
        uniforms.m_prevMipSize   = dm::utof(env->m_cubemapImage[Environment::Pmrem].m_width);
        uniforms.m_prevEdgeFixup = (cmft::EdgeFixup::Warp == env->m_edgeFixup ? 1.0f : 0.0f);
    }

    Texture& getObj(TextureHandle _handle)
    {
        return *s_textures->getObj(_handle);
    }

    Material& getObj(MaterialHandle _handle)
    {
        return *s_materials->getObj(_handle);
    }

    Mesh& getObj(MeshHandle _handle)
    {
        return *s_meshes->getObj(_handle);
    }

    Environment& getObj(EnvHandle _handle)
    {
        return *s_environments->getObj(_handle);
    }

    void setName(TextureHandle _handle, const char* _name)
    {
        return s_textures->setName(_handle, _name);
    }

    void setName(MaterialHandle _handle, const char* _name)
    {
        return s_materials->setName(_handle, _name);
    }

    void setName(MeshHandle _handle, const char* _name)
    {
        return s_meshes->setName(_handle, _name);
    }

    void setName(EnvHandle _handle, const char* _name)
    {
        return s_environments->setName(_handle, _name);
    }

    char* getName(TextureHandle _handle)
    {
        return s_textures->getName(_handle);
    }

    char* getName(MaterialHandle _handle)
    {
        return s_materials->getName(_handle);
    }

    char* getName(MeshHandle _handle)
    {
        return s_meshes->getName(_handle);
    }

    char* getName(EnvHandle _handle)
    {
        return s_environments->getName(_handle);
    }

    TextureHandle acquire(TextureHandle _handle)
    {
        return s_textures->acquire(_handle);
    }

    EnvHandle acquire(EnvHandle _handle)
    {
        return s_environments->acquire(_handle);
    }

    MeshHandle acquire(MeshHandle _handle)
    {
        return s_meshes->acquire(_handle);
    }

    MaterialHandle acquire(MaterialHandle _handle)
    {
        return s_materials->acquire(_handle);
    }

    void release(TextureHandle _handle)
    {
        s_textures->release(_handle);
    }

    void release(MaterialHandle _handle)
    {
        s_materials->release(_handle);
    }

    void release(MeshHandle _handle)
    {
        s_meshes->release(_handle);
    }

    void release(EnvHandle _handle)
    {
        s_environments->release(_handle);
    }

    TextureHandle readTexture(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack)
    {
        // Read name.
        char name[32];
        uint32_t len;
        bx::read(_reader, len);
        bx::read(_reader, name, len);
        name[len] = '\0';

        // Read object.
        const TextureHandle texture = s_textures->read(_reader, _stack);
        setName(texture, name);

        return texture;
    }

    MaterialHandle readMaterial(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack)
    {
        // Read name.
        char name[32];
        uint32_t len;
        bx::read(_reader, len);
        bx::read(_reader, name, len);
        name[len] = '\0';

        // Read object.
        const MaterialHandle material = s_materials->read(_reader, _stack);
        setName(material, name);

        return material;
    }

    MeshHandle readMesh(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack)
    {
        // Read name.
        char name[32];
        uint32_t len;
        bx::read(_reader, len);
        bx::read(_reader, name, len);
        name[len] = '\0';

        // Read object.
        const MeshHandle mesh = s_meshes->read(_reader, _stack);
        setName(mesh, name);

        return mesh;
    }

    EnvHandle readEnv(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack)
    {
        // Read name.
        char name[32];
        uint32_t len;
        bx::read(_reader, len);
        bx::read(_reader, name, len);
        name[len] = '\0';

        // Read object.
        const EnvHandle env = s_environments->read(_reader, _stack);
        setName(env, name);

        return env;
    }

    void readMeshInstance(dm::ReaderSeekerI* _reader, MeshInstance* _instance)
    {
        bx::read(_reader, _instance->m_scale);
        bx::read(_reader, _instance->m_scaleAdj);
        bx::read(_reader, _instance->m_rot, 3*sizeof(float));
        bx::read(_reader, _instance->m_pos, 3*sizeof(float));

        uint16_t id;
        bx::read(_reader, id);
        resourceResolve(&_instance->m_mesh, id);

        uint16_t materialCount;
        bx::read(_reader, materialCount);
        _instance->m_materials.reinit(materialCount, dm::mainAlloc);

        _instance->m_materials.reserve(materialCount);
        for (uint16_t ii = 0, end = materialCount; ii < end; ++ii)
        {
            bx::read(_reader, id);
            resourceResolve(&_instance->m_materials[ii], id);
        }
    }

    void createGpuBuffers(TextureHandle _handle, uint32_t _flags)
    {
        TextureImpl* tex = s_textures->getImpl(_handle);
        tex->createGpuBuffers(_flags);
    }

    void createGpuBuffers(MeshHandle _handle)
    {
        MeshImpl* mesh = s_meshes->getImpl(_handle);
        mesh->createGpuBuffers();
    }

    void createGpuBuffers(EnvHandle _handle)
    {
        EnvironmentImpl* env = s_environments->getImpl(_handle);
        env->createGpuBuffers();
    }

    void write(bx::WriterI* _writer, TextureHandle _handle)
    {
        const cs::TextureImpl* tex = s_textures->getImpl(_handle);
        const char* texName = s_textures->getName(_handle);

        // Write name.
        const char* name = (NULL == texName || '\0' == texName[0]) ? "UnnamedTexture" : texName;
        const uint32_t nameLen = (uint32_t)strlen(name);
        bx::write(_writer, nameLen);
        bx::write(_writer, name, nameLen);

        // Write object.
        tex->write(_writer, _handle.m_idx);
    }

    void write(bx::WriterI* _writer, MaterialHandle _handle)
    {
        const cs::MaterialImpl* mat = s_materials->getImpl(_handle);
        const char* matName = s_materials->getName(_handle);

        // Write name.
        const char* name = (NULL == matName || '\0' == matName[0]) ? "UnnamedMaterial" : matName;
        const uint32_t nameLen = (uint32_t)strlen(name);
        bx::write(_writer, nameLen);
        bx::write(_writer, name, nameLen);

        // Write object.
        mat->write(_writer, _handle.m_idx);
    }

    void write(bx::WriterI* _writer, MeshHandle _handle)
    {
        const cs::MeshImpl* mesh = s_meshes->getImpl(_handle);
        const char* meshName = s_meshes->getName(_handle);

        // Write name.
        const char* name = (NULL == meshName || '\0' == meshName[0]) ? "UnnamedMesh" : meshName;
        const uint32_t nameLen = (uint32_t)strlen(name);
        bx::write(_writer, nameLen);
        bx::write(_writer, name, nameLen);

        // Write object.
        mesh->write(_writer, _handle.m_idx);
    }

    void write(bx::WriterI* _writer, EnvHandle _handle)
    {
        const cs::EnvironmentImpl* env = s_environments->getImpl(_handle);
        const char* envName = s_environments->getName(_handle);

        // Write name.
        const char* name = (NULL == envName || '\0' == envName[0]) ? "UnnamedEnv" : envName;
        const uint32_t nameLen = (uint32_t)strlen(name);
        bx::write(_writer, nameLen);
        bx::write(_writer, name, nameLen);

        // Write object.
        env->write(_writer, _handle.m_idx);
    }

    void write(bx::WriterI* _writer, const MeshInstance& _inst)
    {
        bx::write(_writer, (float)_inst.m_scale);
        bx::write(_writer, (float)_inst.m_scaleAdj);
        bx::write(_writer, _inst.m_rot, 3*sizeof(float));
        bx::write(_writer, _inst.m_pos, 3*sizeof(float));
        bx::write(_writer, (uint16_t)_inst.m_mesh.m_idx);

        const uint16_t materialCount = _inst.m_materials.max();
        bx::write(_writer, (uint16_t)materialCount);
        for (uint16_t ii = 0, end = materialCount; ii < end; ++ii)
        {
            bx::write(_writer, (uint16_t)_inst.m_materials[ii].m_idx);
        }
    }

    void resourceGCFor(double _ms)
    {
        #if 0 //debug only
        printf("Env/Msh/Mat/Tex %d %d %d %d\n"
              , s_environments->count()
              , s_meshes->count()
              , s_materials->count()
              , s_textures->count()
              );
        #endif //debug only

        double maxMs = _ms;
        if (maxMs != 0.0) { maxMs = s_environments->gc(maxMs); }
        if (maxMs != 0.0) { maxMs = s_meshes->gc(maxMs);       }
        if (maxMs != 0.0) { maxMs = s_materials->gc(maxMs);    }
        if (maxMs != 0.0) { maxMs = s_textures->gc(maxMs);     }
    }

    void resourceGC(uint16_t _maxObjects)
    {
        #if 0 //debug only
        printf("Env/Msh/Mat/Tex %d %d %d %d\n"
              , s_environments->count()
              , s_meshes->count()
              , s_materials->count()
              , s_textures->count()
              );
        #endif //debug only

        uint16_t maxObj = _maxObjects;
        if (maxObj != 0) { maxObj = s_environments->gc(maxObj); }
        if (maxObj != 0) { maxObj = s_meshes->gc(maxObj);       }
        if (maxObj != 0) { maxObj = s_materials->gc(maxObj);    }
        if (maxObj != 0) { maxObj = s_textures->gc(maxObj);     }
    }

    void resourceGC()
    {
        #if 0 //debug only
        printf("Env/Msh/Mat/Tex %d %d %d %d\n"
              , s_environments->count()
              , s_meshes->count()
              , s_materials->count()
              , s_textures->count()
              );
        #endif //debug only

        s_environments->gc();
        s_meshes->gc();
        s_materials->gc();
        s_textures->gc();
    }

    void destroyPrograms()
    {
        #define PROG_DESC(_name, _vs, _fs) BGFX_SAFE_DESTROY_PROGRAM(s_programs[Program::_name]);
        #include "context_res.h"
    }

    void destroyUniforms()
    {
        s_uniforms.destroy();
    }

    void destroyTextures()
    {
        s_textures->destroyAll();
    }

    void destroyMeshes()
    {
        s_meshes->destroyAll();
    }

    void destroyEnvironments()
    {
        s_environments->destroyAll();
    }

} //namespace cs

/* vim: set sw=4 ts=4 expandtab: */
