/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_SETTINGS_H_HEADER_GUARD
#define CMFTSTUDIO_SETTINGS_H_HEADER_GUARD

#include <stdint.h>
#include <cmft/cubemapfilter.h> //cmft::LightingModel
#include <bx/readerwriter.h>    //bx::ReaderSeekerI, bx::WriterI
#include <dm/misc.h>            //dm::signf

struct ToneMapping
{
    enum Enum
    {
        Linear,
        Gamma,
        Reinhard,
        Reinhard2,
        Filmic,
        Uncharted2,

        Count,
    };
};

struct SpheresSceneState
{
    SpheresSceneState()
    {
        m_vertCount        = 7.0f;
        m_horizCount       = 7.0f;
        m_scale            = 1.0f;
        m_incl             = 0.0f;
        m_pos[0]           = 0.0f;
        m_pos[1]           = 0.0f;
        m_pos[2]           = 0.0f;
        m_rot[0]           = 0.0f;
        m_rot[1]           = 0.0f;
        m_rot[2]           = 0.0f;
        m_color0[0]        = 0.8f;
        m_color0[1]        = 0.8f;
        m_color0[2]        = 0.8f;
        m_color1[0]        = 0.03f;
        m_color1[1]        = 0.03f;
        m_color1[2]        = 0.03f;
        m_matSelection     = 0;
        m_leftToRightColor = true;
        m_horizMetalness   = false;
        m_horizIncr        = true;
        m_vertIncr         = false;
        m_followCamera     = true;
    }

    float m_horizCount;
    float m_vertCount;
    float m_scale;
    float m_incl;
    float m_pos[3];
    float m_rot[3];
    float m_color0[3];
    float m_color1[3];
    uint8_t m_matSelection;
    bool m_leftToRightColor;
    bool m_horizMetalness;
    bool m_horizIncr;
    bool m_vertIncr;
    bool m_followCamera;
};

static inline void write(bx::WriterI* _writer, const SpheresSceneState& _spheresScene)
{
    bx::write(_writer, _spheresScene.m_horizCount);
    bx::write(_writer, _spheresScene.m_vertCount);
    bx::write(_writer, _spheresScene.m_scale);
    bx::write(_writer, _spheresScene.m_incl);
    bx::write(_writer, _spheresScene.m_pos, 3*sizeof(float));
    bx::write(_writer, _spheresScene.m_rot, 3*sizeof(float));
    bx::write(_writer, _spheresScene.m_color0, 3*sizeof(float));
    bx::write(_writer, _spheresScene.m_color1, 3*sizeof(float));
    bx::write(_writer, _spheresScene.m_matSelection);
    bx::write(_writer, _spheresScene.m_leftToRightColor);
    bx::write(_writer, _spheresScene.m_horizMetalness);
    bx::write(_writer, _spheresScene.m_horizIncr);
    bx::write(_writer, _spheresScene.m_vertIncr);
    bx::write(_writer, _spheresScene.m_followCamera);
}

static inline void read(bx::ReaderI* _reader, SpheresSceneState& _spheresScene)
{
    bx::read(_reader, _spheresScene.m_horizCount);
    bx::read(_reader, _spheresScene.m_vertCount);
    bx::read(_reader, _spheresScene.m_scale);
    bx::read(_reader, _spheresScene.m_incl);
    bx::read(_reader, _spheresScene.m_pos, 3*sizeof(float));
    bx::read(_reader, _spheresScene.m_rot, 3*sizeof(float));
    bx::read(_reader, _spheresScene.m_color0, 3*sizeof(float));
    bx::read(_reader, _spheresScene.m_color1, 3*sizeof(float));
    bx::read(_reader, _spheresScene.m_matSelection);
    bx::read(_reader, _spheresScene.m_leftToRightColor);
    bx::read(_reader, _spheresScene.m_horizMetalness);
    bx::read(_reader, _spheresScene.m_horizIncr);
    bx::read(_reader, _spheresScene.m_vertIncr);
    bx::read(_reader, _spheresScene.m_followCamera);
}

struct SavedSettings
{
    SavedSettings()
    {
        // Selection settings.
        m_selectedMeshIdx       = 0;
        m_selectedEnvMap        = 0;
        m_backgroundType        = 0;
        m_selectedLightingModel = cmft::LightingModel::BlinnBrdf;
        m_selectedToneMapping   = ToneMapping::Uncharted2;
        m_worldRotDest[0]       = 0.0f;
        m_worldRotDest[1]       = 0.0f;
        m_worldRotDest[2]       = 0.0f;
        m_fovDest               = 90.0f;
        m_backgroundMipLevel    = 1.0f;

        // Lighting.
        m_diffuseIbl           = true;
        m_specularIbl          = true;
        m_ambientLightStrenght = 1.0f;

        // Post process.
        resetPostProcess();

        // Hdr.
        resetHdrSettings();
    }

    void resetPostProcess()
    {
        m_brightness = 0.0f;
        m_contrast   = 1.0f;
        m_exposure   = 0.0f;
        m_gamma      = 1.0f;
        m_saturation = 1.0f;
        m_vignette   = 3.0f;
    }

    void resetHdrSettings()
    {
        m_doBloom      = true;
        m_doLightAdapt = true;
        m_middleGray   = 0.18f;
        m_white        = 1.1f;
        m_treshold     = 0.9f;
        m_gaussStdDev  = 0.8f;
        m_blurCoeff    = 0.8f;
    }

    // Selection settings.
    uint16_t m_selectedMeshIdx;
    uint16_t m_selectedEnvMap;
    uint16_t m_backgroundType;
    uint16_t m_selectedLightingModel;
    uint16_t m_selectedToneMapping;
    float m_worldRotDest[3];
    float m_fovDest;
    float m_backgroundMipLevel;

    // Lighting.
    bool m_diffuseIbl;
    bool m_specularIbl;
    float m_ambientLightStrenght;

    // Post processing.
    float m_brightness;
    float m_contrast;
    float m_saturation;
    float m_exposure;
    float m_gamma;
    float m_vignette;

    // Hdr.
    bool m_doBloom;
    bool m_doLightAdapt;
    float m_middleGray;
    float m_white;
    float m_treshold;
    float m_gaussStdDev;
    float m_blurCoeff;

    // Spheres scene.
    SpheresSceneState m_spheres;
};

static inline void write(bx::WriterI* _writer, const SavedSettings& _settings)
{
    // Selection settings.
    bx::write(_writer, _settings.m_selectedMeshIdx);
    bx::write(_writer, _settings.m_selectedEnvMap);
    bx::write(_writer, _settings.m_backgroundType);
    bx::write(_writer, _settings.m_selectedLightingModel);
    bx::write(_writer, _settings.m_selectedToneMapping);
    bx::write(_writer, _settings.m_worldRotDest[0]);
    bx::write(_writer, _settings.m_worldRotDest[1]);
    bx::write(_writer, _settings.m_worldRotDest[2]);
    bx::write(_writer, _settings.m_fovDest);
    bx::write(_writer, _settings.m_backgroundMipLevel);

    // Lighting.
    bx::write(_writer, _settings.m_diffuseIbl);
    bx::write(_writer, _settings.m_specularIbl);
    bx::write(_writer, _settings.m_ambientLightStrenght);

    // Post processing.
    bx::write(_writer, _settings.m_brightness);
    bx::write(_writer, _settings.m_contrast);
    bx::write(_writer, _settings.m_saturation);
    bx::write(_writer, _settings.m_exposure);
    bx::write(_writer, _settings.m_gamma);
    bx::write(_writer, _settings.m_vignette);

    // Hdr.
    bx::write(_writer, _settings.m_doBloom);
    bx::write(_writer, _settings.m_doLightAdapt);
    bx::write(_writer, _settings.m_middleGray);
    bx::write(_writer, _settings.m_white);
    bx::write(_writer, _settings.m_treshold);
    bx::write(_writer, _settings.m_gaussStdDev);
    bx::write(_writer, _settings.m_blurCoeff);

    // Spheres scene.
    ::write(_writer, _settings.m_spheres);
}

static inline void read(bx::ReaderI* _reader, SavedSettings& _settings)
{
    bx::read(_reader, _settings.m_selectedMeshIdx);
    bx::read(_reader, _settings.m_selectedEnvMap);
    bx::read(_reader, _settings.m_backgroundType);
    bx::read(_reader, _settings.m_selectedLightingModel);
    bx::read(_reader, _settings.m_selectedToneMapping);
    bx::read(_reader, _settings.m_worldRotDest[0]);
    bx::read(_reader, _settings.m_worldRotDest[1]);
    bx::read(_reader, _settings.m_worldRotDest[2]);
    bx::read(_reader, _settings.m_fovDest);
    bx::read(_reader, _settings.m_backgroundMipLevel);

    // Lighting.
    bx::read(_reader, _settings.m_diffuseIbl);
    bx::read(_reader, _settings.m_specularIbl);
    bx::read(_reader, _settings.m_ambientLightStrenght);

    // Post processing.
    bx::read(_reader, _settings.m_brightness);
    bx::read(_reader, _settings.m_contrast);
    bx::read(_reader, _settings.m_saturation);
    bx::read(_reader, _settings.m_exposure);
    bx::read(_reader, _settings.m_gamma);
    bx::read(_reader, _settings.m_vignette);

    // Hdr.
    bx::read(_reader, _settings.m_doBloom);
    bx::read(_reader, _settings.m_doLightAdapt);
    bx::read(_reader, _settings.m_middleGray);
    bx::read(_reader, _settings.m_white);
    bx::read(_reader, _settings.m_treshold);
    bx::read(_reader, _settings.m_gaussStdDev);
    bx::read(_reader, _settings.m_blurCoeff);

    // Spheres scene.
    ::read(_reader, _settings.m_spheres);
}

struct LocalSettings
{
    LocalSettings()
    {
        // Camera.
        m_fov         = 90.0f;
        m_worldRot[0] = 0.0f;
        m_worldRot[1] = 0.0f;
        m_worldRot[2] = 0.0f;

        // Temp settings.
        m_selectedLight = UINT8_MAX;

        // AntiAlias.
        m_msaa = false;
        m_fxaa = true;
    }

    // Camera.
    float m_fov;
    float m_worldRot[3];

    // Temp settings.
    uint8_t m_selectedLight;

    // AntiAlias.
    bool m_msaa;
    bool m_fxaa;
};

struct Settings : SavedSettings, LocalSettings
{
private:
    static inline void wrapRotation(float& _rot)
    {
        _rot -= float(int32_t(_rot));
    }
public:

    void apply(const SavedSettings& _savedSettings)
    {
        memcpy((SavedSettings*)this, &_savedSettings, sizeof(SavedSettings));

        wrapRotation(m_worldRotDest[0]);
        wrapRotation(m_worldRotDest[1]);
        wrapRotation(m_worldRotDest[2]);
        wrapRotation(m_worldRot[0]);
        wrapRotation(m_worldRot[1]);
        wrapRotation(m_worldRot[2]);
    }
};

#endif // CMFTSTUDIO_SETTINGS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
