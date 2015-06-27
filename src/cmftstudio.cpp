/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common/common.h"
#include "common/config.h"
#include "common/utils.h"
#include "common/globals.h"
#include "common/timer.h"
#include "common/datastructures.h"

#include "geometry/loaders.h"

#include "backgroundjobs.h"
#include "context.h"
#include "assets.h"
#include "settings.h"
#include "guimanager.h"
#include "mouse.h"
#include "project.h"
#include "renderpipeline.h"
#include "staticres.h"
#include "eventstate.h"

#include <common.h>
#include <entry/input.h>
#include <bgfx_utils.h>

#include <bx/readerwriter.h>
#include <bx/string.h>
#include <bx/thread.h>
#include <bx/os.h>
#include <bx/commandline.h>

#include <dm/misc.h>
#include <dm/pi.h>

// Camera.
//-----

struct Camera
{
    void setup(float _xrot, float _yrot, float _fov, float _width, float _height)
    {
        m_rotX = _xrot;
        m_rotY = _yrot;

        // World rotation.
        bx::mtxRotateXY(m_rot, -_xrot*dm::pi, -_yrot*dm::twoPi);

        // View vector.
        const float look[3] = { 0.0f, 0.0f, -1.0f };
        bx::vec3MulMtx(m_viewVec, look, m_rot);

        // View matrix.
        float right[3];
        rightFromLong(right, _yrot);

        float up[3];
        bx::vec3Cross(up, right, m_viewVec);

        const float at[3] = { 0.0f, 0.0f,  0.0f };
        bx::mtxLookAt(m_view, m_viewVec, at, up);

        // Projection matrix.
        bx::mtxProj(m_proj, _fov, _width/_height, 0.05f, 11.0f);

        // ViewProj matrix.
        bx::mtxMul(m_viewProj, m_view, m_proj);

        // InvViewProj matrix.
        bx::mtxInverse(m_invViewProj, m_viewProj);

        // Fov scale.
        const float refPoint[3] = { 0.0f, -1.0f, 0.954338f };
        float refPointWorld[3];
        bx::vec3MulMtxH(refPointWorld, refPoint, m_invViewProj);
        m_fovScale = bx::vec3Length(refPointWorld);
    }

    void setUniforms() const
    {
        cs::Uniforms& uniforms = cs::getUniforms();
        memcpy(uniforms.m_mtx,    m_rot,     16*sizeof(float));
        memcpy(uniforms.m_camPos, m_viewVec,  3*sizeof(float));
    }

    float m_rotX;
    float m_rotY;
    float m_viewVec[3];
    float m_rot[16];
    float m_view[16];
    float m_proj[16];
    float m_viewProj[16];
    float m_invViewProj[16];
    float m_fovScale;
};

// Environment.
//-----

struct Environment
{
    void init(cs::EnvHandle _env, cs::Environment::Enum _which)
    {
        m_skybox.init(_env, _which);
        m_ibl.init(_env);
    }

    void update(cs::EnvHandle _env, cs::Environment::Enum _which, float _lod, float _duration = 0.5f)
    {
        m_skybox.update(_env, _which, _lod, _duration);
        m_ibl.update(_env, _duration);
    }

    struct SkyboxTransition
    {
        void init(cs::EnvHandle _env, cs::Environment::Enum _which)
        {
            m_currLod      = 0.0f;
            m_nextLod      = 0.0f;
            m_progress     = 0.0f;
            m_currEnv      = cs::acquire(_env);
            m_nextEnv      = cs::EnvHandle::invalid();
            m_currWhich    = _which;
            m_nextWhich    = _which;
            m_doTransition = false;
        }

        void update(cs::EnvHandle _env, cs::Environment::Enum _which, float _lod, float _duration = 0.5f)
        {
            for (;;)
            {
                if (!m_transition.active())
                {
                    const bool envMatch = (m_currEnv.m_idx == _env.m_idx);
                    const bool texMatch = envMatch && (_which == m_currWhich);
                    const bool lodMatch = (_lod == m_currLod) || (cs::Environment::Pmrem != _which);

                    if (texMatch && lodMatch)
                    {
                        m_doTransition = false;
                        return;
                    }
                    else
                    {
                        const float duration = !envMatch ? _duration
                                             : lodMatch  ? _duration*0.5f
                                             : 0.2f;
                        m_transition.start(duration);

                        m_nextEnv = cs::acquire(_env);
                        m_nextWhich = _which;
                        m_nextLod = _lod;
                    }
                }

                m_progress = m_transition.progress();

                if (m_transition.active())
                {
                    m_doTransition = true;
                    return;
                }
                else
                {
                    cs::release(m_currEnv);
                    m_currEnv = m_nextEnv;
                    m_currWhich = m_nextWhich;
                    m_currLod = m_nextLod;
                }
            }
        }

        Transition m_transition;
        float m_currLod;
        float m_nextLod;
        float m_progress;
        cs::EnvHandle m_currEnv;
        cs::EnvHandle m_nextEnv;
        cs::Environment::Enum m_currWhich;
        cs::Environment::Enum m_nextWhich;
        bool m_doTransition;
    };

    struct IblTransition
    {
        void init(cs::EnvHandle _env)
        {
            m_currEnv      = cs::acquire(_env);
            m_nextEnv      = cs::EnvHandle::invalid();
            m_progress     = 0.0f;
            m_doTransition = false;
        }

        void update(cs::EnvHandle _env, float _duration = 0.5f)
        {
            for (;;)
            {
                if (!m_transition.active())
                {
                    const bool envMatch = (m_currEnv.m_idx == _env.m_idx);
                    if (envMatch)
                    {
                        m_doTransition = false;
                        return;
                    }
                    else
                    {
                        m_transition.start(_duration);
                        m_nextEnv = cs::acquire(_env);
                    }
                }

                m_progress = m_transition.progress();

                if (m_transition.active())
                {
                    m_doTransition = true;
                    return;
                }
                else
                {
                    cs::release(m_currEnv);
                    m_currEnv = m_nextEnv;
                }
            }
        }

        Transition m_transition;
        float m_progress;
        cs::EnvHandle m_currEnv;
        cs::EnvHandle m_nextEnv;
        bool m_doTransition;
    };

    SkyboxTransition m_skybox;
    IblTransition    m_ibl;
};

static inline void submitMesh(uint8_t _view
                            , cs::MeshHandle _mesh
                            , cs::Program::Enum _prog
                            , const float* _mtx
                            , const cs::MaterialHandle* _materials
                            , const Environment& _environment
                            )
{
    if (_environment.m_ibl.m_doTransition)
    {
        cs::submit(_view, _mesh, _prog, _mtx, _materials
                 , _environment.m_ibl.m_nextEnv
                 , _environment.m_ibl.m_currEnv
                 , _environment.m_ibl.m_progress
                 );
    }
    else
    {
        cs::submit(_view, _mesh, _prog, _mtx, _materials, _environment.m_ibl.m_currEnv);
    }
}

// Uniforms.
//-----

static inline void updateUniforms(const Settings& _settings)
{
    cs::Uniforms& uniforms = cs::getUniforms();

    uniforms.m_blurCoeff            = float(_settings.m_blurCoeff);
    uniforms.m_ambientLightStrenght = float(_settings.m_ambientLightStrenght);
    uniforms.m_lightingModel        = float(_settings.m_selectedLightingModel);
    uniforms.m_backgroundType       = float(_settings.m_backgroundType);
    uniforms.m_lod                  = float(_settings.m_backgroundMipLevel);
    uniforms.m_texelHalf            = float(g_texelHalf);
    uniforms.m_toneMapping          = float(_settings.m_selectedToneMapping);
    uniforms.m_fov                  = float(bx::toRad(_settings.m_fov));
    uniforms.m_diffuseIbl           = float(_settings.m_diffuseIbl);
    uniforms.m_specularIbl          = float(_settings.m_specularIbl);
    // Post process
    uniforms.m_brightness = _settings.m_brightness;
    uniforms.m_contrast   = _settings.m_contrast;
    uniforms.m_exposure   = _settings.m_exposure;
    uniforms.m_gamma      = _settings.m_gamma;
    uniforms.m_vignette   = _settings.m_vignette;
    uniforms.m_saturation = _settings.m_saturation;
    // Hdr settings.
    uniforms.m_middleGray   = float(_settings.m_middleGray);
    uniforms.m_whiteSqr     = float(dm::squaref(_settings.m_white));
    uniforms.m_treshold     = float(_settings.m_treshold);
    uniforms.m_doBloom      = float(_settings.m_doBloom);
    uniforms.m_doLightAdapt = float(_settings.m_doLightAdapt);
}

// Scenes.
//-----

struct ControlState
{
    ControlState()
    {
        m_curr              = None;
        m_prev              = None;
        m_modelControl      = false;
        m_modalWindowActive = false;
        m_capturedByUI      = false;
    }

    void update(Mouse& _mouse, float _overGui)
    {
        const uint8_t mouseClick = (_mouse.m_left|_mouse.m_right|_mouse.m_middle);

        m_modalWindowActive = widgetIsVisible(Widget::ModalWindowMask);
        m_capturedByUI = _overGui || m_modalWindowActive;

        // Current state.
        m_prev = m_curr;

        if (Mouse::Down == _mouse.m_left || Mouse::Hold == _mouse.m_left)
        {
            m_curr = Drag;
        }
        else if (Mouse::Down == _mouse.m_right || Mouse::Hold == _mouse.m_right)
        {
            m_curr = WorldRotate;
        }
        else if (Mouse::Down == _mouse.m_middle || Mouse::Hold == _mouse.m_middle)
        {
            m_curr = Rotate;
        }
        // Note: it is maybe better to let this disabled.
        //else if (Mouse::Hold == _mouse.m_right && Mouse::Hold == _mouse.m_left)
        //{
        //    m_curr = BringCloser;
        //}
        else
        {
            m_curr = None;
        }

        // Model control.
        if (Mouse::Down == mouseClick && !m_capturedByUI)
        {
            m_modelControl = true;
        }

        if (Mouse::Up == mouseClick)
        {
            m_modelControl = false;
        }

        // Scroll.
        if (!m_capturedByUI && Drag != m_curr && BringCloser != m_curr)
        {
            m_handleScroll = true;
        }
        else
        {
            m_handleScroll = false;
        }
    }

    enum Enum
    {
        None,
        Drag,
        Rotate,
        WorldRotate,
        BringCloser,
    };

    ControlState::Enum m_curr;
    ControlState::Enum m_prev;
    bool m_handleScroll;
    bool m_modelControl;
    bool m_modalWindowActive;
    bool m_capturedByUI;
};

static inline void handleWorldRotation(float* _rotDest, const Mouse& _mouse, const ControlState& _controlState)
{
    if (_controlState.m_modelControl && ControlState::WorldRotate == _controlState.m_curr)
    {
        _rotDest[0] += _mouse.m_dy;
        _rotDest[1] += _mouse.m_dx;

        _rotDest[0] = dm::clamp(_rotDest[0], -0.5f, 0.5f);
    }
}

struct ModelScene
{
    void init(cs::MeshInstance& _inst)
    {
        setModel(_inst);
        m_dragStart[0]   = 0.0f;
        m_dragStart[1]   = 0.0f;
        m_dragStart[2]   = 0.0f;
        m_scaleDest      = 1.0f;
        m_fovScale       = 1.0f;
        m_animScaleBegin = 0.0f;
        m_animScaleEnd   = 0.0f;
        m_animRotBegin   = 0.0f;
        m_animRotEnd     = 0.0f;
        m_isAnimating    = false;
    }

    void setModel(cs::MeshInstance& _inst)
    {
        m_inst = &_inst;
        m_posDest[0] = m_inst->m_pos[0];
        m_posDest[1] = m_inst->m_pos[1];
        m_posDest[2] = m_inst->m_pos[2];
        m_rotDest[0] = m_inst->m_rot[0];
        m_rotDest[1] = m_inst->m_rot[1];
        m_rotDest[2] = m_inst->m_rot[2];
        m_scaleDest  = m_inst->m_scale;
    }

    void resetPosition()
    {
        m_posDest[0] = 0.0f;
        m_posDest[1] = 0.0f;
        m_posDest[2] = 0.0f;
    }

    void resetScale()
    {
        m_scaleDest = 1.0f;
    }

    void animate()
    {
        if (!m_isAnimating)
        {
            m_isAnimating = true;

            m_animScaleBegin = 0.1f;
            m_animScaleEnd   = m_inst->m_scale;

            m_animRotBegin = m_rotDest[1] + 0.15f;
            m_animRotEnd   = m_inst->m_rot[1];

            // Scale.
            m_scaleDest     = m_animScaleEnd;
            m_inst->m_scale = m_animScaleBegin;

            // Rotation.
            m_rotDest[1]     = m_animRotEnd;
            m_inst->m_rot[1] = m_animRotBegin;
        }
    }

    bool isAnimating() const
    {
        return m_isAnimating;
    }

    void finishAnimation()
    {
        m_inst->m_rot[1] = m_rotDest[1];
        m_inst->m_scale  = m_scaleDest;
        m_isAnimating = false;
    }

    void changeModel(cs::MeshInstance& _inst)
    {
        finishAnimation();
        setModel(_inst);
        animate();
    }

    void update(const Mouse& _mouse
              , const ControlState& _controlState
              , const Camera& _cameraDest
              , float _deltaTime
              , float _posUd = 0.1f
              , float _scaUd = 0.1f
              , float _rotUd = 0.1f
              , float _fovUd = 0.1f
              )
    {
        // React on mouse scroll.
        if (_controlState.m_handleScroll)
        {
            const float delta = float(_mouse.m_scroll)*m_scaleDest*0.1f;
            m_scaleDest = dm::clamp(m_scaleDest + delta, 0.1f, 9.99f);
        }

        // React on mouse click.
        if (_controlState.m_modelControl)
        {
            if (ControlState::Rotate == _controlState.m_curr)
            {
                m_rotDest[1] += _mouse.m_dx; // mouse dx influences rotation on y axis.
            }
            else if (ControlState::Drag == _controlState.m_curr)
            {
                float camVP[16];
                bx::mtxMul(camVP, _cameraDest.m_view, _cameraDest.m_proj);

                float inv[16];
                bx::mtxInverse(inv, camVP);

                float depthVec[3];
                bx::vec3MulMtxH(depthVec, m_posDest, camVP);

                float deltaPos[3];
                float vec[3] = { _mouse.m_vx, -_mouse.m_vy, depthVec[2] };
                bx::vec3MulMtxH(deltaPos, vec, inv);

                if (Mouse::Down == _mouse.m_left
                ||  ControlState::Drag != _controlState.m_prev)
                {
                    m_dragStart[0] = deltaPos[0];
                    m_dragStart[1] = deltaPos[1];
                    m_dragStart[2] = deltaPos[2];
                }

                if (Mouse::Hold == _mouse.m_left
                &&  Mouse::Hold != _mouse.m_right)
                {
                    m_posDest[0] += deltaPos[0] - m_dragStart[0];
                    m_posDest[1] += deltaPos[1] - m_dragStart[1];
                    m_posDest[2] += deltaPos[2] - m_dragStart[2];

                    m_dragStart[0] = deltaPos[0];
                    m_dragStart[1] = deltaPos[1];
                    m_dragStart[2] = deltaPos[2];
                }
            }
            else if (ControlState::BringCloser == _controlState.m_curr)
            {
                float toObj[3];
                float toObjNorm[3];
                bx::vec3Sub(toObj, m_posDest, _cameraDest.m_viewVec);
                bx::vec3Norm(toObjNorm, toObj);

                const float delta = _mouse.m_dy*3.0f;
                float deltaPos[3];
                bx::vec3Mul(deltaPos, toObjNorm, delta);

                const bool belowRange = bx::vec3Length(toObj) < 0.25f;
                const bool aboveRange = bx::vec3Length(toObj) > 9.0f;
                const bool inRange = !belowRange && !aboveRange;

                if (inRange || (belowRange && delta > 0.0f) || (aboveRange && delta < 0.0f))
                {
                    m_posDest[0] += deltaPos[0];
                    m_posDest[1] += deltaPos[1];
                    m_posDest[2] += deltaPos[2];
                }
            }
        }

        // Update values.
        m_inst->m_scale  = lerp(m_inst->m_scale,  m_scaleDest,  _deltaTime, _scaUd);
        m_inst->m_pos[0] = lerp(m_inst->m_pos[0], m_posDest[0], _deltaTime, _posUd);
        m_inst->m_pos[1] = lerp(m_inst->m_pos[1], m_posDest[1], _deltaTime, _posUd);
        m_inst->m_pos[2] = lerp(m_inst->m_pos[2], m_posDest[2], _deltaTime, _posUd);
        m_inst->m_rot[1] = lerp(m_inst->m_rot[1], m_rotDest[1], _deltaTime, _rotUd);

        // This adds some delay in update and makes a cool effect when chaning fov.
        m_fovScale = lerp(m_fovScale, _cameraDest.m_fovScale, _deltaTime, _fovUd);

        // Update animation state.
        if (m_isAnimating)
        {
            if (!dm::equals(m_animScaleBegin, m_animScaleEnd, 0.005f)
            ||  !dm::equals(m_animRotBegin,   m_animRotEnd,   0.005f))
            {
                m_animScaleBegin = lerp(m_animScaleBegin, m_animScaleEnd, _deltaTime, _scaUd);
                m_animRotBegin   = lerp(m_animRotBegin,   m_animRotEnd,   _deltaTime, _rotUd);
            }
            else
            {
                m_isAnimating = false;
            }
        }
    }

    void draw(uint8_t _viewId, cs::Program::Enum _prog, const Environment& _environment)
    {
        const cs::Mesh& mesh = cs::getObj(m_inst->m_mesh);

        const float scale = m_inst->m_scale
                          * m_inst->m_scaleAdj
                          * mesh.m_normScale
                          * m_fovScale;

        float mtx[16];
        bx::mtxSRT(mtx
                 , scale
                 , scale
                 , scale
                 , m_inst->m_rot[0]*dm::twoPi
                 , m_inst->m_rot[1]*dm::twoPi
                 , m_inst->m_rot[2]*dm::twoPi
                 , m_inst->m_pos[0]
                 , m_inst->m_pos[1]
                 , m_inst->m_pos[2]
                 );

        submitMesh(_viewId, m_inst->m_mesh, _prog, mtx, m_inst->m_materials.elements(), _environment);
    }

    cs::MeshInstance& getCurrInstance()
    {
        return *m_inst;
    }

private:
    cs::MeshInstance* m_inst;
    float m_posDest[3];
    float m_rotDest[3];
    float m_scaleDest;
    float m_fovScale;
    float m_dragStart[3];
    float m_animScaleBegin;
    float m_animScaleEnd;
    float m_animRotBegin;
    float m_animRotEnd;
    bool m_isAnimating;
};

struct SpheresScene
{
    void init(SpheresSceneState& _state)
    {
        m_state = &_state;

        m_posDest[0] = 0.0f;
        m_posDest[1] = 0.0f;
        m_posDest[2] = 0.0f;

        m_rotDest[0] = 0.0f;
        m_rotDest[1] = 0.0f;
        m_rotDest[2] = 0.0f;

        m_dragStart[0] = 0.0f;
        m_dragStart[1] = 0.0f;
        m_dragStart[2] = 0.0f;

        m_scaleDest = 1.0f;
        m_instScale = 1.0f;
        m_fovScale  = 1.0f;

        for (uint8_t yy = 0; yy < MAX_VERT_SPHERES; ++yy)
        {
            for (uint8_t xx = 0; xx < MAX_HORIZ_SPHERES; ++xx)
            {
                m_spherePos[yy][xx][0] = 0.0f;
                m_spherePos[yy][xx][1] = 0.0f;
                m_spherePos[yy][xx][2] = 0.0f;

                m_spherePosDest[yy][xx][0] = 0.0f;
                m_spherePosDest[yy][xx][1] = 0.0f;
                m_spherePosDest[yy][xx][2] = 0.0f;
            }
        }

        m_sphere = cs::meshSphere();

        m_material[Material::Plain]   = cs::materialCreateShiny();
        m_material[Material::Stripes] = cs::materialCreateStripes();
        m_material[Material::Brick]   = cs::materialCreateBricks();

        m_isAnimating = false;
    }

private:
    static inline void wrapRotation(float& _rot, float& _rotDest)
    {
        const int32_t overflow = int32_t(_rotDest);
        _rot     -= float(overflow);
        _rotDest -= float(overflow);
    }
public:

    void wrapRotation()
    {
        wrapRotation(m_state->m_rot[0], m_rotDest[0]);
        wrapRotation(m_state->m_rot[1], m_rotDest[1]);
        wrapRotation(m_state->m_rot[2], m_rotDest[2]);
    }

    void resetPosition()
    {
        m_posDest[0] = 0.0f;
        m_posDest[1] = 0.0f;
        m_posDest[2] = 0.0f;
    }

    void resetScale()
    {
        m_scaleDest = 1.0f;
    }

    void resetInclination()
    {
        m_state->m_incl = 0.0f;
    }

    void onControlChange(const Camera& _camera)
    {
        // Prevent awful spinning if difference between current and camera rotation is bigger than 360 deg.
        const float camY  = -_camera.m_rotY;
        const float diffY = camY - m_state->m_rot[1];
        const float absY  = fabsf(diffY);
        const float signY = dm::signf(diffY);
        m_state->m_rot[1] += dm::integerPart(absY)*signY;

        const float camX  = -_camera.m_rotX*0.5f;
        const float diffZ = camX - m_state->m_rot[2];
        const float absZ  = fabsf(diffZ);
        const float signZ = dm::signf(diffZ);
        m_state->m_rot[2] += dm::integerPart(absZ)*signZ;

        // Reset position so that spheres end up in front of the camera.
        if (m_state->m_followCamera)
        {
            resetPosition();
        }
    }

    void animate(const Camera& _camera)
    {
        if (!m_isAnimating)
        {
            m_isAnimating = true;

            // Scale.
            m_scaleDest = m_state->m_scale;
            m_state->m_scale = 0.01f;

            // Position.
            for (uint8_t yy = 0, end = uint8_t(m_state->m_vertCount); yy < end; ++yy)
            {
                for (uint8_t xx = 0, end = uint8_t(m_state->m_horizCount); xx < end; ++xx)
                {
                    m_spherePos[yy][xx][0] = 0.0f;
                    m_spherePos[yy][xx][1] = 0.0f;
                    m_spherePos[yy][xx][2] = yy*60.0f;
                }
            }
        }

        if (m_state->m_followCamera)
        {
            m_state->m_rot[0] = 0.0f;
            m_state->m_rot[1] = -_camera.m_rotY-0.25f;
            m_state->m_rot[2] = -_camera.m_rotX*0.5f;
        }
    }

    void finishAnimation()
    {
        m_state->m_scale = m_scaleDest;
        m_isAnimating = false;
    }

    void update(const Mouse& _mouse
              , const ControlState& _controlState
              , const Camera& _camera
              , const Camera& _cameraDest
              , float _deltaTime
              , float _updateDuration = 0.1f
              )
    {
        // React on mouse scroll.
        if (_controlState.m_handleScroll)
        {
            const float delta = float(_mouse.m_scroll)*m_scaleDest*0.1f;
            m_scaleDest = dm::max(0.1f, m_scaleDest + delta);
        }

        // React on mouse click.
        if (_controlState.m_modelControl)
        {
            if (m_state->m_followCamera)
            {
                if (ControlState::Rotate == _controlState.m_curr)
                {
                    m_state->m_incl -= _mouse.m_dy*1.5f;
                    m_state->m_incl = dm::clamp(m_state->m_incl, -0.8f, 0.8f);
                }
                else if (ControlState::Drag == _controlState.m_curr)
                {
                    float camVP[16];
                    bx::mtxMul(camVP, _cameraDest.m_view, _cameraDest.m_proj);

                    float inv[16];
                    bx::mtxInverse(inv, camVP);

                    float depthVec[3];
                    bx::vec3MulMtxH(depthVec, m_posDest, camVP);

                    float deltaPos[3];
                    float vec[3] = { _mouse.m_vx, -_mouse.m_vy, depthVec[2] };
                    bx::vec3MulMtxH(deltaPos, vec, inv);

                    if (Mouse::Down == _mouse.m_left
                    ||  ControlState::Drag != _controlState.m_prev)
                    {
                        m_dragStart[0] = deltaPos[0];
                        m_dragStart[1] = -_mouse.m_vy;
                        m_dragStart[2] = deltaPos[2];
                    }

                    if (Mouse::Hold == _mouse.m_left
                    &&  Mouse::Hold != _mouse.m_right)
                    {
                        m_posDest[1] += -_mouse.m_vy - m_dragStart[1];
                        m_posDest[1] = dm::clamp(m_posDest[1], -1.0f, 1.0f);

                        m_dragStart[0] = deltaPos[0];
                        m_dragStart[1] = -_mouse.m_vy;
                        m_dragStart[2] = deltaPos[2];
                    }
                }
            }
            else
            {
                if (ControlState::Rotate == _controlState.m_curr)
                {
                    m_rotDest[1] += _mouse.m_dx; // mouse dx influences rotation on y axis.

                    m_state->m_incl -= _mouse.m_dy*1.5f;
                    m_state->m_incl = dm::clamp(m_state->m_incl, -1.0f, 1.0f);
                }
                else if (ControlState::Drag == _controlState.m_curr)
                {
                    float camVP[16];
                    bx::mtxMul(camVP, _cameraDest.m_view, _cameraDest.m_proj);

                    float inv[16];
                    bx::mtxInverse(inv, camVP);

                    float depthVec[3];
                    bx::vec3MulMtxH(depthVec, m_posDest, camVP);

                    float deltaPos[3];
                    float vec[3] = { _mouse.m_vx, -_mouse.m_vy, depthVec[2] };
                    bx::vec3MulMtxH(deltaPos, vec, inv);

                    if (Mouse::Down == _mouse.m_left
                    ||  ControlState::Drag != _controlState.m_prev)
                    {
                        m_dragStart[0] = deltaPos[0];
                        m_dragStart[1] = deltaPos[1];
                        m_dragStart[2] = deltaPos[2];
                    }

                    if (Mouse::Hold == _mouse.m_left
                    &&  Mouse::Hold != _mouse.m_right)
                    {
                        m_posDest[0] += deltaPos[0] - m_dragStart[0];
                        m_posDest[1] += deltaPos[1] - m_dragStart[1];
                        m_posDest[2] += deltaPos[2] - m_dragStart[2];

                        m_dragStart[0] = deltaPos[0];
                        m_dragStart[1] = deltaPos[1];
                        m_dragStart[2] = deltaPos[2];
                    }
                }
                else if (ControlState::BringCloser == _controlState.m_curr)
                {
                    float toObj[3];
                    float toObjNorm[3];
                    bx::vec3Sub(toObj, m_posDest, _cameraDest.m_viewVec);
                    bx::vec3Norm(toObjNorm, toObj);

                    const float delta = _mouse.m_dy*3.0f;
                    float deltaPos[3];
                    bx::vec3Mul(deltaPos, toObjNorm, delta);

                    const bool belowRange = bx::vec3Length(toObj) < 0.25f;
                    const bool aboveRange = bx::vec3Length(toObj) > 9.0f;
                    const bool inRange = !belowRange && !aboveRange;

                    if (inRange || (belowRange && delta > 0.0f) || (aboveRange && delta < 0.0f))
                    {
                        m_posDest[0] += deltaPos[0];
                        m_posDest[1] += deltaPos[1];
                        m_posDest[2] += deltaPos[2];
                    }
                }
            }
        }

        // Always face camera if 'followCamera' is set.
        if (m_state->m_followCamera)
        {
            m_rotDest[0] = 0.0f;
            m_rotDest[1] = -_camera.m_rotY-0.25f;
            m_rotDest[2] = -_camera.m_rotX*0.5f;
        }

        // Update values for the whole group.
        m_state->m_scale  = lerp(m_state->m_scale,  m_scaleDest,  _deltaTime, _updateDuration);
        m_state->m_pos[0] = lerp(m_state->m_pos[0], m_posDest[0], _deltaTime, _updateDuration);
        m_state->m_pos[1] = lerp(m_state->m_pos[1], m_posDest[1], _deltaTime, _updateDuration);
        m_state->m_pos[2] = lerp(m_state->m_pos[2], m_posDest[2], _deltaTime, _updateDuration);
        m_state->m_rot[1] = lerp(m_state->m_rot[1], m_rotDest[1], _deltaTime, _updateDuration);
        m_state->m_rot[2] = lerp(m_state->m_rot[2], m_rotDest[2]-m_state->m_incl*0.25f, _deltaTime, _updateDuration);

        // Determine and update values for each sphere individually.
        const float hbegin = -(4.0f/3.0f)*m_fovScale;
        const float hend   =  (4.0f/3.0f)*m_fovScale;
        const float hrange = hend-hbegin;
        const float hstep  = hrange/(m_state->m_horizCount+1.0f);

        const float vbegin = -m_fovScale;
        const float vend   =  m_fovScale;
        const float vrange = vend-vbegin;
        const float vstep  = vrange/(m_state->m_vertCount +1.0f);

        const int32_t horizCount = int32_t(m_state->m_horizCount);
        const int32_t vertCount  = int32_t(m_state->m_vertCount);

        float yyf = 1.0f;
        for (int32_t yy = 0; yy < MAX_VERT_SPHERES; ++yy, yyf += 1.0f)
        {
            float xxf = 1.0f;
            for (int32_t xx = 0; xx < MAX_HORIZ_SPHERES; ++xx, xxf += 1.0f)
            {
                const float sx = hbegin + hstep*xxf;
                const float sy = vbegin + vstep*yyf;

                if (xx >= horizCount || yy >= vertCount)
                {
                    // If not visible on screen, move them apart.
                    m_spherePosDest[yy][xx][1] = (yy&1) ? sy+0.5f : sy-0.5f;
                    m_spherePosDest[yy][xx][2] = (xx&1) ? sx+0.5f : sx-0.5f;
                }
                else
                {
                    m_spherePosDest[yy][xx][1] = sy;
                    m_spherePosDest[yy][xx][2] = sx;
                }

                m_spherePos[yy][xx][0] = lerp(m_spherePos[yy][xx][0], m_spherePosDest[yy][xx][0], _deltaTime, _updateDuration);
                m_spherePos[yy][xx][1] = lerp(m_spherePos[yy][xx][1], m_spherePosDest[yy][xx][1], _deltaTime, _updateDuration);
                m_spherePos[yy][xx][2] = lerp(m_spherePos[yy][xx][2], m_spherePosDest[yy][xx][2], _deltaTime, _updateDuration);
            }
        }

        const float maxCount = DM_MAX(m_state->m_horizCount, m_state->m_vertCount);
        const float scale = (1.0f/maxCount) * (1.25f + maxCount*0.04f);
        m_instScale = lerp(m_instScale, scale, _deltaTime, _updateDuration);

        m_fovScale = lerp(m_fovScale, _camera.m_fovScale, _deltaTime, _updateDuration);

        // Update animation state.
        if (m_isAnimating)
        {
            const bool isStillAnimating = !dm::equals(m_state->m_scale, m_scaleDest, 0.01f);
            m_isAnimating = isStillAnimating;
        }
    }

    void draw(uint8_t _viewId, cs::Program::Enum _prog, const Environment& _environment)
    {
        const cs::Mesh& mesh = cs::getObj(m_sphere);
        cs::Material& mat = cs::getObj(m_material[m_state->m_matSelection]);

        const float groupScaleAdjust = 0.9f;

        float groupMtx[16];
        bx::mtxSRT(groupMtx
                 , m_state->m_scale*groupScaleAdjust
                 , m_state->m_scale*groupScaleAdjust
                 , m_state->m_scale*groupScaleAdjust
                 , m_state->m_rot[0]*dm::twoPi
                 , m_state->m_rot[1]*dm::twoPi
                 , m_state->m_rot[2]*dm::twoPi
                 , m_state->m_pos[0]
                 , m_state->m_pos[1]
                 , m_state->m_pos[2]
                 );

        const float sphereScale = mesh.m_normScale
                                * m_instScale
                                * m_fovScale;

        float mstep;
        float gstep;
        if (m_state->m_horizMetalness)
        {
            mstep = 1.0f/dm::max(1.0f, m_state->m_horizCount-1.0f);
            gstep = 1.0f/dm::max(1.0f, m_state->m_vertCount -1.0f);
        }
        else
        {
            mstep = 1.0f/dm::max(1.0f, m_state->m_vertCount -1.0f);
            gstep = 1.0f/dm::max(1.0f, m_state->m_horizCount-1.0f);
        }

        float cstep;
        if (m_state->m_leftToRightColor)
        {
            cstep = 1.0f/dm::max(1.0f, m_state->m_horizCount-1.0f);
        }
        else
        {
            cstep = 1.0f/dm::max(1.0f, m_state->m_vertCount-1.0f);
        }

        float yyf = 0.0f;
        for (int32_t yy = 0, end = int32_t(m_state->m_vertCount); yy < end; ++yy, yyf += 1.0f)
        {
            float xxf = 0.0f;
            for (int32_t xx = 0, end = int32_t(m_state->m_horizCount); xx < end; ++xx, xxf += 1.0f)
            {
                // Setup material.
                if (m_state->m_horizMetalness)
                {
                    const float mval = mstep*xxf;
                    const float gval = gstep*yyf;
                    mat.m_surface.g    = m_state->m_vertIncr  ? gval : 1.0f - gval;
                    mat.m_reflectivity = m_state->m_horizIncr ? mval : 1.0f - mval;
                }
                else // vertical metalness
                {
                    const float mval = mstep*yyf;
                    const float gval = gstep*xxf;
                    mat.m_surface.g    = m_state->m_horizIncr ? gval : 1.0f - gval;
                    mat.m_reflectivity = m_state->m_vertIncr  ? mval : 1.0f - mval;
                }

                const float cval = (m_state->m_leftToRightColor ? 1.0f-cstep*xxf : cstep*yyf);
                mat.m_albedo.r = bx::flerp(m_state->m_color1[0], m_state->m_color0[0], cval);
                mat.m_albedo.g = bx::flerp(m_state->m_color1[1], m_state->m_color0[1], cval);
                mat.m_albedo.b = bx::flerp(m_state->m_color1[2], m_state->m_color0[2], cval);

                // Setup matrix.
                float sphereMtx[16];
                bx::mtxSRT(sphereMtx
                         , sphereScale
                         , sphereScale
                         , sphereScale
                         , 0.0f
                         , 0.0f
                         , 0.0f
                         , m_spherePos[yy][xx][0]
                         , m_spherePos[yy][xx][1]
                         , m_spherePos[yy][xx][2]
                         );
                float mtx[16];
                bx::mtxMul(mtx, sphereMtx, groupMtx);

                // Draw.
                submitMesh(_viewId, m_sphere, _prog, mtx, (cs::MaterialHandle*)&m_material[m_state->m_matSelection], _environment);
            }
        }
    }

private:
    enum
    {
        MAX_HORIZ_SPHERES = 12,
        MAX_VERT_SPHERES  = 12,
    };

    struct Material
    {
        enum Enum
        {
            Plain,
            Stripes,
            Brick,

            Count,
        };
    };

    SpheresSceneState* m_state;
    float m_posDest[3];
    float m_rotDest[3];
    float m_dragStart[3];
    float m_scaleDest;
    float m_instScale;
    float m_fovScale;
    float m_spherePos[MAX_VERT_SPHERES][MAX_HORIZ_SPHERES][3];
    float m_spherePosDest[MAX_VERT_SPHERES][MAX_HORIZ_SPHERES][3];
    cs::MeshHandle m_sphere;
    cs::MaterialHandle m_material[Material::Count];
    bool m_isAnimating;
};

struct MaterialPreview
{
    MaterialPreview()
    {
        m_rot[0]     = 0.2f;
        m_rot[1]     = 0.0f;
        m_rotDest[0] = 0.2f;
        m_rotDest[1] = 0.0f;
        m_eye[0]     = 1.0f;
        m_eye[1]     = 0.0f;
        m_eye[2]     = 0.0f;
        m_active     = false;
    }

    void makeActive()
    {
        m_active = true;
    }

    void spin()
    {
        m_rot[1] -= 0.1f*dm::pi;
    }

    void update(const Mouse& _mouse, float _deltaTime, float _updateDuration = 0.1f)
    {
        const bool mouseDown = Mouse::Down == _mouse.m_left
                            || Mouse::Down == _mouse.m_right
                            || Mouse::Down == _mouse.m_middle;
        const bool mouseHold = Mouse::Hold == _mouse.m_left
                            || Mouse::Hold == _mouse.m_right
                            || Mouse::Hold == _mouse.m_middle;

        if (m_active && (mouseHold || mouseDown))
        {
            m_rotDest[0] += _mouse.m_dy*2.0f;
            m_rotDest[1] += _mouse.m_dx*2.0f;

            m_rotDest[0] = dm::clamp(m_rotDest[0], -0.5f, 0.5f);
        }
        else
        {
            m_active = false;
        }

        m_rot[0] = lerp(m_rot[0], m_rotDest[0], _deltaTime, _updateDuration);
        m_rot[1] = lerp(m_rot[1], m_rotDest[1], _deltaTime, _updateDuration);

        float mtx[16];
        bx::mtxRotateXY(mtx, -m_rot[0]*dm::pi, -m_rot[1]*dm::twoPi);

        const float look[3] = { 0.0f, 0.0f, -1.0f };
        bx::vec3MulMtx(m_eye, look, mtx);
    }

    void draw(cs::MaterialHandle _mat, cs::EnvHandle _env)
    {
        updateMaterialPreview(_mat, _env, m_eye);
    }

private:
    float m_rot[2];
    float m_rotDest[2];
    float m_eye[3];
    bool m_active;
};

// CmftStudio
//-----

void cmdToggleFullscreen(const void* /*_userData*/)
{
    entry::WindowHandle window = { 0 };
    entry::toggleFullscreen(window);
}

static const InputBinding s_keyBindings[] =
{
    #if BX_PLATFORM_OSX
    { entry::Key::KeyF, entry::Modifier::LeftMeta,                            1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::RightMeta,                           1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::RightMeta|entry::Modifier::LeftMeta, 1, cmdToggleFullscreen,  NULL },
    #endif //BX_PLATFORM_OSX
    { entry::Key::KeyF, entry::Modifier::LeftAlt,                             1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::RightAlt,                            1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::RightCtrl|entry::Modifier::LeftCtrl, 1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::LeftCtrl,                            1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::RightCtrl,                           1, cmdToggleFullscreen,  NULL },
    { entry::Key::KeyF, entry::Modifier::RightCtrl|entry::Modifier::LeftCtrl, 1, cmdToggleFullscreen,  NULL },
    INPUT_BINDING_END
};

struct CmftStudioApp
{
private:
    static inline void cmftSaveAction(cs::EnvHandle _env, CmftSaveWidgetState& _widget)
    {
        const cs::Environment& env = cs::getObj(_env);
        const cmft::Image& output = env.m_cubemapImage[_widget.m_envType];

        const bool hasMem = (NULL != output.m_data) && (0 != output.m_dataSize);
        if (!hasMem)
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Saving '%s' failed.", _widget.m_outputNameExt);
            imguiStatusMessage(msg, 6.0f, true, "Close");

            return;
        }

        outputWindowShow();
        outputWindowClear();
        outputWindowPrint("File:      %s", _widget.m_outputNameExt);
        outputWindowPrint("Directory: %s", _widget.m_directory);

        outputWindowPrint("---------------------------------------------------------------------------------------------------------");

        char path[DM_PATH_LEN];
        bx::snprintf(path, sizeof(path), "%s" DM_DIRSLASH "%s", _widget.m_directory, _widget.m_outputName);

        const bool saved = cmft::imageSave(output
                                         , path
                                         , _widget.m_fileType
                                         , _widget.m_outputType
                                         , _widget.m_textureFormat
                                         , true
                                         );

        if (saved)
        {
            outputWindowPrint("---------------------------------------------------------------------------------------------------------");
            outputWindowPrint("File saved.");

            char msg[128];
            bx::snprintf(msg, sizeof(msg), "File '%s' saved!", _widget.m_outputNameExt);
            imguiStatusMessage(msg, 3.0f, false, "Close");
        }
        else
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Saving '%s' failed.", _widget.m_outputNameExt);
            imguiStatusMessage(msg, 6.0f, true, "Close");

            outputWindowPrint("Saving '%s' failed!", _widget.m_outputNameExt);
        }
    }

    static inline void envBrowseAction(EnvMapBrowserState& _state, cs::EnvHandle _currentEnv)
    {
        const bool loaded = cs::envLoad(_currentEnv, _state.m_selection, _state.m_filePath);

        if (loaded)
        {
            cs::createGpuBuffers(_currentEnv);

            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Cubemap '%s' successfully loaded.", _state.m_fileName);
            imguiStatusMessage(msg, 3.0f, false, "Close");
        }
        else
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Cubemap '%s' loading failed! Selected file is invalid.", _state.m_fileName);
            imguiStatusMessage(msg, 6.0f, true, "Close");
        }
    }

    void guiActionHandler()
    {
        cs::MeshInstance&   instance  = m_meshInstList[m_settings.m_selectedMeshIdx];
        const cs::EnvHandle envHandle = m_envList[m_settings.m_selectedEnvMap];

        // LeftScrollArea action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_leftScrollArea.m_events))
        {
            // Handle mesh removal.
            if (LeftScrollAreaState::RemoveMesh == m_widgets.m_leftScrollArea.m_action)
            {
                // Remove from the list.
                listRemoveRelease(m_meshInstList, m_widgets.m_leftScrollArea.m_data);

                // Make sure there is at least one mesh instance.
                if (m_meshInstList.count() == 0)
                {
                    cs::MeshInstance* inst = m_meshInstList.addNew();
                    inst->set(cs::meshSphere());

                    if (m_materialList.count() == 0)
                    {
                        m_materialList.add(cs::materialCreateStripes());
                    }

                    inst->set(m_materialList[0]);
                }

                // Adjust selected idx and set it.
                m_settings.m_selectedMeshIdx = dm::min(m_settings.m_selectedMeshIdx, uint16_t(m_meshInstList.count()-1));
                m_modelScene.changeModel(m_meshInstList[m_settings.m_selectedMeshIdx]);
            }
            // Handle material removal.
            else if (LeftScrollAreaState::RemoveMaterial == m_widgets.m_leftScrollArea.m_action)
            {
                const cs::MaterialHandle matHandle = { m_widgets.m_leftScrollArea.m_data };

                // Remove from the list.
                const uint16_t idx = m_materialList.idxOf(matHandle);
                listRemoveRelease(m_materialList, idx);

                // Make sure there is at least one material.
                if (m_materialList.count() == 0)
                {
                    // Create a plain material.
                    m_materialList.add(cs::materialCreateStripes());
                }

                // Assign the first material from the list.
                instance.set(m_materialList[0]);
            }
            // Handle material copy.
            else if (LeftScrollAreaState::CopyMaterial == m_widgets.m_leftScrollArea.m_action)
            {
                const cs::MaterialHandle matHandle = { m_widgets.m_leftScrollArea.m_data };

                // Make a copy.
                const cs::MaterialHandle cpy = cs::materialCreateFrom(matHandle);

                // Specify a name.
                char name[32];
                bx::snprintf(name, sizeof(name), "%s - Copy", cs::getName(matHandle));
                cs::setName(cpy, name);

                // Add to the list.
                m_materialList.add(cpy);

                // Assign the newly created material.
                instance.set(cpy, instance.m_selGroup);

            }
            // Handle material creation.
            else if (LeftScrollAreaState::CreateMaterial == m_widgets.m_leftScrollArea.m_action)
            {
                // Create a plain material.
                const cs::MaterialHandle mat = cs::materialCreateShiny();
                cs::setName(mat, "Unnamed");

                // Add to the list.
                m_materialList.add(mat);

                // Assign the newly created material.
                instance.set(mat, instance.m_selGroup);
            }
        }

        // RightScrollArea action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_rightScrollArea.m_events))
        {
            // Remove env.
            if (RightScrollAreaState::RemoveEnv == m_widgets.m_rightScrollArea.m_action)
            {
                // Destroy and remove from the list.
                listRemoveRelease(m_envList, m_widgets.m_rightScrollArea.m_data);

                // Make sure there is at least one environment.
                if (m_envList.count() == 0)
                {
                    m_envList.add(cs::envCreateCmftStudioLogo());
                }

                // Adjust selected idx.
                m_settings.m_selectedEnvMap = dm::min(m_settings.m_selectedEnvMap, uint16_t(m_envList.count()-1));
            }
            // Create env.
            else if (RightScrollAreaState::CreateEnv == m_widgets.m_rightScrollArea.m_action)
            {
                // Create new.
                m_envList.add(cs::envCreateCmftStudioLogo());

                // Adjust selected idx.
                m_settings.m_selectedEnvMap = m_envList.count()-1;
            }
            else if (RightScrollAreaState::ToggleFullscreen == m_widgets.m_rightScrollArea.m_action)
            {
                cmdToggleFullscreen(NULL);
            }
        }

        // TexPickerWidget action.
        for (uint8_t ii = 0; ii < cs::Material::TextureCount; ++ii)
        {
            const cs::Material::Texture matTex = (cs::Material::Texture)ii;

            if (guiEvent(GuiEvent::HandleAction, m_widgets.m_texPicker[matTex].m_events))
            {
                if (TexPickerWidgetState::Pick == m_widgets.m_texPicker[matTex].m_action)
                {
                    const cs::TextureHandle texture = m_textureList[m_widgets.m_texPicker[matTex].m_selection];

                    cs::Material& material = cs::getObj(instance.m_materials[instance.m_selGroup]);
                    material.set(matTex, texture);
                }
                else if (TexPickerWidgetState::Remove == m_widgets.m_texPicker[matTex].m_action)
                {
                    listRemoveRelease(m_textureList, m_widgets.m_texPicker[matTex].m_selection);
                    m_widgets.m_texPicker[matTex].m_selection = DM_MIN(m_widgets.m_texPicker[matTex].m_selection, m_textureList.count()-1);
                }
            }
        }

        // SkyboxBrowser action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_skyboxBrowser.m_events))
        {
            envBrowseAction(m_widgets.m_skyboxBrowser, envHandle);
        }

        // PmremBrowser action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_pmremBrowser.m_events))
        {
            envBrowseAction(m_widgets.m_pmremBrowser, envHandle);
        }

        // IemBrowser action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_iemBrowser.m_events))
        {
            envBrowseAction(m_widgets.m_iemBrowser, envHandle);
        }

        // TextureBrowser action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_textureBrowser.m_events))
        {
            for (uint16_t ii = m_widgets.m_textureBrowser.m_files.count(); ii--; )
            {
                const TextureBrowserWidgetState::BrowserState::File& file = m_widgets.m_textureBrowser.m_files[ii];

                if ('\0' != file.m_path[0])
                {
                    const cs::TextureHandle texture = cs::textureLoad(file.m_path);
                    cs::setName(texture, file.m_name);
                    m_textureList.add(texture);
                    m_widgets.m_texPicker[m_widgets.m_textureBrowser.m_texPickerFor].m_selection = m_textureList.count()-1;
                }
            }

            m_widgets.m_textureBrowser.m_files.reset();
        }

        // CmftInfoSkybox action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftInfoSkybox.m_events))
        {
            // Resize.
            if (CmftInfoWidgetState::Resize == m_widgets.m_cmftInfoSkybox.m_action)
            {
                cs::envResize(m_widgets.m_cmftInfoSkybox.m_env, cs::Environment::Skybox, uint32_t(m_widgets.m_cmftInfoSkybox.m_resize));
                imguiStatusMessage("Skybox image resized!", 3.0f, false);
            }
            // Convert.
            else if (CmftInfoWidgetState::Convert == m_widgets.m_cmftInfoSkybox.m_action)
            {
                const cmft::TextureFormat::Enum currentFormat = envGetImage(m_widgets.m_cmftInfoSkybox.m_env, cs::Environment::Skybox).m_format;
                const cmft::TextureFormat::Enum convertTo = m_widgets.m_cmftInfoSkybox.selectedCmftFormat();

                // Convert.
                cs::envConvert(m_widgets.m_cmftInfoSkybox.m_env, cs::Environment::Skybox, convertTo);

                // Notify user.
                char msg[128];
                bx::snprintf(msg, sizeof(msg)
                           , "Skybox image format converted from '%s' to '%s'."
                           , cmft::getTextureFormatStr(currentFormat)
                           , cmft::getTextureFormatStr(convertTo)
                           );
                imguiStatusMessage(msg, 4.0f, false, "Close");
            }
        }

        // CmftInfoPmrem convert action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftInfoPmrem.m_events))
        {
            const cmft::TextureFormat::Enum currentFormat = envGetImage(m_widgets.m_cmftInfoPmrem.m_env, cs::Environment::Pmrem).m_format;
            const cmft::TextureFormat::Enum convertTo = m_widgets.m_cmftInfoPmrem.selectedCmftFormat();

            // Convert.
            cs::envConvert(m_widgets.m_cmftInfoPmrem.m_env, cs::Environment::Pmrem, convertTo);

            // Notify user.
            char msg[128];
            bx::snprintf(msg, sizeof(msg)
                       , "Radiance image format converted from '%s' to '%s'."
                       , cmft::getTextureFormatStr(currentFormat)
                       , cmft::getTextureFormatStr(convertTo)
                       );
            imguiStatusMessage(msg, 4.0f, false, "Close");
        }

        // CmftInfoIem convert action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftInfoIem.m_events))
        {
            const cmft::TextureFormat::Enum currentFormat = envGetImage(m_widgets.m_cmftInfoIem.m_env, cs::Environment::Iem).m_format;
            const cmft::TextureFormat::Enum convertTo = m_widgets.m_cmftInfoIem.selectedCmftFormat();

            // Convert.
            cs::envConvert(m_widgets.m_cmftInfoIem.m_env, cs::Environment::Iem, convertTo);

            // Notify user.
            char msg[128];
            bx::snprintf(msg, sizeof(msg)
                       , "Irraidance image format converted from '%s' to '%s'."
                       , cmft::getTextureFormatStr(currentFormat)
                       , cmft::getTextureFormatStr(convertTo)
                       );
            imguiStatusMessage(msg, 4.0f, false, "Close");
        }

        // CmftSaveSkybox action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftSaveSkybox.m_events))
        {
            cmftSaveAction(m_envList[m_settings.m_selectedEnvMap], m_widgets.m_cmftSaveSkybox);
        }

        // CmftSavePmrem action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftSavePmrem.m_events))
        {
            cmftSaveAction(m_envList[m_settings.m_selectedEnvMap], m_widgets.m_cmftSavePmrem);
        }

        // CmftSaveIem action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftSaveIem.m_events))
        {
            cmftSaveAction(m_envList[m_settings.m_selectedEnvMap], m_widgets.m_cmftSaveIem);
        }

        // CmftPmremWidget action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftPmrem.m_events))
        {
            if (!m_backgroundThread.isRunning()
            &&  ThreadStatus::Idle == m_threadParams.m_cmftFilter.m_threadStatus)
            {
                // Hide CmftPmremWidget.
                widgetHide(Widget::RightSideSubwidgetMask);

                // Copy parameters.
                const cs::EnvHandle handle = m_envList[m_settings.m_selectedEnvMap];
                const cs::Environment& env = cs::getObj(handle);
                cmft::imageRef(m_threadParams.m_cmftFilter.m_input, env.m_cubemapImage[cs::Environment::Skybox]);
                m_threadParams.m_cmftFilter.m_srcSize       = uint32_t(m_widgets.m_cmftPmrem.m_srcSize);
                m_threadParams.m_cmftFilter.m_dstSize       = uint32_t(m_widgets.m_cmftPmrem.m_dstSize);
                m_threadParams.m_cmftFilter.m_inputGamma    = m_widgets.m_cmftPmrem.m_inputGamma;
                m_threadParams.m_cmftFilter.m_outputGamma   = m_widgets.m_cmftPmrem.m_outputGamma;
                m_threadParams.m_cmftFilter.m_mipCount      = uint8_t(m_widgets.m_cmftPmrem.m_mipCount);
                m_threadParams.m_cmftFilter.m_glossScale    = uint8_t(m_widgets.m_cmftPmrem.m_glossScale);
                m_threadParams.m_cmftFilter.m_glossBias     = uint8_t(m_widgets.m_cmftPmrem.m_glossBias);
                m_threadParams.m_cmftFilter.m_numCpuThreads = uint8_t(m_widgets.m_cmftPmrem.m_numCpuThreads);
                m_threadParams.m_cmftFilter.m_filterType    = cs::Environment::Pmrem;
                m_threadParams.m_cmftFilter.m_lightingModel = (cmft::LightingModel::Enum)m_widgets.m_cmftPmrem.m_lightingModel;
                m_threadParams.m_cmftFilter.m_edgeFixup     = (cmft::EdgeFixup::Enum)m_widgets.m_cmftPmrem.m_edgeFixup;
                m_threadParams.m_cmftFilter.m_excludeBase   = m_widgets.m_cmftPmrem.m_excludeBase;
                m_threadParams.m_cmftFilter.m_useOpenCL     = m_widgets.m_cmftPmrem.m_useOpenCL;
                m_threadParams.m_cmftFilter.m_envHandle     = handle;

                // Start background thread.
                m_backgroundThread.init(cmftFilterFunc, (void*)&m_threadParams.m_cmftFilter);
            }
            else
            {
                const char* msg = "cmft processing could not be started at this time. Background thread is in use!";
                imguiStatusMessage(msg, 6.0f, true, "Close");
            }
        }

        // CmftIemWidget action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftIem.m_events))
        {
            if (!m_backgroundThread.isRunning()
            &&  ThreadStatus::Idle == m_threadParams.m_cmftFilter.m_threadStatus)
            {
                // Hide CmftIemWidget.
                widgetHide(Widget::RightSideSubwidgetMask);

                // Copy parameters.
                const cs::EnvHandle handle = m_envList[m_settings.m_selectedEnvMap];
                const cs::Environment& env = cs::getObj(handle);
                cmft::imageRef(m_threadParams.m_cmftFilter.m_input, env.m_cubemapImage[cs::Environment::Skybox]);
                m_threadParams.m_cmftFilter.m_srcSize     = uint32_t(m_widgets.m_cmftIem.m_srcSize);
                m_threadParams.m_cmftFilter.m_dstSize     = uint32_t(m_widgets.m_cmftIem.m_dstSize);
                m_threadParams.m_cmftFilter.m_inputGamma  = m_widgets.m_cmftIem.m_inputGamma;
                m_threadParams.m_cmftFilter.m_outputGamma = m_widgets.m_cmftIem.m_outputGamma;
                m_threadParams.m_cmftFilter.m_filterType  = cs::Environment::Iem;
                m_threadParams.m_cmftFilter.m_envHandle   = handle;

                // Start background thread.
                m_backgroundThread.init(cmftFilterFunc, (void*)&m_threadParams.m_cmftFilter);
            }
            else
            {
                const char* msg = "cmft processing could not be started at this time. Background thread is in use!";
                imguiStatusMessage(msg, 6.0f, true, "Close");
            }
        }

        // CmftTransform action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_cmftTransform.m_events))
        {
            cs::envTransform(m_widgets.m_cmftTransform.m_env, cs::Environment::Skybox, m_widgets.m_cmftTransform.getTrasformaArgs());
        }

        // Tonemap action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_tonemapWidget.m_events))
        {
            if (TonemapWidgetState::Original == m_widgets.m_tonemapWidget.m_selection)
            {
                envRestoreSkybox(m_widgets.m_tonemapWidget.m_env);
                imguiStatusMessage("Original skybox image restored!", 3.0f, false);
            }
            else //if (TonemapWidgetState::Tonemapped == m_widgets.m_tonemapWidget.m_selection).
            {
                envTonemap(m_widgets.m_tonemapWidget.m_env
                         , 1.0f/m_widgets.m_tonemapWidget.m_invGamma
                         , m_widgets.m_tonemapWidget.m_minLum
                         , m_widgets.m_tonemapWidget.m_lumRange
                         );
                imguiStatusMessage("Tonemap operator applied!", 3.0f, false);
            }
        }

        // CmftFilter thread result.
        if (ThreadStatus::Completed & m_threadParams.m_cmftFilter.m_threadStatus)
        {
            if (threadStatus(ThreadStatus::ExitSuccess, m_threadParams.m_cmftFilter.m_threadStatus))
            {
                if (cs::Environment::Iem == m_threadParams.m_cmftFilter.m_filterType)
                {
                    cs::envLoad(m_threadParams.m_cmftFilter.m_envHandle, cs::Environment::Iem, m_threadParams.m_cmftFilter.m_output);
                    cs::createGpuBuffers(m_threadParams.m_cmftFilter.m_envHandle);
                    imguiRemoveStatusMessage(StatusWindowId::FilterIem);
                    imguiStatusMessage("Irraidance filter completed!", 3.0f, false, "Close");
                }
                else //if (cs::Environment::Pmrem == m_threadParams.m_cmftFilter.m_filterType).
                {
                    cs::Environment& env = cs::getObj(m_threadParams.m_cmftFilter.m_envHandle);
                    env.m_edgeFixup = (cmft::EdgeFixup::Enum)m_threadParams.m_cmftFilter.m_edgeFixup;

                    cs::envLoad(m_threadParams.m_cmftFilter.m_envHandle, cs::Environment::Pmrem, m_threadParams.m_cmftFilter.m_output);
                    cs::createGpuBuffers(m_threadParams.m_cmftFilter.m_envHandle);
                    imguiRemoveStatusMessage(StatusWindowId::FilterPmrem);
                    imguiStatusMessage("Radiance filter completed!", 3.0f, false, "Close");
                }
            }
            else
            {
                if (cs::Environment::Iem == m_threadParams.m_cmftFilter.m_filterType)
                {
                    imguiRemoveStatusMessage(StatusWindowId::FilterIem);
                    imguiStatusMessage("Irraidance filter failed!", 3.0f, true, "Close");
                }
                else //if (cs::Environment::Pmrem == m_threadParams.m_cmftFilter.m_filterType).
                {
                    imguiRemoveStatusMessage(StatusWindowId::FilterPmrem);
                    imguiStatusMessage("Radiance filter failed!", 3.0f, true, "Close");
                }
            }

            // Cleanup.
            if (m_backgroundThread.isRunning())
            {
                m_backgroundThread.shutdown();
            }
            m_threadParams.m_cmftFilter.m_threadStatus = ThreadStatus::Idle;
        }

        // MeshSaveWidget action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_meshSave.m_events))
        {
            char meshPath[DM_PATH_LEN];
            bx::snprintf(meshPath, sizeof(meshPath), "%s" DM_DIRSLASH "%s.bin"
                       , m_widgets.m_meshSave.m_directory
                       , m_widgets.m_meshSave.m_outputName
                       );

            const bool success = cs::meshSave(m_widgets.m_meshSave.m_mesh, meshPath);

            char msg[128];
            if (success)
            {
                bx::snprintf(msg, sizeof(msg), "Mesh '%s' successfully saved.", m_widgets.m_meshSave.m_outputName);
                imguiStatusMessage(msg, 3.0f, false, "Close");
            }
            else
            {
                bx::snprintf(msg, sizeof(msg), "Mesh '%s' saving failed!", m_widgets.m_meshSave.m_outputName);
                imguiStatusMessage(msg, 6.0f, true, "Close");
            }
        }

        // MeshBrowser action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_meshBrowser.m_events))
        {
            if ('\0' != m_widgets.m_meshBrowser.m_filePath[0])
            {
                // Obj loading is taking a long time, do it in a background thread.
                if (0 == strcmp("obj", m_widgets.m_meshBrowser.m_fileExt))
                {
                    if (!m_backgroundThread.isRunning() && ThreadStatus::Idle == m_threadParams.m_cmftFilter.m_threadStatus)
                    {
                        // Setup parameters.
                        CS_CHECK(sizeof(m_threadParams.m_modelLoad.m_userData) >= sizeof(ObjInData), "Array overflow!");
                        ObjInData* obj = (ObjInData*)m_threadParams.m_modelLoad.m_userData;
                        obj->m_scale       = 1.0f;
                        obj->m_packUv      = 1;
                        obj->m_packNormal  = 1;
                        obj->m_ccw         = m_widgets.m_meshBrowser.m_ccw;
                        obj->m_flipV       = m_widgets.m_meshBrowser.m_flipV;
                        obj->m_calcTangent = true;

                        dm::strscpya(m_threadParams.m_modelLoad.m_filePath, m_widgets.m_meshBrowser.m_filePath);
                        dm::strscpya(m_threadParams.m_modelLoad.m_fileName, m_widgets.m_meshBrowser.m_fileName);

                        // Acquire stack allocator for this thread.
                        m_threadParams.m_modelLoad.m_stackAlloc = dm::allocSplitStack(DM_MEGABYTES(200), DM_MEGABYTES(400));

                        // Start thread.
                        m_backgroundThread.init(modelLoadFunc, (void*)&m_threadParams.m_modelLoad);
                    }
                    else
                    {
                        const char* msg = "Mesh could not be converted right now. Background thread is in use!";
                        imguiStatusMessage(msg, 6.0f, true, "Close");
                    }
                }
                else // Load mesh in this thread.
                {
                    const cs::MeshHandle mesh = cs::meshLoad(m_widgets.m_meshBrowser.m_filePath);
                    cs::createGpuBuffers(mesh);
                    cs::setName(mesh, m_widgets.m_meshBrowser.m_fileName);

                    cs::MeshInstance* inst = m_meshInstList.addNew();
                    inst->set(mesh);

                    char name[128];
                    const uint32_t numGroups = cs::meshNumGroups(mesh);
                    for (uint16_t ii = 0; ii < numGroups; ++ii)
                    {
                        // Create plain.
                        const cs::MaterialHandle material = cs::materialCreatePlain();

                        // Set name.
                        bx::snprintf(name, sizeof(name), "%s - %u", m_widgets.m_meshBrowser.m_fileName, ii);
                        cs::setName(material, name);

                        // Add to list.
                        m_materialList.add(material);

                        // Asign it.
                        inst->set(material, ii);
                    }

                    m_settings.m_selectedMeshIdx = m_meshInstList.count()-1;
                    m_modelScene.changeModel(m_meshInstList[m_settings.m_selectedMeshIdx]);
                }
            }
        }

        // ModelLoad thread result.
        if (ThreadStatus::Completed & m_threadParams.m_modelLoad.m_threadStatus)
        {
            char buf[128];

            if (m_threadParams.m_modelLoad.m_threadStatus & ThreadStatus::ExitSuccess)
            {
                // Setup mesh.
                const cs::MeshHandle mesh = m_threadParams.m_modelLoad.m_mesh;
                cs::createGpuBuffers(mesh);
                cs::setName(mesh, m_threadParams.m_modelLoad.m_fileName);

                cs::MeshInstance* inst = m_meshInstList.addNew();
                inst->set(mesh);

                // Create materials.
                const uint32_t numGroups = cs::meshNumGroups(mesh);
                for (uint16_t ii = 0; ii < numGroups; ++ii)
                {
                    // Create plain.
                    const cs::MaterialHandle material = cs::materialCreatePlain();

                    // Assign name.
                    bx::snprintf(buf, sizeof(buf), "%s - %u", m_threadParams.m_modelLoad.m_fileName, ii);
                    cs::setName(material, buf);

                    // Add to list.
                    m_materialList.add(material);

                    // Assign it.
                    inst->set(material, ii);
                }

                m_settings.m_selectedMeshIdx = m_meshInstList.count()-1;
                m_modelScene.changeModel(m_meshInstList[m_settings.m_selectedMeshIdx]);

                bx::snprintf(buf, sizeof(buf), "Mesh '%s' loaded successfully.", m_threadParams.m_modelLoad.m_fileName);
                imguiStatusMessage(buf, 3.0f, false, "Close");
            }
            else
            {
                bx::snprintf(buf, sizeof(buf), "Mesh '%s' loading failed!", m_threadParams.m_modelLoad.m_fileName);
                imguiStatusMessage(buf, 6.0f, true, "Close");
            }

            imguiRemoveStatusMessage(StatusWindowId::MeshConversion);

            // Cleanup.
            if (m_backgroundThread.isRunning())
            {
                m_backgroundThread.shutdown();
            }
            m_threadParams.m_modelLoad.m_threadStatus = ThreadStatus::Idle;
            dm::allocFreeStack(m_threadParams.m_modelLoad.m_stackAlloc);
        }

        // ProjectWindow action.
        if (guiEvent(GuiEvent::HandleAction, m_widgets.m_projectWindow.m_events))
        {
            if (ProjectWindowState::Load == m_widgets.m_projectWindow.m_action)
            {
                if (!m_backgroundThread.isRunning()
                &&  ThreadStatus::Idle == m_threadParams.m_projectLoad.m_threadStatus)
                {
                    // Copy params.
                    dm::strscpya(m_threadParams.m_projectLoad.m_path, m_widgets.m_projectWindow.m_load.m_filePath);
                    dm::strscpya(m_threadParams.m_projectLoad.m_name, m_widgets.m_projectWindow.m_load.m_fileName);

                    // Acquire stack allocator for this thread.
                    m_threadParams.m_projectLoad.m_stackAlloc = dm::allocSplitStack(DM_MEGABYTES(800), DM_MEGABYTES(400));

                    // Start background thread.
                    m_backgroundThread.init(projectLoadFunc, (void*)&m_threadParams.m_projectLoad);
                }
                else
                {
                    const char* msg = "cmftStudio project could not be loaded at this time. Background thread is in use!";
                    imguiStatusMessage(msg, 6.0f, true, "Close");
                }
            }
            else //if (m_widgets.m_projectWindow::Save == m_widgets.m_projectWindow.m_action).
            {
                if (!m_backgroundThread.isRunning()
                &&  ThreadStatus::Idle == m_threadParams.m_projectSave.m_threadStatus)
                {
                    // Copy resource handles and settings parameters.
                    for (uint16_t ii = 0, end = m_materialList.count(); ii < end; ++ii)
                    {
                        m_threadParams.m_projectSave.m_materialList.add(cs::acquire(m_materialList[ii]));
                    }
                    for (uint16_t ii = 0, end = m_envList.count(); ii < end; ++ii)
                    {
                        m_threadParams.m_projectSave.m_envList.add(cs::acquire(m_envList[ii]));
                    }
                    for (uint16_t ii = 0, end = m_meshInstList.count(); ii < end; ++ii)
                    {
                        m_threadParams.m_projectSave.m_meshInstList.add(cs::acquire(m_meshInstList[ii]));
                    }
                    m_threadParams.m_projectSave.m_compressionLevel = int32_t(m_widgets.m_projectWindow.m_compressionLevel);
                    memcpy(&m_threadParams.m_projectSave.m_settings, &m_settings, sizeof(Settings));
                    bx::snprintf(m_threadParams.m_projectSave.m_path
                               , sizeof(m_threadParams.m_projectSave.m_path)
                               , "%s" DM_DIRSLASH "%s.csp"
                               , m_widgets.m_projectWindow.m_save.m_directory
                               , m_widgets.m_projectWindow.m_projectName
                               );
                    dm::strscpya(m_threadParams.m_projectSave.m_name, m_widgets.m_projectWindow.m_save.m_fileName);

                    // Acquire stack allocator for this thread.
                    m_threadParams.m_projectSave.m_stackAlloc = dm::allocSplitStack(DM_MEGABYTES(200), DM_MEGABYTES(400));

                    // Start background thread.
                    m_backgroundThread.init(projectSaveFunc, (void*)&m_threadParams.m_projectSave);
                }
                else
                {
                    const char* msg = "cmftStudio project could not be saved at this time. Background thread is in use!";
                    imguiStatusMessage(msg, 6.0f, true, "Close");
                }
            }
        }

        // ProjectSave thread result.
        if (ThreadStatus::Completed & m_threadParams.m_projectSave.m_threadStatus)
        {
            // Update status message.
            char msg[128];
            if (threadStatus(ThreadStatus::ExitSuccess, m_threadParams.m_projectSave.m_threadStatus))
            {
                bx::snprintf(msg, sizeof(msg), "Project '%s' successfully saved.", m_threadParams.m_projectSave.m_name);
                imguiStatusMessage(msg, 3.0f, false, "Close");
            }
            else
            {
                bx::snprintf(msg, sizeof(msg), "Project '%s' saving failed!", m_threadParams.m_projectSave.m_name);
                imguiStatusMessage(msg, 6.0f, true, "Close");
            }
            imguiRemoveStatusMessage(StatusWindowId::ProjectSave);

            // Cleanup.
            if (m_backgroundThread.isRunning())
            {
                m_backgroundThread.shutdown();
            }
            m_threadParams.m_projectSave.m_threadStatus = ThreadStatus::Idle;
            m_threadParams.m_projectSave.releaseAll();
            dm::allocFreeStack(m_threadParams.m_projectSave.m_stackAlloc);
        }

        // ProjectLoad thread result.
        if (ThreadStatus::Completed & m_threadParams.m_projectLoad.m_threadStatus)
        {
            // Trigger 'ProjectLoaded' event.
            if (threadStatus(ThreadStatus::ExitSuccess, m_threadParams.m_projectLoad.m_threadStatus))
            {
                eventTrigger(Event::ProjectLoaded);
                m_threadParams.m_projectLoad.m_threadStatus = ThreadStatus::Halted;
            }
            else if (threadStatus(ThreadStatus::ExitFailure, m_threadParams.m_projectLoad.m_threadStatus))
            {
                m_threadParams.m_projectLoad.m_threadStatus = ThreadStatus::Idle;
            }

            // Update status message.
            imguiRemoveStatusMessage(StatusWindowId::ProjectLoad);

            // Cleanup.
            if (m_backgroundThread.isRunning())
            {
                m_backgroundThread.shutdown();
            }
        }
    }

    bool tryLoadProject(const char* _name)
    {
        // Load startup project during splash screen.
        char home[DM_PATH_LEN];
        char projectPath[DM_PATH_LEN];
        bool projectLoaded = false;

        // Try loading project from runtime directory.
        if (dm::fileExists(_name))
        {
            projectLoaded = projectLoad(_name
                                      , m_textureList
                                      , m_materialList
                                      , m_envList
                                      , m_meshInstList
                                      , m_settings
                                      );
        }

        // Try loading project from home directory.
        if (!projectLoaded)
        {
            dm::homeDir(home);

            dm::strscpya(projectPath, home);
            bx::strlcat(projectPath, _name, DM_PATH_LEN);

            if (dm::fileExists(projectPath))
            {
                projectLoaded = projectLoad(projectPath
                                          , m_textureList
                                          , m_materialList
                                          , m_envList
                                          , m_meshInstList
                                          , m_settings
                                          );
            }
        }

        // Try loading project from config directory.
        if (!projectLoaded)
        {
            dm::strscpya(projectPath, home);
            bx::strlcat(projectPath, _name, DM_PATH_LEN);

            if (dm::fileExists(projectPath))
            {
                projectLoaded = projectLoad(projectPath
                                          , m_textureList
                                          , m_materialList
                                          , m_envList
                                          , m_meshInstList
                                          , m_settings
                                          );
            }
        }

        // Try loading project from desktop directory.
        if (!projectLoaded)
        {
            dm::desktopDir(projectPath);
            bx::strlcat(projectPath, _name, DM_PATH_LEN);

            if (dm::fileExists(projectPath))
            {
                projectLoaded = projectLoad(projectPath
                                          , m_textureList
                                          , m_materialList
                                          , m_envList
                                          , m_meshInstList
                                          , m_settings
                                          );
            }
        }

        // Try loading project from downloads directory.
        if (!projectLoaded)
        {
            dm::strscpya(projectPath, home);
            bx::strlcat(projectPath, _name, DM_PATH_LEN);

            if (dm::fileExists(projectPath))
            {
                projectLoaded = projectLoad(projectPath
                                          , m_textureList
                                          , m_materialList
                                          , m_envList
                                          , m_meshInstList
                                          , m_settings
                                          );
            }
        }

        return projectLoaded;
    }

public:
    void run(int _argc, const char* const* _argv)
    {
        // Action for --help.
        bx::CommandLine cmdLine(_argc, _argv);
        if (cmdLine.hasArg('h', "help"))
        {
            printCliHelp();
            return;
        }

        configFromDefaultPaths(g_config);

        // Setup allocator.
        dm::allocInit();
        cmft::setAllocator(dm::mainAlloc);

        const double splashScreenDuration = 1.5;
        const float modalWindowAnimDuration = 0.06f;
        float posUd    = 0.20f;
        float scaleUd  = 0.20f;
        float rotUd    = 0.00f;
        float valueUd  = 0.20f;
        float widgetUd = 0.15f;
        float fovUd    = 0.10f;
        float envUd    = 0.50f;

        initStaticResources();
        m_threadParams.init();

        // Get parameters from cli.
        configFromCli(g_config, _argc, _argv);

        // Init bgfx.
        bgfx::init(g_config.m_renderer, BGFX_PCI_ID_NONE, 0, NULL, cs::bgfxAlloc);

        uint32_t reset = BGFX_RESET_VSYNC;
        bgfx::reset(g_config.m_width, g_config.m_height, reset);

        uint32_t debug = BGFX_DEBUG_NONE;
        bgfx::setDebug(debug);

        // Setup window.
        entry::WindowHandle window = { 0 };
        entry::setWindowSize(window, g_config.m_width, g_config.m_height);
        entry::setWindowTitle(window, "cmftStudio");

        // Initialize key bindings.
        inputAddBindings("cmftStudio", s_keyBindings);

        // Setup renderer dependent parameters.
        const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
        g_texelHalf = bgfx::RendererType::Direct3D9 == renderer ? 0.5f : 0.0f;
        g_originBottomLeft = false
                          || bgfx::RendererType::OpenGL   == renderer
                          || bgfx::RendererType::OpenGLES == renderer
                           ;

        // Initialization that happens before splash screen.
        cs::initContext();
        cs::initUniforms();
        cs::initPrograms();

        // OpenGL prefers this initialization before submitting splash screen.
        if (g_originBottomLeft)
        {
            renderPipelineInit(g_config.m_width, g_config.m_height, reset);
        }

        // Draw splash screen.
        initVertexDecls();
        const cs::TextureHandle splashTex = cs::textureLoadRaw(g_loadingScreenTex, g_loadingScreenTexSize);
        drawSplashScreen(splashTex, g_config.m_width, g_config.m_height);
        bx::sleep(100);

        // DirectX preferes this initialization after submitting splash screen.
        if (!g_originBottomLeft)
        {
            renderPipelineInit(g_config.m_width, g_config.m_height, reset);
        }

        // Start splash timer.
        int64_t timeSplash = timerCurrentTick();
        stateEnter(State::SplashScreen);

        // Init resource lists.
        initLists();

        const bool projectLoaded = tryLoadProject(g_config.m_startupProject);

        // If startup project is not available, load predefined resources.
        if (!projectLoaded)
        {
            Assets::loadResources();
            Assets::getAll(m_textureList);
            Assets::getAll(m_materialList);
            Assets::getAll(m_envList);
            Assets::getAll(m_meshInstList);
            Assets::releaseResources();
        }

        for (uint16_t ii = 0, end = m_textureList.count(); ii < end; ++ii)
        {
            const cs::TextureHandle handle = m_textureList[ii];
            cs::createGpuBuffers(handle); // this has to be executed here on the main thread.
        }
        for (uint16_t ii = 0, end = m_meshInstList.count(); ii < end; ++ii)
        {
            cs::createGpuBuffers(m_meshInstList[ii].m_mesh); // this has to be executed here on the main thread.
        }
        for (uint16_t ii = 0, end = m_envList.count(); ii < end; ++ii)
        {
            const cs::EnvHandle handle = m_envList[ii];
            cs::createGpuBuffers(handle); // this has to be executed here on the main thread.
        }

        // Make sure there is at least one material.
        if (m_materialList.count() == 0)
        {
            cs::MaterialHandle plain = cs::materialCreateShiny();
            cs::setName(plain, "Plain");
            m_materialList.add(plain);

            m_materialList.add(cs::materialCreateStripes());
            m_materialList.add(cs::materialCreateBricks());
        }

        // Make sure there is at least one environment.
        if (m_envList.count() == 0)
        {
            m_envList.add(cs::envCreateCmftStudioLogo());
        }

        // Make sure there is at least one mesh instance.
        if (m_meshInstList.count() == 0)
        {
            cs::MeshInstance* inst = m_meshInstList.addNew();
            inst->set(cs::meshSphere());
            inst->set(m_materialList[0]);
            inst->m_scaleAdj = 0.8f;
        }

        // Scenes.
        m_environment.init(m_envList[m_settings.m_selectedEnvMap], cs::Environment::Skybox);
        m_modelScene.init(m_meshInstList[m_settings.m_selectedMeshIdx]);
        m_spheresScene.init(m_settings.m_spheres);

        // Use this values at the beginning for a nice intro animation.
        m_settings.m_worldRot[0] = m_settings.m_worldRotDest[0] - 0.2f;
        m_settings.m_worldRot[1] = m_settings.m_worldRotDest[1];
        m_settings.m_worldRot[2] = m_settings.m_worldRotDest[2];
        m_settings.m_fov = 35.0f;

        // Gui.
        guiInit();
        outputWindowInit(&m_widgets.m_outputWindow);

        // Initialization is done, wait for splash screen to finish.
        for (;;)
        {
            const int64_t timeNow = timerCurrentTick();
            const int64_t delta = timeNow - timeSplash;
            if (timerToSec(delta) > splashScreenDuration)
            {
                break;
            }
            else
            {
                bx::sleep(100);
            }
        }

        // Splash screen texture is not needed any more.
        cs::release(splashTex);

        // Transition to intro state.
        stateEnter(State::IntroAnimation);
        m_modelScene.animate();

        char asciiKey = 0;
        bool mouseOverGui = false;

        // Main loop.
        uint32_t width  = g_config.m_width;
        uint32_t height = g_config.m_height;
        while (!entry::processEvents(width, height, debug, reset, &m_mouseState) )
        {
            // When not minimized.
            if (0 != width && 0 != height)
            {
                // Update size.
                g_width  = width;
                g_widthf = dm::utof(g_width);

                g_height  = height;
                g_heightf = dm::utof(g_height);
            }

            if (m_settings.m_msaa)
            {
                reset |= BGFX_RESET_MSAA_X16;
            }
            else
            {
                reset &= ~BGFX_RESET_MSAA_X16;
            }

            // React on size change and/or reset.
            renderPipelineUpdateSize(g_width, g_height, reset);

            // Handle events:
            eventFrame();

            // On project load start.
            if (eventHandle(Event::ProjectIsLoading))
            {
                // Free current resources when project loading starts.
                for (uint16_t ii = m_textureList.count();  ii--; ) { freeHostMem(m_textureList[ii]);         }
                for (uint16_t ii = m_envList.count();      ii--; ) { freeHostMem(m_envList[ii]);             }
                for (uint16_t ii = m_meshInstList.count(); ii--; ) { freeHostMem(m_meshInstList[ii].m_mesh); }
            }

            // On project load complete.
            if (eventHandle(Event::ProjectLoaded))
            {
                m_projTransition.m_currEnv  = cs::acquire(m_envList[m_settings.m_selectedEnvMap]);
                m_projTransition.m_currInst = cs::acquire(&m_meshInstList[m_settings.m_selectedMeshIdx]);

                for (uint16_t ii = m_materialList.count(); ii--; ) { cs::release(m_materialList[ii]);  }
                for (uint16_t ii = m_textureList.count();  ii--; ) { cs::release(m_textureList[ii]);   }
                for (uint16_t ii = m_envList.count();      ii--; ) { cs::release(m_envList[ii]);       }
                for (uint16_t ii = m_meshInstList.count(); ii--; ) { cs::release(&m_meshInstList[ii]); }

                stateEnter(State::SendResourcesToGpu);
            }

            if (eventHandle(Event::BeginLoadTransition))
            {
                // Release resources and clear lists.
                cs::release(m_projTransition.m_currEnv);
                cs::release(m_projTransition.m_currInst);

                m_textureList.reset();
                m_materialList.reset();
                m_envList.reset();
                m_meshInstList.reset();

                // Copy lists from projectLoad.
                for (uint16_t ii = 0, end = m_threadParams.m_projectLoad.m_textureList.count(); ii < end; ++ii)
                {
                    const cs::TextureHandle handle = m_threadParams.m_projectLoad.m_textureList[ii];
                    m_textureList.add(handle);
                }
                for (uint16_t ii = 0, end = m_threadParams.m_projectLoad.m_materialList.count(); ii < end; ++ii)
                {
                    m_materialList.add(m_threadParams.m_projectLoad.m_materialList[ii]);
                }
                for (uint16_t ii = 0, end = m_threadParams.m_projectLoad.m_meshInstList.count(); ii < end; ++ii)
                {
                    const cs::MeshInstance& inst = m_threadParams.m_projectLoad.m_meshInstList[ii];
                    cs::MeshInstance* cpy = m_meshInstList.addNew();
                    cpy = new (cpy) cs::MeshInstance(inst);
                }
                for (uint16_t ii = 0, end = m_threadParams.m_projectLoad.m_envList.count(); ii < end; ++ii)
                {
                    const cs::EnvHandle handle = m_threadParams.m_projectLoad.m_envList[ii];
                    m_envList.add(handle);
                }

                // Apply settings.
                m_settings.apply(m_threadParams.m_projectLoad.m_settings);
                m_modelScene.setModel(m_meshInstList[m_settings.m_selectedMeshIdx]);
                m_spheresScene.wrapRotation();

                // Assign every invalid material to the first one in list.
                const cs::MaterialHandle firstMat = m_materialList[0];
                for (uint16_t ii = 0; ii < m_meshInstList.count(); ++ii)
                {
                    for (uint32_t group = 0, count = meshNumGroups(m_meshInstList[ii].m_mesh); group < count; ++group)
                    {
                        if (!cs::isValid(m_meshInstList[ii].m_materials[group]))
                        {
                            m_meshInstList[ii].set(firstMat, group);
                        }
                    }
                }

                // Cleanup.
                m_threadParams.m_projectLoad.reset();
                m_threadParams.m_projectLoad.m_threadStatus = ThreadStatus::Idle;
                dm::allocFreeStack(m_threadParams.m_projectLoad.m_stackAlloc);

                stateEnter(State::ProjectLoadTransition);
            }

            // Handle state changes:

            if (onState(State::IntroAnimation))
            {
                // Enter MainState when animation is done.
                const bool animationEnded = !m_modelScene.isAnimating();
                if (animationEnded)
                {
                    stateEnter(State::MainState);
                }
            }

            if (onState(State::SendResourcesToGpu))
            {
                if (onStateEnter(State::SendResourcesToGpu))
                {
                    m_projTransition.m_texIdx = m_threadParams.m_projectLoad.m_textureList.count();
                    m_projTransition.m_mshIdx = m_threadParams.m_projectLoad.m_meshInstList.count();
                    m_projTransition.m_envIdx = m_threadParams.m_projectLoad.m_envList.count();
                }

                // Create gpu buffers, one pre frame.

                if (m_projTransition.m_texIdx)
                {
                    m_projTransition.m_texIdx--;
                    const cs::TextureHandle tex = m_threadParams.m_projectLoad.m_textureList[m_projTransition.m_texIdx];
                    cs::createGpuBuffers(tex);
                }
                else if (m_projTransition.m_mshIdx)
                {
                    m_projTransition.m_mshIdx--;
                    const cs::MeshInstance& inst = m_threadParams.m_projectLoad.m_meshInstList[m_projTransition.m_mshIdx];
                    cs::createGpuBuffers(inst.m_mesh);
                }
                else if (m_projTransition.m_envIdx)
                {
                    m_projTransition.m_envIdx--;
                    const cs::EnvHandle env = m_threadParams.m_projectLoad.m_envList[m_projTransition.m_envIdx];
                    cs::createGpuBuffers(env);
                }
                else
                {
                    eventTrigger(Event::BeginLoadTransition);
                }
            }

            if (onState(State::ProjectLoadTransition))
            {
                if (onStateEnter(State::ProjectLoadTransition))
                {
                    // Set update values.
                    valueUd = 0.2f;
                    posUd   = 0.2f;
                    scaleUd = 0.2f;
                    fovUd   = 0.0f;
                    rotUd   = 0.0f;
                    envUd   = 1.4f;

                    // Setup animation.
                    m_settings.m_fov = 45.0f; // for a nice transition.
                    m_settings.m_worldRot[0] = m_settings.m_worldRotDest[0] - 0.15f;
                    m_settings.m_worldRot[1] = m_settings.m_worldRotDest[1];
                    m_settings.m_worldRot[2] = m_settings.m_worldRotDest[2];

                    // Switch to the first tab.
                    m_widgets.m_leftScrollArea.m_selectedTab = 0;

                    m_modelScene.animate();

                    // Display notification.
                    char msg[128];
                    bx::snprintf(msg, sizeof(msg), "Project '%s' successfully loaded.", m_threadParams.m_projectLoad.m_name);
                    imguiStatusMessage(msg, 3.0f, false, "Close");
                }

                // Enter MainState when animation is done.
                const bool animationEnded = !m_modelScene.isAnimating();
                if (animationEnded)
                {
                    stateEnter(State::MainState);
                }
            }

            // Adjust update durations for the main state.
            if (onStateEnter(State::MainState))
            {
                widgetUd = 0.1f;
                valueUd  = 0.1f;
                posUd    = 0.1f;
                rotUd    = 0.1f;
                fovUd    = 0.1f;
                scaleUd  = 0.1f;
                envUd    = 0.5f;
            }

            // All states handled.
            stateFrame();

            // Update time.
            timerUpdate();
            const float deltaTime = dm::toBool(reset&BGFX_RESET_VSYNC)
                                  ? 1.0f/60.0f
                                  : dm::min(1.0f/60.0f, (float)timerDeltaSec())
                                  ;
            cs::Uniforms& uniforms = cs::getUniforms();
            uniforms.m_time = deltaTime;

            // Update keyboard input.
            const uint8_t* utf8 = inputGetChar();
            asciiKey = (NULL != utf8) ? utf8[0] : 0;

            // Update mouse input.
            m_mouseClickState.update(m_mouseState, g_width, g_height);

            // Update settings.
            updateUniforms(m_settings);

            // Draw gui and collect input from gui.
            mouseOverGui = guiDraw(m_imguiState
                                 , m_settings
                                 , m_widgets
                                 , m_mouseClickState
                                 , asciiKey
                                 , deltaTime
                                 , m_meshInstList
                                 , m_textureList
                                 , m_materialList
                                 , m_envList
                                 , widgetUd
                                 , modalWindowAnimDuration
                                 );

            m_controlState.update(m_mouseClickState, mouseOverGui);

            // Setup camera.
            handleWorldRotation(m_settings.m_worldRotDest, m_mouseClickState, m_controlState);
            m_settings.m_fov         = lerp(m_settings.m_fov,         m_settings.m_fovDest,         deltaTime, valueUd);
            m_settings.m_worldRot[0] = lerp(m_settings.m_worldRot[0], m_settings.m_worldRotDest[0], deltaTime, valueUd);
            m_settings.m_worldRot[1] = lerp(m_settings.m_worldRot[1], m_settings.m_worldRotDest[1], deltaTime, valueUd);

            Camera camDest;
            camDest.setup(m_settings.m_worldRotDest[0], m_settings.m_worldRotDest[1], m_settings.m_fov, g_widthf, g_heightf);

            Camera cam;
            cam.setup(m_settings.m_worldRot[0], m_settings.m_worldRot[1], m_settings.m_fov, g_widthf, g_heightf);
            cam.setUniforms();

            renderPipelineSetActiveCamera(cam.m_view, cam.m_proj);

            // Update and submit environment.
            const cs::EnvHandle currentEnv = m_envList[m_settings.m_selectedEnvMap];
            const cs::Environment::Enum envType = (cs::Environment::Enum)m_settings.m_backgroundType;
            m_environment.update(currentEnv, envType, m_settings.m_backgroundMipLevel, envUd);

            if (m_environment.m_skybox.m_doTransition)
            {

                renderPipelineSubmitSkybox(m_environment.m_skybox.m_nextEnv
                                         , m_environment.m_skybox.m_currEnv
                                         , m_environment.m_skybox.m_progress
                                         , m_environment.m_skybox.m_nextWhich
                                         , m_environment.m_skybox.m_currWhich
                                         , m_environment.m_skybox.m_nextLod
                                         , m_environment.m_skybox.m_currLod
                                         );
            }
            else
            {
                renderPipelineSubmitSkybox(m_environment.m_skybox.m_currEnv
                                         , m_environment.m_skybox.m_currWhich
                                         , m_environment.m_skybox.m_currLod
                                         );
            }

            // Handle gui events that apply to current screne.
            if (guiEvent(GuiEvent::HandleAction, m_widgets.m_leftScrollArea.m_events))
            {
                switch (m_widgets.m_leftScrollArea.m_action)
                {
                case LeftScrollAreaState::ResetMeshPosition:       { m_modelScene.resetPosition();        } break;
                case LeftScrollAreaState::ResetMeshScale:          { m_modelScene.resetScale();           } break;
                case LeftScrollAreaState::SpheresControlChanged:   { m_spheresScene.onControlChange(cam); } break;
                case LeftScrollAreaState::ResetSpheresPosition:    { m_spheresScene.resetPosition();      } break;
                case LeftScrollAreaState::ResetSpheresScale:       { m_spheresScene.resetScale();         } break;
                case LeftScrollAreaState::ResetSpheresInclination: { m_spheresScene.resetInclination();   } break;
                case LeftScrollAreaState::UpdateMaterialPreview:   { m_matPreview.makeActive();           } break;

                case LeftScrollAreaState::MeshSelectionChange:
                    {
                        m_modelScene.changeModel(m_meshInstList[m_settings.m_selectedMeshIdx]);
                    }
                break;

                case LeftScrollAreaState::TabSelectionChange:
                    {
                        // From spheres to model/material tab.
                        if (LeftScrollAreaState::SpheresTab == m_widgets.m_leftScrollArea.m_prevSelectedTab)
                        {
                            m_spheresScene.finishAnimation();
                            m_modelScene.animate();
                        }

                        // From other to spheres tab.
                        if (LeftScrollAreaState::SpheresTab == m_widgets.m_leftScrollArea.m_selectedTab)
                        {
                            m_modelScene.finishAnimation();
                            m_spheresScene.animate(cam);
                        }

                        // From other to material tab.
                        if (1 == m_widgets.m_leftScrollArea.m_selectedTab)
                        {
                            m_matPreview.spin();
                        }
                    }
                break;

                default:
                    {
                        m_widgets.m_leftScrollArea.m_events ^= GuiEvent::HandleAction;
                    }
                break;
                };
            }

            // Update and submit current scene.
            if (LeftScrollAreaState::SpheresTab == m_widgets.m_leftScrollArea.m_selectedTab)
            {
                m_spheresScene.update(m_mouseClickState, m_controlState, cam, camDest, deltaTime, valueUd);
                m_spheresScene.draw(RenderPipeline::ViewIdMesh, cs::Program::MeshRgbe8, m_environment);
            }
            else /*if (LeftScrollAreaState::ModelTab    == m_widgets.m_leftScrollArea.m_selectedTab
                   ||  LeftScrollAreaState::MaterialTab == m_widgets.m_leftScrollArea.m_selectedTab).*/
            {
                m_modelScene.update(m_mouseClickState, m_controlState, camDest, deltaTime, posUd, scaleUd, rotUd, fovUd);
                m_modelScene.draw(RenderPipeline::ViewIdMesh, cs::Program::MeshRgbe8, m_environment);
            }

            // Update and submit material view.
            if (LeftScrollAreaState::MaterialTab == m_widgets.m_leftScrollArea.m_selectedTab)
            {
                const cs::MaterialHandle activeMaterial = m_modelScene.getCurrInstance().getActiveMaterial();
                const cs::EnvHandle activeEnv = m_envList[m_settings.m_selectedEnvMap];
                m_matPreview.update(m_mouseClickState, deltaTime, 0.1f);
                m_matPreview.draw(activeMaterial, activeEnv);
            }

            // Setup frame.
            renderPipelineSetupFrame(m_settings.m_doLightAdapt, m_settings.m_doBloom, m_settings.m_fxaa);

            // Submit frame.
            renderPipelineSubmitFrame(m_settings.m_gaussStdDev);

            // Render.
            renderPipelineFlush();

            // Handle gui response that will take effect in the next frame.
            guiActionHandler();

            // Run resource garbage collector.
            cs::resourceGC(1);

            // Release freed memory.
            cs::allocGc();
        }

        // Cleanup.
        destroyLists();
        m_threadParams.destroy();

        guiDestroy();
        renderPipelineCleanup();

        cs::destroyPrograms();
        cs::destroyUniforms();
        cs::destroyMeshes();
        cs::destroyEnvironments();
        cs::destroyTextures();
        cs::destroyContext();

        bgfx::shutdown();

        dm::allocPrintStats();
        cs::allocDestroy();
    }

private:
    void initLists()
    {
        const uint32_t totalSize = m_textureList.sizeFor(CS_MAX_TEXTURES)
                                 + m_materialList.sizeFor(CS_MAX_MATERIALS)
                                 + m_envList.sizeFor(CS_MAX_ENVIRONMENTS)
                                 + m_meshInstList.sizeFor(CS_MAX_MESHINSTANCES)
                                 ;
        m_listMem = BX_ALLOC(dm::staticAlloc, totalSize);

        void* mem = m_listMem;
        mem = m_textureList.init(CS_MAX_TEXTURES,       mem, dm::staticAlloc);
        mem = m_materialList.init(CS_MAX_MATERIALS,     mem, dm::staticAlloc);
        mem = m_envList.init(CS_MAX_ENVIRONMENTS,       mem, dm::staticAlloc);
        mem = m_meshInstList.init(CS_MAX_MESHINSTANCES, mem, dm::staticAlloc);
    }

    void destroyLists()
    {
        m_textureList.destroy();
        m_materialList.destroy();
        m_envList.destroy();
        m_meshInstList.destroy();

        BX_FREE(dm::staticAlloc, m_listMem);
    }

    // Resources.
    void*                m_listMem;
    cs::TextureList      m_textureList;
    cs::MaterialList     m_materialList;
    cs::EnvList          m_envList;
    cs::MeshInstanceList m_meshInstList;

    // Input.
    entry::MouseState m_mouseState;
    Mouse m_mouseClickState;

    // Control state.
    ControlState m_controlState;

    // Settings/gui state.
    Settings m_settings;
    ImguiState m_imguiState;
    GuiWidgetState m_widgets;

    // Scenes.
    Environment m_environment;
    MaterialPreview m_matPreview;
    ModelScene m_modelScene;
    SpheresScene m_spheresScene;

    struct ProjectTransition
    {
        cs::EnvHandle     m_currEnv;
        cs::MeshInstance* m_currInst;

        uint16_t m_texIdx;
        uint16_t m_mshIdx;
        uint16_t m_envIdx;
    };
    ProjectTransition m_projTransition;

    // Background thread.
    struct ThreadParams
    {
        void init()
        {
            m_projectSave.init();
            m_projectLoad.init();
        }

        void destroy()
        {
            m_projectSave.destroy();
            m_projectLoad.destroy();
        }

        ProjectSaveThreadParams m_projectSave;
        ProjectLoadThreadParams m_projectLoad;
        CmftFilterThreadParams  m_cmftFilter;
        ModelLoadThreadParams   m_modelLoad;
    };
    ThreadParams m_threadParams;
    bx::Thread m_backgroundThread;
};
static CmftStudioApp s_cmftStudio;

int _main_(int _argc, char** _argv)
{
    s_cmftStudio.run(_argc, _argv);

    return 0;
}

/* vim: set sw=4 ts=4 expandtab: */
