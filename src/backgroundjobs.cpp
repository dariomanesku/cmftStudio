/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "backgroundjobs.h"

#include "guimanager.h" //outputWindow*()
#include "project.h"    //projectLoad()/projectSave()
#include "eventstate.h" //eventTrigger()
#include "context.h"    //meshLoad()

#include "common/cmft.h"

void onProjectSaveValidFile(uint32_t /*_flags*/, const void* /*_data*/)
{
    // Set status message.
    const char* msg = "[ Compressing and saving project file... ]";
    imguiStatusMessage(msg, 0.0f, false, NULL, 0, StatusWindowId::ProjectSave);

    // Show output window.
    outputWindowClear();
    outputWindowShow();

    // Wait for outputWindow animation to finish smoothly.
    bx::sleep(300);
}

void onProjectSaveInvalidFile(uint32_t /*_flags*/, const void* _data)
{
    // Show error message.
    const char* msg = (const char*)_data;
    imguiStatusMessage(msg, 6.0f, true, "Close");
}

int32_t projectSaveFunc(void* _projectSaveThreadParams)
{
    ProjectSaveThreadParams* params = (ProjectSaveThreadParams*)_projectSaveThreadParams;

    // Start.
    params->m_threadStatus = ThreadStatus::Started;

    // Save project.
    const bool success = projectSave(params->m_path
                                   , params->m_materialList
                                   , params->m_envList
                                   , params->m_meshInstList
                                   , params->m_settings
                                   , params->m_compressionLevel
                                   , &onProjectSaveValidFile
                                   , &onProjectSaveInvalidFile
                                   );

    // Result.
    if (success)
    {
        params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitSuccess;
        return EXIT_SUCCESS;
    }
    else
    {
        params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitFailure;
        return EXIT_FAILURE;
    }
}

void onProjectLoadValidFile(uint32_t /*_flags*/, const void* /*_data*/)
{
    // Set status message.
    const char* msg = "[ Uncompressing and loading project file... ]";
    imguiStatusMessage(msg, 0.0f, false, NULL, 0, StatusWindowId::ProjectLoad);

    // Show output window.
    outputWindowClear();
    outputWindowShow();

    // Announce main thread that project loading has successfully started.
    eventTrigger(Event::ProjectIsLoading);

    // Wait for outputWindow animation to finish smoothly.
    bx::sleep(300);
}

void onProjectLoadInvalidFile(uint32_t /*_flags*/, const void* _data)
{
    // Show error message.
    const char* msg = (const char*)_data;
    imguiStatusMessage(msg, 6.0f, true, NULL, 0, StatusWindowId::LoadError);
}

int32_t projectLoadFunc(void* _projectLoadThreadParams)
{
    ProjectLoadThreadParams* params = (ProjectLoadThreadParams*)_projectLoadThreadParams;

    // Start.
    params->m_threadStatus = ThreadStatus::Started;

    // Load project.
    const bool success = projectLoad(params->m_path
                                   , params->m_textureList
                                   , params->m_materialList
                                   , params->m_envList
                                   , params->m_meshInstList
                                   , params->m_settings
                                   , &onProjectLoadValidFile
                                   , &onProjectLoadInvalidFile
                                   , params->m_stackAlloc
                                   );

    // Result.
    if (success)
    {
        params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitSuccess;
        return EXIT_SUCCESS;
    }
    else
    {
        params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitFailure;
        return EXIT_FAILURE;
    }
}

int32_t modelLoadFunc(void* _modelLoadThreadParameters)
{
    ModelLoadThreadParams* params = (ModelLoadThreadParams*)_modelLoadThreadParameters;

    // Start.
    params->m_threadStatus = ThreadStatus::Started;

    // Show output window.
    outputWindowShow();
    outputWindowClear();
    outputWindowPrint("Loading %s", params->m_filePath);

    // Set status message.
    const char* msg = "[ Mesh is loading in background... ]";
    imguiStatusMessage(msg, 0.0f, false, NULL, 0, StatusWindowId::MeshConversion);

    // Allow for animation to finish smoothly.
    bx::sleep(300);

    // Load mesh.
    params->m_mesh = cs::meshLoad(params->m_filePath, (void*)params->m_userData, params->m_stackAlloc);

    if (cs::isValid(params->m_mesh))
    {
        params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitSuccess;
        return EXIT_SUCCESS;
    }
    else
    {
        params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitFailure;
        return EXIT_FAILURE;
    }
}

int32_t cmftFilterFunc(void* _cmftFilterThreadParams)
{
    CmftFilterThreadParams* params = (CmftFilterThreadParams*)_cmftFilterThreadParams;

    // Start.
    params->m_threadStatus = ThreadStatus::Started;

    // Set status message.
    if (cs::Environment::Pmrem == params->m_filterType)
    {
        const char* msg = "[ Cmft radiance filter running in background... ]";
        imguiStatusMessage(msg, 0.0f, false, NULL, 0, StatusWindowId::FilterPmrem);
    }
    else //if (cs::Environment::Iem == params->m_filterType).
    {
        const char* msg = "[ Cmft irradiance filter running in background... ]";
        imguiStatusMessage(msg, 0.0f, false, NULL, 0, StatusWindowId::FilterIem);
    }

    // Show output window.
    outputWindowClear();
    outputWindowPrint("Preparing data and starting cmft...");
    outputWindowShow();

    // Allow for animation to finish smoothly.
    bx::sleep(300);

    // Processing is done on rgba32f.
    cmft::imageConvert(params->m_output, cmft::TextureFormat::RGBA32F, params->m_input);

    if (cs::Environment::Pmrem == params->m_filterType)
    {
        // Init OpenCL context.
        cmft::ClContext clContext;

        int32_t clLoaded = 0;
        if (params->m_useOpenCL)
        {
            clLoaded = bx::clLoad(); // Dynamically load OpenCL lib.

            if (clLoaded)
            {
                clContext.init(CMFT_CL_VENDOR_ANY_GPU, CMFT_CL_DEVICE_TYPE_GPU, 0);
            }
        }

        // Resize.
        if (params->m_srcSize != params->m_input.m_width)
        {
            cmft::imageResize(params->m_output, params->m_srcSize, params->m_srcSize);
        }

        // Input gamma.
        cmft::imageApplyGamma(params->m_output, params->m_inputGamma);

        // Notice: Nvidia crashes the driver if memory is not from crt allcator.
        // Don't know what seems to be the reason. Could be a driver issue.
        cmft::setAllocator(dm::crtAlloc);

        // Radiance filter.
        const bool success = cmft::imageRadianceFilter(params->m_output
                                                     , params->m_dstSize
                                                     , params->m_lightingModel
                                                     , params->m_excludeBase
                                                     , params->m_mipCount
                                                     , params->m_glossScale
                                                     , params->m_glossBias
                                                     , params->m_edgeFixup
                                                     , params->m_numCpuThreads
                                                     , &clContext
                                                     );

        cmft::setAllocator(dm::mainAlloc);

        // Cleanup.
        clContext.destroy();
        if (clLoaded)
        {
            bx::clUnload(); // Dynamically unload OpenCL lib.
        }

        // Check result.
        if (!success)
        {
            params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitFailure;
            return EXIT_FAILURE;
        }

        // Output gamma.
        cmft::imageApplyGamma(params->m_output, params->m_outputGamma);

    }
    else //if (cs::Environment::Iem == params->m_filterType).
    {
        cmft::imageApplyGamma(params->m_output, params->m_inputGamma);
        cmft::imageIrradianceFilterSh(params->m_output, params->m_dstSize);
        cmft::imageApplyGamma(params->m_output, params->m_outputGamma);
    }

    // Result.
    params->m_threadStatus = ThreadStatus::Completed | ThreadStatus::ExitSuccess;

    return EXIT_SUCCESS;
}

/* vim: set sw=4 ts=4 expandtab: */
