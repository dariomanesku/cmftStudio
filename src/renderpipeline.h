/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_RENDERPIPELINE_H_HEADER_GUARD
#define CMFTSTUDIO_RENDERPIPELINE_H_HEADER_GUARD

#include <stdint.h>  // uint32_t
#include "context.h" // cs::*

namespace bgfx { struct TextureHandle; }

// Utils.
//-----

// Must be called before screenSpaceQuad(), screenQuad() and drawSplashScreen().
void initVertexDecls();

// Full screen quad.
void screenSpaceQuad(float _textureWidth
                   , float _textureHeight
                   , bool  _originBottomLeft = false
                   , float _width            = 1.0f
                   , float _height           = 1.0f
                   );
void screenQuad(int32_t _x, int32_t _y, int32_t _width, int32_t _height, bool _originBottomLeft = false);
void drawSplashScreen(cs::TextureHandle _texture, uint32_t _width, uint32_t _height);

// Render pipeline.
//-----

struct RenderPipeline
{
    enum
    {
        ViewIdSkybox,
        ViewIdMesh,
        ViewIdLum0,
        ViewIdLum1,
        ViewIdLum2,
        ViewIdLum3,
        ViewIdLum4,
        ViewIdBright,
        ViewIdBloom,
        ViewIdTonemap,
        ViewIdFxaa,
        ViewIdMaterial,
        ViewIdTonemapImage,
        ViewIdWireframe,
        ViewIdUpdateLastLum,
        ViewIdSplashScreen,

        ViewIdDebug0,
        ViewIdDebug1,
        ViewIdDebug2,
        ViewIdDebug3,
        ViewIdDebug4,
        ViewIdDebug5,

        ViewIdGui,
    };
};

void renderPipelineInit(uint32_t _width, uint32_t _height, uint32_t _reset);

void renderPipelineUpdateSize(uint32_t _width, uint32_t _height, uint32_t _reset);
void renderPipelineSetupFrame(bool _doLightAdapt, bool _doBloom, bool _doFxaa);
void renderPipelineSetActiveCamera(float* _camView, float* _camProj);
void renderPipelineSubmitSkybox(cs::EnvHandle _env, cs::Environment::Enum _which = cs::Environment::Skybox, float _lod = 0.0f);
void renderPipelineSubmitSkybox(cs::EnvHandle _nextEnv
                              , cs::EnvHandle _currEnv
                              , float _progress                   = 0.0f
                              , cs::Environment::Enum _nextWhich  = cs::Environment::Skybox
                              , cs::Environment::Enum _currWhich  = cs::Environment::Skybox
                              , float _nextLod                    = 0.0f
                              , float _currLod                    = 0.0f
                              );
void renderPipelineSubmitFrame(float _bloomStdDev = 0.8f);
void renderPipelineFlush();
void renderPipelineCleanup();

void                updateWireframePreview(cs::MeshHandle _mesh, uint16_t _groupIdx = 0);
void                updateMaterialPreview(cs::MaterialHandle _material, cs::EnvHandle _env, const float _eye[3]);
void                updateTonemapImage(cs::TextureHandle _image, float _gamma, float _minLum, float _lumRange);
bgfx::TextureHandle getWireframeTexture();
bgfx::TextureHandle getMaterialTexture();
bgfx::TextureHandle getTonemapTexture();

#endif // CMFTSTUDIO_RENDERPIPELINE_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
