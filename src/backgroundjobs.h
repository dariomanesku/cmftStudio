/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_BACKGROUNDJOBS_H_HEADER_GUARD
#define CMFTSTUDIO_BACKGROUNDJOBS_H_HEADER_GUARD

#include "common/common.h"
#include <stdint.h>

#include "guimanager.h" // imguiEnqueueStatusMessage()
#include "context.h"    // cs::*List, cs::MeshHandle
#include "settings.h"   // Settings
#include <dm/misc.h>    // DM_PATH_LEN

struct ThreadStatus
{
    enum Enum
    {
        Idle        = 0x00,
        Started     = 0x01,
        Completed   = 0x02,
        Halted      = 0x04,

        ExitSuccess = 0x10,
        ExitFailure = 0x20,
    };
};

static inline bool threadStatus(ThreadStatus::Enum _state, uint8_t& _states)
{
    if (_states & _state)
    {
        _states ^= _state;
        return true;
    }

    return false;
}

// Project save.
//-----

struct ProjectSaveThreadParams
{
    void init()
    {
        m_stackAlloc       = NULL;
        m_compressionLevel = 6;
        m_threadStatus     = ThreadStatus::Idle;
        m_path[0]          = '\0';
        m_name[0]          = '\0';

        const uint32_t size = cs::MaterialList::sizeFor(CS_MAX_MATERIALS)
                            + cs::EnvList::sizeFor(CS_MAX_ENVIRONMENTS)
                            + cs::MeshInstanceList::sizeFor(CS_MAX_MESHINSTANCES);
        m_memBlock = DM_ALLOC(dm::staticAlloc, size);

        void* ptr = m_memBlock;
        ptr = m_materialList.init(CS_MAX_MATERIALS,     ptr, dm::staticAlloc);
        ptr = m_envList.init(CS_MAX_ENVIRONMENTS,       ptr, dm::staticAlloc);
        ptr = m_meshInstList.init(CS_MAX_MESHINSTANCES, ptr, dm::staticAlloc);
    }

    void releaseAll()
    {
        listRemoveReleaseAll(m_materialList);
        listRemoveReleaseAll(m_envList);
        listRemoveReleaseAll(m_meshInstList);
    }

    void destroy()
    {
        m_materialList.destroy();
        m_envList.destroy();
        m_meshInstList.destroy();
        DM_FREE(dm::staticAlloc, m_memBlock);
    }

    Settings m_settings;
    dm::StackAllocatorI* m_stackAlloc;
    int32_t m_compressionLevel;
    uint8_t m_threadStatus;
    char m_path[DM_PATH_LEN];
    char m_name[128];
    void* m_memBlock;
    cs::MaterialList m_materialList;
    cs::EnvList m_envList;
    cs::MeshInstanceList m_meshInstList;
};

int32_t projectSaveFunc(void* _projectSaveThreadParams);

// Project load.
//-----

struct ProjectLoadThreadParams
{
    void init()
    {
        m_stackAlloc   = NULL;
        m_threadStatus = ThreadStatus::Idle;
        m_path[0]      = '\0';
        m_name[0]      = '\0';

        const uint32_t size = cs::TextureList::sizeFor(CS_MAX_TEXTURES)
                            + cs::MaterialList::sizeFor(CS_MAX_MATERIALS)
                            + cs::EnvList::sizeFor(CS_MAX_ENVIRONMENTS)
                            + cs::MeshInstanceList::sizeFor(CS_MAX_MESHINSTANCES);
        m_memBlock = DM_ALLOC(dm::staticAlloc, size);

        void* ptr = m_memBlock;
        ptr = m_textureList.init(CS_MAX_TEXTURES,       ptr, dm::staticAlloc);
        ptr = m_materialList.init(CS_MAX_MATERIALS,     ptr, dm::staticAlloc);
        ptr = m_envList.init(CS_MAX_ENVIRONMENTS,       ptr, dm::staticAlloc);
        ptr = m_meshInstList.init(CS_MAX_MESHINSTANCES, ptr, dm::staticAlloc);
    }

    void reset()
    {
        m_textureList.reset();
        m_materialList.reset();
        m_envList.reset();
        m_meshInstList.removeAll();
    }

    void destroy()
    {
        m_textureList.destroy();
        m_materialList.destroy();
        m_envList.destroy();
        m_meshInstList.destroy();
        DM_FREE(dm::staticAlloc, m_memBlock);
    }

    Settings m_settings;
    dm::StackAllocatorI* m_stackAlloc;
    uint8_t m_threadStatus;
    char m_path[DM_PATH_LEN];
    char m_name[128];
    void* m_memBlock;
    cs::TextureList m_textureList;
    cs::MaterialList m_materialList;
    cs::EnvList m_envList;
    cs::MeshInstanceList m_meshInstList;
};

int32_t projectLoadFunc(void* _projectLoadThreadParams);

// Mesh format conversion.
//-----

struct ModelLoadThreadParams
{
    ModelLoadThreadParams()
    {
        m_mesh         = cs::MeshHandle::invalid();
        m_stackAlloc   = NULL;
        m_threadStatus = ThreadStatus::Idle;
        m_filePath[0]  = '\0';
        m_fileName[0]  = '\0';
    }

    cs::MeshHandle m_mesh;
    dm::StackAllocatorI* m_stackAlloc;
    uint8_t m_threadStatus;
    char m_filePath[DM_PATH_LEN];
    char m_fileName[128];
    uint8_t m_userData[sizeof(int32_t)*64];
};

int32_t modelLoadFunc(void* _modelLoadThreadParameters);

// Cmft filter.
//-----

namespace cmft { struct ImageSoftRef; }

struct CmftFilterThreadParams
{
    CmftFilterThreadParams()
    {
        m_threadStatus  = ThreadStatus::Idle;
        m_srcSize       = 256;
        m_dstSize       = 256;
        m_inputGamma    = 2.2f;
        m_outputGamma   = 1.0f/2.2f;
        m_mipCount      = 7;
        m_glossScale    = 10;
        m_glossBias     = 3;
        m_numCpuThreads = 4;
        m_filterType    = cs::Environment::Pmrem;
        m_lightingModel = cmft::LightingModel::BlinnBrdf;
        m_edgeFixup     = cmft::EdgeFixup::None;
        m_excludeBase   = false;
        m_useOpenCL     = true;
        m_envHandle     = cs::EnvHandle::invalid();
    }

    uint8_t m_threadStatus;
    uint32_t m_srcSize;
    uint32_t m_dstSize;
    float m_inputGamma;
    float m_outputGamma;
    uint8_t m_mipCount;
    uint8_t m_glossScale;
    uint8_t m_glossBias;
    uint8_t m_numCpuThreads;
    cs::Environment::Enum m_filterType;
    cmft::LightingModel::Enum m_lightingModel;
    cmft::EdgeFixup::Enum m_edgeFixup;
    bool m_excludeBase;
    bool m_useOpenCL;
    cmft::ImageSoftRef m_output;
    cmft::ImageSoftRef m_input;
    cs::EnvHandle m_envHandle;
};

int32_t cmftFilterFunc(void* _cmftFilterThreadParams);

#endif // CMFTSTUDIO_BACKGROUNDJOBS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
