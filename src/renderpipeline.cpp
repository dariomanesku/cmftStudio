/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "renderpipeline.h"

#include "common/utils.h"
#include <bx/fpumath.h>   // bx::mtxProj
#include <dm/pi.h>

#define CS_DEBUG_LUMAVG 0

#ifndef CS_DEBUG_LUMAVG
    #define CS_DEBUG_LUMAVG 0
#endif //CS_DEBUG_LUMAVG

struct PosColorTexCoord0Vertex
{
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_rgba;
    float m_u;
    float m_v;

    static void init()
    {
        ms_decl.begin()
               .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
               .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
               .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
               .end();
    }

    static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosColorTexCoord0Vertex::ms_decl;

void initVertexDecls()
{
    PosColorTexCoord0Vertex::init();
}

template <int32_t N>
static inline void calcOffsetsNxN(float _offsets[][4], uint16_t _dstWidth, uint16_t _dstHeight)
{
    const float invDstWidthf  = 1.0f/dm::utof(_dstWidth);
    const float invDstHeightf = 1.0f/dm::utof(_dstHeight);

    const float du = 1.0f/dm::utof(N*_dstWidth);
    const float dv = 1.0f/dm::utof(N*_dstHeight);

    const float shiftU = (invDstWidthf  - du)*0.5f;
    const float shiftV = (invDstHeightf - dv)*0.5f;

    const float nn = float(N);

    uint16_t index = 0;
    for (float yy = 0.0f, end = nn; yy < end; yy+=1.0f)
    {
        for (float xx = 0.0f, end = nn; xx < end; xx+=1.0f)
        {
            _offsets[index][0] = xx * du - shiftU;
            _offsets[index][1] = yy * dv - shiftV;
            index++;
        }
    }
}

static inline void calcOffsets4x4(float _offsets[][4], uint16_t _dstWidth, uint16_t _dstHeight)
{
    calcOffsetsNxN<4>(_offsets, _dstWidth, _dstHeight);
}

void screenSpaceQuad(float _textureWidth, float _textureHeight, bool _originBottomLeft, float _width, float _height)
{
    if (bgfx::checkAvailTransientVertexBuffer(3, PosColorTexCoord0Vertex::ms_decl))
    {
        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, PosColorTexCoord0Vertex::ms_decl);
        PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)vb.data;

        const float zz = 0.0f;

        const float minx = -_width;
        const float maxx =  _width;
        const float miny = 0.0f;
        const float maxy = _height*2.0f;

        const float texelHalfW = g_texelHalf/_textureWidth;
        const float texelHalfH = g_texelHalf/_textureHeight;
        const float minu = -1.0f + texelHalfW;
        const float maxu =  1.0f + texelHalfW;

        float minv = texelHalfH;
        float maxv = 2.0f + texelHalfH;

        if (_originBottomLeft)
        {
            float temp = minv;
            minv = maxv;
            maxv = temp;

            minv -= 1.0f;
            maxv -= 1.0f;
        }

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = zz;
        vertex[0].m_rgba = 0xffffffff;
        vertex[0].m_u = minu;
        vertex[0].m_v = minv;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = zz;
        vertex[1].m_rgba = 0xffffffff;
        vertex[1].m_u = maxu;
        vertex[1].m_v = minv;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = zz;
        vertex[2].m_rgba = 0xffffffff;
        vertex[2].m_u = maxu;
        vertex[2].m_v = maxv;

        bgfx::setVertexBuffer(&vb);
    }
}

void screenQuad(int32_t _x, int32_t _y, int32_t _width, int32_t _height, bool _originBottomLeft)
{
    if (bgfx::checkAvailTransientVertexBuffer(6, PosColorTexCoord0Vertex::ms_decl) )
    {
        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 6, PosColorTexCoord0Vertex::ms_decl);
        PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)vb.data;

        const float widthf  = float(_width);
        const float heightf = float(_height);

        const float minx = float(_x);
        const float miny = float(_y);
        const float maxx = minx+widthf;
        const float maxy = miny+heightf;
        const float zz = 0.0f;

        const float texelHalfW = g_texelHalf/widthf;
        const float texelHalfH = g_texelHalf/heightf;
        const float minu = texelHalfW;
        const float maxu = 1.0f - texelHalfW;
        const float minv = _originBottomLeft ? texelHalfH+1.0f : texelHalfH     ;
        const float maxv = _originBottomLeft ? texelHalfH      : texelHalfH+1.0f;

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = zz;
        vertex[0].m_rgba = 0xffffffff;
        vertex[0].m_u = minu;
        vertex[0].m_v = minv;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = zz;
        vertex[1].m_rgba = 0xffffffff;
        vertex[1].m_u = maxu;
        vertex[1].m_v = minv;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = zz;
        vertex[2].m_rgba = 0xffffffff;
        vertex[2].m_u = maxu;
        vertex[2].m_v = maxv;

        vertex[3].m_x = maxx;
        vertex[3].m_y = maxy;
        vertex[3].m_z = zz;
        vertex[3].m_rgba = 0xffffffff;
        vertex[3].m_u = maxu;
        vertex[3].m_v = maxv;

        vertex[4].m_x = minx;
        vertex[4].m_y = maxy;
        vertex[4].m_z = zz;
        vertex[4].m_rgba = 0xffffffff;
        vertex[4].m_u = minu;
        vertex[4].m_v = maxv;

        vertex[5].m_x = minx;
        vertex[5].m_y = miny;
        vertex[5].m_z = zz;
        vertex[5].m_rgba = 0xffffffff;
        vertex[5].m_u = minu;
        vertex[5].m_v = minv;

        bgfx::setVertexBuffer(&vb);
    }
}

void drawSplashScreen(cs::TextureHandle _texture, uint32_t _width, uint32_t _height)
{
    float view[16];
    float proj[16];
    bx::mtxIdentity(view);
    bx::mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);
    bgfx::setViewTransform(RenderPipeline::ViewIdSplashScreen, view, proj);
    bgfx::setViewRect(RenderPipeline::ViewIdSplashScreen, 0, 0, (uint16_t)_width, (uint16_t)_height);
    bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
    screenSpaceQuad(dm::utof(_width), dm::utof(_height));
    cs::setTexture(cs::TextureUniform::Color, _texture);
    cs::bgfx_submit(RenderPipeline::ViewIdSplashScreen, cs::Program::Image);
    g_frameNum = bgfx::frame();
}

struct RenderPipelineImpl : public RenderPipeline
{
    void init(uint32_t _width, uint32_t _height, uint32_t _reset)
    {
        m_width  = _width;
        m_height = _height;
        m_reset  = _reset;

        enum
        {
            FlagsFb      = BGFX_TEXTURE_RT|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP,
            FlagsFbDepth = BGFX_TEXTURE_RT|BGFX_TEXTURE_RT_BUFFER_ONLY,
        };

        m_fbMaterialTextures[0] = bgfx::createTexture2D(MaterialFbSize, MaterialFbSize, 1, bgfx::TextureFormat::BGRA8, FlagsFb);
        m_fbMaterialTextures[1] = bgfx::createTexture2D(MaterialFbSize, MaterialFbSize, 1, bgfx::TextureFormat::D16,   FlagsFbDepth);
        m_fbMaterial = bgfx::createFrameBuffer(2, m_fbMaterialTextures, true);

        m_fbTonemapImageTextures[0] = bgfx::createTexture2D(TonemapImgWidth, TonemapImgHeight, 1, bgfx::TextureFormat::BGRA8, FlagsFb);
        m_fbTonemapImage = bgfx::createFrameBuffer(1, m_fbTonemapImageTextures, true);

        m_fbWireframeTextures[0] = bgfx::createTexture2D(WireframeFbSize, WireframeFbSize, 1, bgfx::TextureFormat::BGRA8, FlagsFb);
        m_fbWireframeTextures[1] = bgfx::createTexture2D(WireframeFbSize, WireframeFbSize, 1, bgfx::TextureFormat::D16,   FlagsFbDepth);
        m_fbWireframe = bgfx::createFrameBuffer(2, m_fbWireframeTextures, true);

        m_fbLumCalc[0] = bgfx::createFrameBuffer(256, 256, bgfx::TextureFormat::BGRA8);
        m_fbLumCalc[1] = bgfx::createFrameBuffer( 64,  64, bgfx::TextureFormat::BGRA8);
        m_fbLumCalc[2] = bgfx::createFrameBuffer( 16,  16, bgfx::TextureFormat::BGRA8);
        m_fbLumCalc[3] = bgfx::createFrameBuffer(  4,   4, bgfx::TextureFormat::BGRA8);
        m_fbLumCur     = bgfx::createFrameBuffer(  1,   1, bgfx::TextureFormat::BGRA8);
        m_fbLumLast    = bgfx::createFrameBuffer(  1,   1, bgfx::TextureFormat::BGRA8);

        createSizeDependentFrameBuffers();

        // TODO: This is a dirty way to make sure fbLumCurr and fbLumLast are initialized to 0.
        // Find an acutual solution.
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x00000000, 0.0f, 0);
        bgfx::setViewClear(1, BGFX_CLEAR_COLOR, 0x00000000, 0.0f, 0);
        bgfx::setViewFrameBuffer(0, m_fbLumLast);
        bgfx::setViewFrameBuffer(1, m_fbLumCur);
        bgfx::submit(0, cs::getProgram(cs::Program::Color));
        bgfx::submit(1, cs::getProgram(cs::Program::Color));
        bgfx::frame();
        const bgfx::FrameBufferHandle invalidHandle = BGFX_INVALID_HANDLE;
        bgfx::setViewFrameBuffer(0, invalidHandle);
        bgfx::setViewFrameBuffer(1, invalidHandle);
        bgfx::setViewClear(0, BGFX_CLEAR_NONE, 0x00000000, 0.0f, 0);
        bgfx::setViewClear(1, BGFX_CLEAR_NONE, 0x00000000, 0.0f, 0);
    }

    void updateSize(uint32_t _width, uint32_t _height, uint32_t _reset)
    {
        // Recreate render targets on window resize.
        if (m_width  != _width
        ||  m_height != _height
        ||  m_reset  != _reset)
        {
            m_width  = _width;
            m_height = _height;
            m_reset  = _reset;

            if (bgfx::isValid(m_fbMain))   { bgfx::destroyFrameBuffer(m_fbMain);   m_fbMain.idx   = bgfx::invalidHandle; }
            if (bgfx::isValid(m_fbPost))   { bgfx::destroyFrameBuffer(m_fbPost);   m_fbPost.idx   = bgfx::invalidHandle; }
            if (bgfx::isValid(m_fbBright)) { bgfx::destroyFrameBuffer(m_fbBright); m_fbBright.idx = bgfx::invalidHandle; }
            if (bgfx::isValid(m_fbBlur))   { bgfx::destroyFrameBuffer(m_fbBlur);   m_fbBlur.idx   = bgfx::invalidHandle; }

            createSizeDependentFrameBuffers();
        }
    }

    void setupFrame(bool _doLightAdapt, bool _doBloom, bool _doFxaa)
    {
        m_doLightAdapt = _doLightAdapt;
        m_doBloom      = _doBloom;
        m_doFxaa       = _doFxaa;

        float screenProj[16];
        float windowProj[16];
        bx::mtxOrtho(screenProj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);
        bx::mtxOrtho(windowProj, 0.0f, (float)m_width, (float)m_height, 0.0f, 0.0f, 1000.0f);

        bgfx::setViewRect(ViewIdSkybox, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        bgfx::setViewRect(ViewIdMesh,   0, 0, (uint16_t)m_width, (uint16_t)m_height);

        bgfx::setViewTransform(ViewIdSkybox, NULL, screenProj);
        //bgfx::setViewTransform for ViewIdMesh is called in 'setActiveCamera()'.

        bgfx::setViewFrameBuffer(ViewIdSkybox, m_fbMain);
        bgfx::setViewFrameBuffer(ViewIdMesh,   m_fbMain);

        bgfx::setViewClear(ViewIdSkybox, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        if (m_doLightAdapt)
        {
            bgfx::setViewRect(ViewIdLum0, 0, 0, 256, 256);
            bgfx::setViewRect(ViewIdLum1, 0, 0,  64,  64);
            bgfx::setViewRect(ViewIdLum2, 0, 0,  16,  16);
            bgfx::setViewRect(ViewIdLum3, 0, 0,   4,   4);
            bgfx::setViewRect(ViewIdLum4, 0, 0,   1,   1);
            bgfx::setViewRect(ViewIdUpdateLastLum, 0, 0, 1, 1);

            bgfx::setViewTransform(ViewIdLum0, NULL, screenProj);
            bgfx::setViewTransform(ViewIdLum1, NULL, screenProj);
            bgfx::setViewTransform(ViewIdLum2, NULL, screenProj);
            bgfx::setViewTransform(ViewIdLum3, NULL, screenProj);
            bgfx::setViewTransform(ViewIdLum4, NULL, screenProj);
            bgfx::setViewTransform(ViewIdUpdateLastLum, NULL, screenProj);

            bgfx::setViewFrameBuffer(ViewIdLum0, m_fbLumCalc[0]);
            bgfx::setViewFrameBuffer(ViewIdLum1, m_fbLumCalc[1]);
            bgfx::setViewFrameBuffer(ViewIdLum2, m_fbLumCalc[2]);
            bgfx::setViewFrameBuffer(ViewIdLum3, m_fbLumCalc[3]);
            bgfx::setViewFrameBuffer(ViewIdLum4, m_fbLumCur);
            bgfx::setViewFrameBuffer(ViewIdUpdateLastLum, m_fbLumLast);
        }

        if (m_doBloom)
        {
            bgfx::setViewRect(ViewIdBright, 0, 0, uint16_t(m_brightWidth), uint16_t(m_brightHeight));
            bgfx::setViewRect(ViewIdBloom,  0, 0, uint16_t(m_blurWidth),   uint16_t(m_blurHeight));

            bgfx::setViewTransform(ViewIdBright, NULL, screenProj);
            bgfx::setViewTransform(ViewIdBloom,  NULL, screenProj);

            bgfx::setViewFrameBuffer(ViewIdBright, m_fbBright);
            bgfx::setViewFrameBuffer(ViewIdBloom,  m_fbBlur);
        }

        bgfx::setViewRect(ViewIdTonemap, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        bgfx::setViewTransform(ViewIdTonemap, NULL, screenProj);

        if (m_doFxaa)
        {
            bgfx::setViewFrameBuffer(ViewIdTonemap, m_fbPost);

            bgfx::setViewRect(ViewIdFxaa, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
            bgfx::setViewTransform(ViewIdFxaa, NULL, screenProj);
        }
        else
        {
            const bgfx::FrameBufferHandle invalidHandle = BGFX_INVALID_HANDLE;
            bgfx::setViewFrameBuffer(ViewIdTonemap, invalidHandle);
        }

#if CS_DEBUG_LUMAVG
        bgfx::setViewRect(ViewIdDebug0, 0, 0, uint16_t(m_width), uint16_t(m_height));
        bgfx::setViewRect(ViewIdDebug1,  306,     10, 512, 512);
        bgfx::setViewRect(ViewIdDebug2,  823,     10, 512, 512);
        bgfx::setViewRect(ViewIdDebug3,  306, 512+20, 512, 512);
        bgfx::setViewRect(ViewIdDebug4,  823, 512+20, 512, 512);
        bgfx::setViewRect(ViewIdDebug5, 1340, 512+20, 512, 512);
        bgfx::setViewTransform(ViewIdDebug0, NULL, screenProj);
        bgfx::setViewTransform(ViewIdDebug1, NULL, screenProj);
        bgfx::setViewTransform(ViewIdDebug2, NULL, screenProj);
        bgfx::setViewTransform(ViewIdDebug3, NULL, screenProj);
        bgfx::setViewTransform(ViewIdDebug4, NULL, screenProj);
        bgfx::setViewTransform(ViewIdDebug5, NULL, screenProj);
#endif //CS_DEBUG_LUMAVG
    }

    void setActiveCamera(float* _camView, float* _camProj)
    {
        bgfx::setViewTransform(ViewIdMesh, _camView, _camProj);
    }

    void submitSkybox(cs::EnvHandle _env, cs::Environment::Enum _which, float _lod)
    {
        enum
        {
            EnvTexFlags = BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP,
        };

        cs::Uniforms& uniforms = cs::getUniforms();
        const cs::Environment& env = cs::getObj(_env);

        // Environment uniforms.
        for (uint8_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
        {
            uniforms.m_directionalLights[ii].m_dirEnabled[0]    = env.m_lights[ii].m_dir[0];
            uniforms.m_directionalLights[ii].m_dirEnabled[1]    = env.m_lights[ii].m_dir[1];
            uniforms.m_directionalLights[ii].m_dirEnabled[2]    = env.m_lights[ii].m_dir[2];
            uniforms.m_directionalLights[ii].m_dirEnabled[3]    = env.m_lights[ii].m_enabled;
            uniforms.m_directionalLights[ii].m_colorStrenght[0] = env.m_lights[ii].m_colorStrenght[0];
            uniforms.m_directionalLights[ii].m_colorStrenght[1] = env.m_lights[ii].m_colorStrenght[1];
            uniforms.m_directionalLights[ii].m_colorStrenght[2] = env.m_lights[ii].m_colorStrenght[2];
            uniforms.m_directionalLights[ii].m_colorStrenght[3] = env.m_lights[ii].m_colorStrenght[3];
        }

        // Uniforms.
        if (_which == cs::Environment::Pmrem)
        {
            uniforms.m_lod = _lod;
            uniforms.m_edgeFixup = (cmft::EdgeFixup::Warp == env.m_edgeFixup) ? 1.0f : 0.0f;
        }
        else
        {
            uniforms.m_lod = 0.0f;
            uniforms.m_edgeFixup = 0.0f;
        }

        // Texture.
        const cs::TextureHandle envTex = env.m_cubemap[_which];
        cs::setTexture(cs::TextureUniform::Skybox, envTex, EnvTexFlags);

        // Geometry.
        screenSpaceQuad(dm::utof(m_width), dm::utof(m_height), true);

        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdSkybox, cs::Program::Sky);
    }

    void submitSkybox(cs::EnvHandle _nextEnv
                    , cs::EnvHandle _currEnv
                    , float _progress
                    , cs::Environment::Enum _nextWhich
                    , cs::Environment::Enum _currWhich
                    , float _nextLod
                    , float _currLod
                    )
    {
        enum
        {
            EnvTexFlags = BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP,
        };

        cs::Uniforms& uniforms = cs::getUniforms();
        const cs::Environment& env = cs::getObj(_nextEnv);
        const cs::Environment& envPrev = cs::getObj(_currEnv);

        // Environment uniforms.
        for (uint8_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
        {
            uniforms.m_directionalLights[ii].m_dirEnabled[0]    = env.m_lights[ii].m_dir[0];
            uniforms.m_directionalLights[ii].m_dirEnabled[1]    = env.m_lights[ii].m_dir[1];
            uniforms.m_directionalLights[ii].m_dirEnabled[2]    = env.m_lights[ii].m_dir[2];
            uniforms.m_directionalLights[ii].m_dirEnabled[3]    = env.m_lights[ii].m_enabled;
            uniforms.m_directionalLights[ii].m_colorStrenght[0] = env.m_lights[ii].m_colorStrenght[0];
            uniforms.m_directionalLights[ii].m_colorStrenght[1] = env.m_lights[ii].m_colorStrenght[1];
            uniforms.m_directionalLights[ii].m_colorStrenght[2] = env.m_lights[ii].m_colorStrenght[2];
            uniforms.m_directionalLights[ii].m_colorStrenght[3] = env.m_lights[ii].m_colorStrenght[3];
        }

        //Uniforms.
        uniforms.m_skyboxTransition = _progress;
        uniforms.m_lod = _nextLod;
        uniforms.m_lodPrev = _currLod;

        // Setup fixup.
        uniforms.m_edgeFixup = (_nextWhich == cs::Environment::Pmrem)
                             ? (cmft::EdgeFixup::Warp == env.m_edgeFixup ? 1.0f : 0.0f)
                             : 0.0f;
        uniforms.m_mipSize = dm::utof(env.m_cubemapImage[cs::Environment::Pmrem].m_width);

        uniforms.m_prevEdgeFixup = (_currWhich == cs::Environment::Pmrem)
                                 ? (cmft::EdgeFixup::Warp == envPrev.m_edgeFixup ? 1.0f : 0.0f)
                                 : 0.0f;
        uniforms.m_prevMipSize = dm::utof(envPrev.m_cubemapImage[cs::Environment::Pmrem].m_width);

        // Program and textures.
        cs::setTexture(cs::TextureUniform::Skybox, env.m_cubemap[_nextWhich],     EnvTexFlags);
        cs::setTexture(cs::TextureUniform::Iem,    envPrev.m_cubemap[_currWhich], EnvTexFlags); //use Iem sampler for sampling prev env.

        // Geometry.
        screenSpaceQuad(dm::utof(m_width), dm::utof(m_height), true);

        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdSkybox, cs::Program::SkyTrans);
    }

    void submitFrame(float _bloomStdDev = 0.8f)
    {
        enum
        {
            LumTexFlags  = BGFX_TEXTURE_MIN_POINT|BGFX_TEXTURE_MAG_POINT|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP,
            FxaaTexFlags = BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP,
        };

        cs::Uniforms& uniforms = cs::getUniforms();

        if (m_doLightAdapt)
        {
            // Calculate luminance.
            calcOffsets4x4(uniforms.m_offsets, 256, 256);
            cs::setTexture(cs::TextureUniform::Color, m_fbMainTextures[0], LumTexFlags);
            screenSpaceQuad(256.0f, 256.0f, g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP);
            cs::bgfx_submit(ViewIdLum0, cs::Program::Lum);

            // Downscale luminance 0.
            calcOffsets4x4(uniforms.m_offsets, 64, 64);
            cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[0], LumTexFlags);
            screenSpaceQuad(64.0f, 64.0f, g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdLum1, cs::Program::LumDownscale);

            // Downscale luminance 1.
            calcOffsets4x4(uniforms.m_offsets, 16, 16);
            cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[1], LumTexFlags);
            screenSpaceQuad(16.0f, 16.0f, g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdLum2, cs::Program::LumDownscale);

            // Downscale luminance 2.
            calcOffsets4x4(uniforms.m_offsets, 4, 4);
            cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[2], LumTexFlags);
            screenSpaceQuad(4.0f, 4.0f, g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdLum3, cs::Program::LumDownscale);

            // Compute average luminance tex.
            calcOffsets4x4(uniforms.m_offsets, 1, 1);
            cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[3], LumTexFlags);
            cs::setTexture(cs::TextureUniform::Lum,   m_fbLumLast,    LumTexFlags);
            screenSpaceQuad(1.0f, 1.0f, g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdLum4, cs::Program::LumAvg);

            // Update last lum.
            cs::setTexture(cs::TextureUniform::Color, m_fbLumCur);
            screenSpaceQuad(1.0f, 1.0f, g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdUpdateLastLum, cs::Program::Equals);
        }

        if (m_doBloom)
        {
            calcOffsets4x4(uniforms.m_offsets, m_brightWidth, m_brightHeight);
            cs::setTexture(cs::TextureUniform::Color, m_fbMainTextures[0]);
            cs::setTexture(cs::TextureUniform::Lum, m_fbLumCur);
            screenSpaceQuad(float(m_brightWidth), float(m_brightHeight), g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdBright, cs::Program::Bright);

            const float blur0 = gaussian(0.0f     , 0.0f, _bloomStdDev);
            const float blur1 = gaussian(1.0f/4.0f, 0.0f, _bloomStdDev);
            const float blur2 = gaussian(2.0f/4.0f, 0.0f, _bloomStdDev);
            const float blur3 = gaussian(3.0f/4.0f, 0.0f, _bloomStdDev);
            const float blur4 = gaussian(4.0f/4.0f, 0.0f, _bloomStdDev);
            const float blurn = blur0+2.0f*(blur1+blur2+blur3+blur4);
            uniforms.m_weights[0] = blur0/blurn;
            uniforms.m_weights[1] = blur1/blurn;
            uniforms.m_weights[2] = blur2/blurn;
            uniforms.m_weights[3] = blur3/blurn;
            uniforms.m_weights[4] = blur4/blurn;

            // Blur pass vertically.
            cs::setTexture(cs::TextureUniform::Color, m_fbBright);
            screenSpaceQuad(float(m_blurWidth), float(m_blurHeight), g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdBloom, cs::Program::Blur);

            // Set texture for the next (tonemapping) pass.
            cs::setTexture(cs::TextureUniform::Blur, m_fbBlur);
        }

        // Tone mapping.
        cs::setTexture(cs::TextureUniform::Color, m_fbMainTextures[0]);
        cs::setTexture(cs::TextureUniform::Lum, m_fbLumCur);
        screenSpaceQuad(float(m_width), float(m_height), g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdTonemap, cs::Program::Tonemap);

        if (m_doFxaa)
        {
            cs::setTexture(cs::TextureUniform::Color, m_fbPostTextures[0], FxaaTexFlags);
            screenSpaceQuad(float(m_width), float(m_height), g_originBottomLeft);
            bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
            cs::bgfx_submit(ViewIdFxaa, cs::Program::Fxaa);
        }

#if CS_DEBUG_LUMAVG
        screenSpaceQuad(float(int32_t(m_width)), float(int32_t(m_height)), g_originBottomLeft);
        uniforms.m_rgba[0] = 0.3f;
        uniforms.m_rgba[1] = 0.3f;
        uniforms.m_rgba[2] = 0.3f;
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdDebug0, cs::Program::Color);

        cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[0], LumTexFlags);
        screenSpaceQuad(512.0f, 512.0f, g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdDebug1, cs::Program::ImageRe8);

        cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[1], LumTexFlags);
        screenSpaceQuad(512.0f, 512.0f, g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdDebug2, cs::Program::ImageRe8);

        cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[2], LumTexFlags);
        screenSpaceQuad(512.0f, 512.0f, g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdDebug3, cs::Program::ImageRe8);

        cs::setTexture(cs::TextureUniform::Color, m_fbLumCalc[3], LumTexFlags);
        screenSpaceQuad(512.0f, 512.0f, g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdDebug4, cs::Program::ImageRe8);

        cs::setTexture(cs::TextureUniform::Color, m_fbLumCur, LumTexFlags);
        screenSpaceQuad(512.0f, 512.0f, g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(ViewIdDebug5, cs::Program::ImageRe8);
#endif //CS_DEBUG_LUMAVG
    }

    void flush()
    {
        g_frameNum = bgfx::frame();
    }

    void updateWireframePreview(cs::MeshHandle _mesh, uint16_t _groupIdx)
    {
        const float at[3]  = { 0.0f, 0.0f, 0.0f };
        const float pos[3] = { 1.0f, 0.0f, 0.0f };
        float fixedView[16];
        float fixedProj[16];
        bx::mtxLookAt(fixedView, pos, at);
        bx::mtxOrtho(fixedProj, -0.6f, 0.6f, -0.6f, 0.6f, -100.0f, 100.0f);

        bgfx::setViewRect(ViewIdWireframe, 0, 0, WireframeFbSize, WireframeFbSize);
        bgfx::setViewTransform(ViewIdWireframe, fixedView, fixedProj);
        bgfx::setViewFrameBuffer(ViewIdWireframe, m_fbWireframe);
        bgfx::setViewClear(ViewIdWireframe, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        float mtx[16];
        const cs::Mesh& mesh = cs::getObj(_mesh);
        bx::mtxScale(mtx, mesh.m_normScale, mesh.m_normScale, mesh.m_normScale);

        cs::submit(RenderPipeline::ViewIdWireframe
                 , _mesh
                 , cs::Program::Wireframe
                 , mtx
                 , _groupIdx
                 , cs::MaterialHandle::invalid()
                 , cs::EnvHandle::invalid()
                 , CS_DEFAULT_DRAW_STATE | BGFX_STATE_PT_LINES
                 );
    }

    void updateMaterialPreview(cs::MaterialHandle _material, cs::EnvHandle _env, const float _eye[3])
    {
        const float at[3] = { 0.0f, 0.0f, 0.0f };
        float view[16];
        bx::mtxLookAt(view, _eye, at);

        float proj[16];
        bx::mtxProj(proj, 60.0f, 1.0f, 0.1f, 100.0f);

        bgfx::setViewTransform(ViewIdMaterial, view, proj);
        bgfx::setViewFrameBuffer(ViewIdMaterial, m_fbMaterial);
        bgfx::setViewRect(ViewIdMaterial, 0, 0, MaterialFbSize, MaterialFbSize);
        bgfx::setViewClear(ViewIdMaterial, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x404040ff, 1.0f, 0);

        cs::Uniforms& uniforms = cs::getUniforms();
        uniforms.m_matCam[0] = _eye[0];
        uniforms.m_matCam[1] = _eye[1];
        uniforms.m_matCam[2] = _eye[2];

        float mtx[16];
        const cs::MeshHandle sphere = cs::meshSphere();
        const cs::Mesh& mesh = cs::getObj(sphere);
        bx::mtxScale(mtx, mesh.m_normScale, mesh.m_normScale, mesh.m_normScale);
        cs::submit(RenderPipeline::ViewIdMaterial, sphere, cs::Program::Material, mtx, &_material, _env);
    }

    void updateTonemapImage(cs::TextureHandle _image, float _gamma, float _minLum, float _lumRange)
    {
        float screenProj[16];
        bx::mtxOrtho(screenProj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);

        bgfx::setViewRect(ViewIdTonemapImage, 0, 0, TonemapImgWidth, TonemapImgHeight);
        bgfx::setViewTransform(ViewIdTonemapImage, NULL, screenProj);
        bgfx::setViewFrameBuffer(ViewIdTonemapImage, m_fbTonemapImage);

        cs::Uniforms& uniforms = cs::getUniforms();
        uniforms.m_tonemapGamma    = _gamma;
        uniforms.m_tonemapMinLum   = _minLum;
        uniforms.m_tonemapLumRange = _lumRange;

        cs::setTexture(cs::TextureUniform::Skybox, _image);
        screenSpaceQuad(240.0f, 120.0f, g_originBottomLeft);
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(RenderPipeline::ViewIdTonemapImage, cs::Program::CubemapTonemap);
    }

    bgfx::TextureHandle getMaterialTexture() const
    {
        return m_fbMaterialTextures[0];
    }

    bgfx::TextureHandle getWireframeTexture() const
    {
        return m_fbWireframeTextures[0];
    }

    bgfx::TextureHandle getTonemapTexture() const
    {
        return m_fbTonemapImageTextures[0];
    }

    void cleanup()
    {
        bgfx::destroyFrameBuffer(m_fbLumCalc[0]);
        bgfx::destroyFrameBuffer(m_fbLumCalc[1]);
        bgfx::destroyFrameBuffer(m_fbLumCalc[2]);
        bgfx::destroyFrameBuffer(m_fbLumCalc[3]);
        bgfx::destroyFrameBuffer(m_fbLumCur);
        bgfx::destroyFrameBuffer(m_fbLumLast);
        bgfx::destroyFrameBuffer(m_fbBright);
        bgfx::destroyFrameBuffer(m_fbBlur);
        bgfx::destroyFrameBuffer(m_fbMain);
        bgfx::destroyFrameBuffer(m_fbPost);
        bgfx::destroyFrameBuffer(m_fbMaterial);
        bgfx::destroyFrameBuffer(m_fbTonemapImage);
        bgfx::destroyFrameBuffer(m_fbWireframe);
    }

private:
    void createSizeDependentFrameBuffers()
    {
        enum
        {
            FlagsFb      = BGFX_TEXTURE_RT|BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP,
            FlagsFbDepth = BGFX_TEXTURE_RT|BGFX_TEXTURE_RT_BUFFER_ONLY,
        };

        const uint32_t msaa = (m_reset&BGFX_RESET_MSAA_MASK)>>BGFX_RESET_MSAA_SHIFT;

        const uint32_t flagsFb      = ((msaa+1)<<BGFX_TEXTURE_RT_MSAA_SHIFT)|FlagsFb;
        const uint32_t flagsFbDepth = ((msaa+1)<<BGFX_TEXTURE_RT_MSAA_SHIFT)|FlagsFbDepth;

        m_fbMainTextures[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), 1, bgfx::TextureFormat::BGRA8, flagsFb);
        m_fbMainTextures[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), 1, bgfx::TextureFormat::D16,   flagsFbDepth);
        m_fbMain = bgfx::createFrameBuffer(2, m_fbMainTextures, true);

        m_fbPostTextures[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), 1, bgfx::TextureFormat::BGRA8, flagsFb);
        m_fbPost = bgfx::createFrameBuffer(1, m_fbPostTextures, true);

        m_brightWidth  = uint16_t(m_width/3);
        m_brightHeight = uint16_t(m_height/3);
        m_fbBright = bgfx::createFrameBuffer(m_brightWidth, m_brightHeight, bgfx::TextureFormat::BGRA8);

        m_blurWidth  = uint16_t(m_width/6);
        m_blurHeight = uint16_t(m_height/6);
        m_fbBlur = bgfx::createFrameBuffer(m_blurWidth, m_blurHeight, bgfx::TextureFormat::BGRA8);
    }

    enum
    {
        TonemapImgWidth  = 240,
        TonemapImgHeight = 120,
        MaterialFbSize   = 256,
        WireframeFbSize  = 256,
    };

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_reset;

    bgfx::FrameBufferHandle m_fbMain;
    bgfx::TextureHandle     m_fbMainTextures[2];

    bgfx::FrameBufferHandle m_fbPost;
    bgfx::TextureHandle     m_fbPostTextures[1];

    bgfx::FrameBufferHandle m_fbMaterial;
    bgfx::TextureHandle     m_fbMaterialTextures[2];

    bgfx::FrameBufferHandle m_fbTonemapImage;
    bgfx::TextureHandle     m_fbTonemapImageTextures[1];

    bgfx::FrameBufferHandle m_fbWireframe;
    bgfx::TextureHandle     m_fbWireframeTextures[2];

    bgfx::FrameBufferHandle m_fbLumCalc[4];
    bgfx::FrameBufferHandle m_fbLumCur;
    bgfx::FrameBufferHandle m_fbLumLast;

    uint16_t m_brightWidth;
    uint16_t m_brightHeight;
    bgfx::FrameBufferHandle m_fbBright;

    uint16_t m_blurWidth;
    uint16_t m_blurHeight;
    bgfx::FrameBufferHandle m_fbBlur;

    bool m_doLightAdapt;
    bool m_doBloom;
    bool m_doFxaa;
};
static RenderPipelineImpl s_pipeline;

void renderPipelineInit(uint32_t _width, uint32_t _height, uint32_t _reset)
{
    s_pipeline.init(_width, _height, _reset);
}

void renderPipelineUpdateSize(uint32_t _width, uint32_t _height, uint32_t _reset)
{
    s_pipeline.updateSize(_width, _height, _reset);
}

void renderPipelineSetupFrame(bool _doLightAdapt, bool _doBloom, bool _doFxaa)
{
    s_pipeline.setupFrame(_doLightAdapt, _doBloom, _doFxaa);
}

void renderPipelineSetActiveCamera(float* _camView, float* _camProj)
{
    s_pipeline.setActiveCamera(_camView, _camProj);
}

void renderPipelineSubmitSkybox(cs::EnvHandle _env, cs::Environment::Enum _which, float _lod)
{
    s_pipeline.submitSkybox(_env, _which, _lod);
}

void renderPipelineSubmitSkybox(cs::EnvHandle _nextEnv
                              , cs::EnvHandle _currEnv
                              , float _progress
                              , cs::Environment::Enum _nextWhich
                              , cs::Environment::Enum _currWhich
                              , float _nextLod
                              , float _currLod
                              )
{
    s_pipeline.submitSkybox(_nextEnv, _currEnv, _progress, _nextWhich, _currWhich, _nextLod, _currLod);
}

void renderPipelineSubmitFrame(float _bloomStdDev)
{
    s_pipeline.submitFrame(_bloomStdDev);
}

void renderPipelineFlush()
{
    s_pipeline.flush();
}

void renderPipelineCleanup()
{
    s_pipeline.cleanup();
}

void updateWireframePreview(cs::MeshHandle _mesh, uint16_t _groupIdx)
{
    s_pipeline.updateWireframePreview(_mesh, _groupIdx);
}

void updateMaterialPreview(cs::MaterialHandle _material, cs::EnvHandle _env, const float _eye[3])
{
    s_pipeline.updateMaterialPreview(_material, _env, _eye);
}

void updateTonemapImage(cs::TextureHandle _image, float _gamma, float _minLum, float _lumRange)
{
    s_pipeline.updateTonemapImage(_image, _gamma, _minLum, _lumRange);
}

bgfx::TextureHandle getWireframeTexture()
{
    return s_pipeline.getWireframeTexture();
}

bgfx::TextureHandle getMaterialTexture()
{
    return s_pipeline.getMaterialTexture();
}

bgfx::TextureHandle getTonemapTexture()
{
    return s_pipeline.getTonemapTexture();
}

/* vim: set sw=4 ts=4 expandtab: */
