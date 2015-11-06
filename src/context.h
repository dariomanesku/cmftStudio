/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_CONTEXT_H_HEADER_GUARD
#define CMFTSTUDIO_CONTEXT_H_HEADER_GUARD

#include <stdint.h>                 // uint32_t
#include <string.h>                 // memset, memcpy

#include "common/cmft.h"            // cmft::Image
#include "common/datastructures.h"

#include <bgfx/bgfx.h>              // bgfx::TextureHandle
#include <dm/readerwriter.h>        // bx::WriterI, bx::ReaderSeekerI

namespace cs
{
    CS_HANDLE(TextureHandle);
    CS_HANDLE(EnvHandle);
    CS_HANDLE(MaterialHandle);
    CS_HANDLE(MeshHandle);

    // Programs.
    //-----

    struct Program
    {
        enum Enum
        {
            #define PROG_DESC(_name, _vs, _fs) _name,
            #include "context_res.h"

            Count,
        };
    };

    bgfx::ProgramHandle getProgram(Program::Enum _prog);

    // Material.
    //-----

    struct Material
    {
        enum Texture
        {
            Albedo,
            Normal,
            Surface,
            Reflectivity,
            Occlusion,
            Emissive,

            TextureCount
        };

        bool                has(Material::Texture _tex) const;
        void                set(Material::Texture _tex, cs::TextureHandle _handle);
        cs::TextureHandle   get(Material::Texture _tex) const;
        bgfx::TextureHandle getBgfxHandle(Material::Texture _tex) const;

        enum
        {
            Size = 10*4,
            DataSize = Size*sizeof(float),
        };

        union
        {
            float m_data[Size];

            struct
            {
               /*0 - 45*/ struct /*Albedo*/       { float r,g,b,sample; } m_albedo;
               /*1 - 46*/ struct /*Reflectivity*/ { float r,g,b,sample; } m_specular;
               /*2 - 47*/ struct /*Emissive*/     { float r,g,b,sample; } m_emissive;
               /*3 - 48*/ struct
                          {
                              struct /*Surface*/ { float g,  sample; } m_surface;
                              struct /*Normal*/  { float mul,sample; } m_normal;
                          };
               /*4 - 49*/ struct { float m_reflectivity,    m_metalOrSpec,  m_fresnel,           m_specAttn; };
               /*5 - 50*/ struct { float m_invGloss,        m_invMetalness, m_texMultiplier,     m_unused00; };
               /*6 - 51*/ struct { float m_occlusionSample, m_aoBias,       m_emissiveIntensity, m_unused10; };
               /*7 - 52*/ struct { float m_swizSurface[4];      };
               /*8 - 53*/ struct { float m_swizReflectivity[4]; };
               /*9 - 54*/ struct { float m_swizOcclusion[4];    };
            };
        };

    protected:
        cs::TextureHandle m_tex[TextureCount];
        float* m_uniformSample[6];
    };

    MaterialHandle materialDefault();
    MaterialHandle materialCreate();
    MaterialHandle materialCreatePlain();
    MaterialHandle materialCreateShiny();
    MaterialHandle materialCreateStripes();
    MaterialHandle materialCreateBricks();
    MaterialHandle materialCreate(const float* _data
                                , cs::TextureHandle _albedo           = cs::TextureHandle::invalid()
                                , cs::TextureHandle _normal           = cs::TextureHandle::invalid()
                                , cs::TextureHandle _surface          = cs::TextureHandle::invalid()
                                , cs::TextureHandle _reflectivity     = cs::TextureHandle::invalid()
                                , cs::TextureHandle _ambientOcclusion = cs::TextureHandle::invalid()
                                , cs::TextureHandle _emmisive         = cs::TextureHandle::invalid()
                                );
    MaterialHandle materialCreateFrom(MaterialHandle _handle);


    // Uniforms.
    //-----

    struct DirectionalLight
    {
        union
        {
            struct
            {
                float m_color[3];
                float m_strenght;
            };

            float m_colorStrenght[4];
        };

        union
        {
            struct
            {
                float m_dir[3];
                float m_enabled;
            };

            float m_dirEnabled[4];
        };
    };

    struct Uniforms
    {
        enum
        {
            Size = 46*4 + Material::Size,
            Num = Size/4,
        };
        union
        {
            float m_data[Size];

            struct
            {
                /*0-3*/   struct { float m_mtx[16]; };
                /*4-19*/  struct { float m_offsets[16][4]; };
                /*20-21*/ struct { float m_weights[8]; };
                /*22*/    struct { float m_skyboxTransition, m_enabled, m_lod, m_lodPrev; };
                /*23*/    struct { float m_tonemapGamma, m_tonemapMinLum, m_tonemapLumRange, m_texelHalf; };
                /*24*/    struct { float m_camPos[3], m_time; };
                /*25*/    struct { float m_rgba[4]; };
                /*26*/    struct { float m_doLightAdapt, m_envTransition, m_edgeFixup, m_prevEdgeFixup; };
                /*27*/    struct { float m_brightness, m_contrast, m_saturation, m_unused270; };
                /*28*/    struct { float m_backgroundType, m_fov, m_blurCoeff, m_toneMapping; };
                /*29*/    struct { float m_mipCount, m_prevMipCount, m_mipSize, m_prevMipSize; };
                /*30*/    struct { float m_exposure, m_gamma, m_vignette, m_unused300; };
                /*31*/    struct { float m_middleGray, m_whiteSqr, m_treshold, m_doBloom; };
                /*32*/    struct { float m_diffuseIbl, m_specularIbl, m_ambientLightStrenght, m_lightingModel; };
                /*33*/    struct { float m_matCam[3], m_selectedLight; };
                /*34-43*/ struct { DirectionalLight m_directionalLights[CS_MAX_LIGHTS]; };
                /*46-54*/ struct { float m_material[Material::Size]; };
            };
        };
    };

    struct TextureUniform
    {
        enum Enum
        {
            #define TEXUNI_DESC(_enum, _stage, _name) _enum,
            #include "context_res.h"

            Count,
        };
    };

    Uniforms& getUniforms();
    void      submitUniforms();

    static inline uint32_t bgfx_submit(uint8_t _viewId, Program::Enum _prog, int32_t _depth = 0)
    {
        submitUniforms();
        return bgfx::submit(_viewId, getProgram(_prog), _depth);
    }


    // Texture.
    //-----

    struct Texture
    {
        bgfx::TextureHandle m_bgfxHandle;
        uint32_t m_size;
        void*    m_data;
        uint8_t  m_numMips;
    };

    cs::TextureHandle   textureStripes();
    cs::TextureHandle   textureBricksN();
    cs::TextureHandle   textureBricksAo();
    cs::TextureHandle   textureLoad(const char* _path);
    cs::TextureHandle   textureLoad(const void* _data, uint32_t _size);
    cs::TextureHandle   textureLoadRaw(const void* _data, uint32_t _size);
    bgfx::TextureHandle textureGetBgfxHandle(cs::TextureHandle _handle);


    // Mesh.
    //-----

    struct Mesh
    {
        float m_normScale;
    };

    MeshHandle meshSphere();
    uint32_t   meshNumGroups(MeshHandle _mesh);
    MeshHandle meshLoad(const void* _data, uint32_t _size, const char* _ext);
    MeshHandle meshLoad(const char* _filePath, void* _userData = NULL, dm::StackAllocatorI* _stack = dm::stackAlloc);
    MeshHandle meshLoad(dm::ReaderSeekerI& _reader, dm::StackAllocatorI* _stack = dm::stackAlloc);
    bool       meshSave(MeshHandle _mesh, const char* _filePath);


    // MeshInstance.
    //-----

    struct MeshInstance
    {
        MeshInstance();
        MeshInstance(const MeshInstance& _other);
        ~MeshInstance();

        void set(cs::MeshHandle _mesh);
        void set(cs::MaterialHandle _material, uint32_t _groupIdx = 0);
        cs::MaterialHandle getActiveMaterial() const;
        float* computeMtx();

        float m_scale;
        float m_scaleAdj;
        float m_rot[3];
        float m_pos[3];
        float m_mtx[16];
        cs::MeshHandle m_mesh;
        uint16_t m_selGroup;
        dm::Array<MaterialHandle> m_materials;
    };

    MeshInstance* acquire(const MeshInstance* _inst);
    MeshInstance& acquire(const MeshInstance& _inst);
    void          release(const MeshInstance* _inst);


    // Environment.
    //-----

    struct Environment
    {
        enum Enum
        {
            Skybox,
            Pmrem,  // Prefiltered Mipmapped Radiance Environment Map (PMREM).
            Iem,    // Irradiance Environment Map (IEM).

            Count
        };

        cmft::Image m_cubemapImage[Count];
        cmft::Image m_origSkyboxImage;
        cs::TextureHandle m_cubemap[Count];
        cs::TextureHandle m_latlong[Count];
        cs::TextureHandle m_origSkybox;
        DirectionalLight m_lights[CS_MAX_LIGHTS];
        cmft::EdgeFixup::Enum m_edgeFixup;
        uint8_t m_lightsNum;
        bool m_lightUseBackgroundColor[CS_MAX_LIGHTS];
    };

    EnvHandle    envCreateCmftStudioLogo();
    EnvHandle    envCreate(uint32_t _rgba = 0x303030ff);
    EnvHandle    envCreate(const char* _skyboxPath, const char* _pmremPath, const char* _iemPath);
    EnvHandle    envCreate(dm::ReaderSeekerI& _reader);
    void         envLoad(EnvHandle _handle, Environment::Enum _which, cmft::Image& _image); // Notice: this takes ownership of '_image'.
    bool         envLoad(EnvHandle _handle, Environment::Enum _which, const char* _filePath);
    void         envTransform_UseMacroInstead(EnvHandle _handle, Environment::Enum _which, ...);
    #define      envTransform(_handle, _which, ...) envTransform_UseMacroInstead(_handle, _which, __VA_ARGS__, UINT32_MAX)
    void         envResize(EnvHandle _handle, Environment::Enum _which, uint32_t _faceSize);
    void         envConvert(EnvHandle _handle, Environment::Enum _which, cmft::TextureFormat::Enum _format);
    void         envTonemap(EnvHandle _handle, float _gamma, float _minLum, float _lumRange);
    void         envRestoreSkybox(EnvHandle _handle);
    cmft::Image& envGetImage(EnvHandle _handle, Environment::Enum _which);


    // Resource resolver.
    //-----

    void resourceMap(uint16_t _id, TextureHandle _handle);
    void resourceMap(uint16_t _id, MaterialHandle _handle);
    void resourceMap(uint16_t _id, MeshHandle _handle);
    void resourceMap(uint16_t _id, EnvHandle _handle);
    void resourceResolve(TextureHandle* _handle, uint16_t _id);
    void resourceResolve(MaterialHandle* _handle, uint16_t _id);
    void resourceResolve(MeshHandle* _handle, uint16_t _id);
    void resourceResolve(EnvHandle* _handle, uint16_t _id);
    void resourceResolveAll();
    void resourceClearMappings();


    // Lists.
    //-----

    typedef HandleArray<TextureHandle>  TextureList;
    typedef HandleArray<MaterialHandle> MaterialList;
    typedef HandleArray<MeshHandle>     MeshList;
    typedef HandleArray<EnvHandle>      EnvList;
    typedef dm::List<MeshInstance>      MeshInstanceList;

    void listRemoveRelease(TextureList& _list, uint16_t _idx);
    void listRemoveRelease(MaterialList& _list, uint16_t _idx);
    void listRemoveRelease(MeshList& _list, uint16_t _idx);
    void listRemoveRelease(EnvList& _list, uint16_t _idx);
    void listRemoveRelease(MeshInstanceList& _list, uint16_t _idx);

    void listRemoveReleaseAll(TextureList& _list);
    void listRemoveReleaseAll(MaterialList& _list);
    void listRemoveReleaseAll(MeshList& _list);
    void listRemoveReleaseAll(EnvList& _list);
    void listRemoveReleaseAll(MeshInstanceList& _list);


    // Context.
    //-----

    void initContext();
    void initPrograms();
    void initUniforms();

    void freeHostMem(TextureHandle _handle);
    void freeHostMem(MeshHandle _handle);
    void freeHostMem(EnvHandle _handle);

    void setTexture(TextureUniform::Enum _which, cs::TextureHandle _handle, uint32_t _flags = UINT32_MAX);
    void setTexture(TextureUniform::Enum _which, bgfx::TextureHandle _handle, uint32_t _flags = UINT32_MAX);
    void setTexture(TextureUniform::Enum _which, bgfx::FrameBufferHandle _handle, uint32_t _flags = UINT32_MAX, uint8_t _attachment = 0);
    void setMaterial(MaterialHandle _handle);
    void setEnv(EnvHandle _handle);
    void setEnvTransition(EnvHandle _from);

    #define CS_DEFAULT_DRAW_STATE 0                             \
                                  | BGFX_STATE_RGB_WRITE        \
                                  | BGFX_STATE_ALPHA_WRITE      \
                                  | BGFX_STATE_DEPTH_WRITE      \
                                  | BGFX_STATE_DEPTH_TEST_LESS  \
                                  | BGFX_STATE_CULL_CCW         \
                                  | BGFX_STATE_MSAA

    void submit(uint8_t _view
              , MeshInstance& _instance
              , Program::Enum _prog
              , EnvHandle _env = EnvHandle::invalid()
              , uint64_t _state = CS_DEFAULT_DRAW_STATE
              );
    void submit(uint8_t _view
              , MeshHandle _mesh
              , Program::Enum _prog
              , const float* _mtx
              , const MaterialHandle* _materials
              , EnvHandle _env = EnvHandle::invalid()
              , uint64_t _state = CS_DEFAULT_DRAW_STATE
              );
    void submit(uint8_t _view
              , MeshHandle _mesh
              , Program::Enum _prog
              , const float* _mtx
              , const MaterialHandle* _materials
              , EnvHandle _nextEnv
              , EnvHandle _currEnv
              , float _progress
              , uint64_t _state = CS_DEFAULT_DRAW_STATE
              );
    void submit(uint8_t _view
              , MeshHandle _mesh
              , Program::Enum _prog
              , const float* _mtx
              , uint32_t _groupIdx
              , MaterialHandle _material
              , EnvHandle _env = EnvHandle::invalid()
              , uint64_t _state = CS_DEFAULT_DRAW_STATE
              );

    Texture&     getObj(TextureHandle _handle);
    Material&    getObj(MaterialHandle _handle);
    Mesh&        getObj(MeshHandle _handle);
    Environment& getObj(EnvHandle _handle);

    void setName(TextureHandle _handle, const char* _name);
    void setName(MaterialHandle _handle, const char* _name);
    void setName(MeshHandle _handle, const char* _name);
    void setName(EnvHandle _handle, const char* _name);

    char* getName(TextureHandle _handle);
    char* getName(MaterialHandle _handle);
    char* getName(MeshHandle _handle);
    char* getName(EnvHandle _handle);

    TextureHandle  acquire(TextureHandle _handle);
    MaterialHandle acquire(MaterialHandle _handle);
    MeshHandle     acquire(MeshHandle _handle);
    EnvHandle      acquire(EnvHandle _handle);

    void release(TextureHandle _handle);
    void release(MaterialHandle _handle);
    void release(MeshHandle _handle);
    void release(EnvHandle _handle);

    TextureHandle  readTexture(dm::ReaderSeekerI* _reader,dm::StackAllocatorI* _stack = dm::stackAlloc);
    MaterialHandle readMaterial(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack = dm::stackAlloc);
    MeshHandle     readMesh(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack = dm::stackAlloc);
    EnvHandle      readEnv(dm::ReaderSeekerI* _reader, dm::StackAllocatorI* _stack = dm::stackAlloc);
    void           readMeshInstance(dm::ReaderSeekerI* _reader, MeshInstance* _instance);

    /// Notice: after read*(), createGpuBuffers*() need to be called from the main thread.
    void createGpuBuffers(TextureHandle _handle, uint32_t _flags = BGFX_TEXTURE_NONE);
    void createGpuBuffers(MeshHandle _handle);
    void createGpuBuffers(EnvHandle _handle);

    void write(bx::WriterI* _writer, TextureHandle _handle);
    void write(bx::WriterI* _writer, MaterialHandle _handle);
    void write(bx::WriterI* _writer, MeshHandle _handle);
    void write(bx::WriterI* _writer, EnvHandle _handle);
    void write(bx::WriterI* _writer, const MeshInstance& _inst);

    void resourceGCFor(double _ms);
    void resourceGC(uint16_t _maxObj);
    void resourceGC();

    void destroyPrograms();
    void destroyUniforms();
    void destroyTextures();
    void destroyMeshes();
    void destroyEnvironments();
    void destroyContext();

} //namespace cs

#endif // CMFTSTUDIO_CONTEXT_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
