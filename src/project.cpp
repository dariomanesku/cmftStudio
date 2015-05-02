/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "project.h"

#include "guimanager.h"     // imguiEnqueueStatusMessage(), outputWindow*()
#include "settings.h"       // Settings
#include "inflatedeflate.h" // DeflateFileReader/Writer

#include <bx/string.h>      // bx::snprintf

#define CMFTSTUDIO_CHUNK_MAGIC_PROJECT     BX_MAKEFOURCC('c', 's', 0x6, 0x9)
#define CMFTSTUDIO_CHUNK_MAGIC_MAT_BEGIN   BX_MAKEFOURCC('M', 'A', 'T', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_MAT_END     BX_MAKEFOURCC('M', 'A', 'T', 0x1)
#define CMFTSTUDIO_CHUNK_MAGIC_MIN_BEGIN   BX_MAKEFOURCC('M', 'I', 'N', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_MIN_END     BX_MAKEFOURCC('M', 'I', 'N', 0x1)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_BEGIN   BX_MAKEFOURCC('M', 'S', 'H', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_END     BX_MAKEFOURCC('M', 'S', 'H', 0x1)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_MISC    BX_MAKEFOURCC('M', 'S', 'H', 0x2)
#define CMFTSTUDIO_CHUNK_MAGIC_MSH_DONE    BX_MAKEFOURCC('M', 'S', 'H', 0x3)
#define CMFTSTUDIO_CHUNK_MAGIC_ENV_BEGIN   BX_MAKEFOURCC('E', 'N', 'V', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_ENV_END     BX_MAKEFOURCC('E', 'N', 'V', 0x1)
#define CMFTSTUDIO_CHUNK_MAGIC_TEX_BEGIN   BX_MAKEFOURCC('T', 'E', 'X', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_TEX_END     BX_MAKEFOURCC('T', 'E', 'X', 0x1)
#define CMFTSTUDIO_CHUNK_MAGIC_SET_BEGIN   BX_MAKEFOURCC('S', 'E', 'T', 0x0)
#define CMFTSTUDIO_CHUNK_MAGIC_SET_END     BX_MAKEFOURCC('S', 'E', 'T', 0x1)
#define CMFTSTUDIO_CHUNK_MAGIC_PROJECT_END BX_MAKEFOURCC(0x9, 0x6, 'c', 's')

bool projectSave(const char* _path
               , const cs::MaterialList& _materialList
               , const cs::EnvList& _envList
               , const cs::MeshInstanceList& _meshInstList
               , const Settings& _settings
               , int32_t _compressionLevel
               , OnValidFile _validFileCallback
               , OnInvalidFile _invalidFileCallback
               , dm::StackAllocatorI* _stackAlloc
               )
{
    // Open file for writing.
    FILE* file = fopen(_path, "wb");
    if (NULL == file)
    {
        if (NULL != _invalidFileCallback)
        {
            char msg[128] = "Error: Invalid output file specified!";
            if (NULL != _path && '\0' != _path[0])
            {
                bx::snprintf(msg, sizeof(msg), "Error: Unable to open output file \'%s\'!", _path);
            }
            _invalidFileCallback(CmftStudioProject::FileIOError, msg);
        }

        return false;
    }

    // File is valid.
    if (NULL != _validFileCallback)
    {
        _validFileCallback(0, NULL);
    }

    outputWindowPrint("Saving '%s'...", bx::baseName(_path));
    outputWindowPrint("Path: '%s'", _path);
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");
    outputWindowPrint(" Resource  |                                                                         Name  |      Size  ");
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");

    enum { BiggestId = 128 };
    dm::SetT<BiggestId> texturesToWrite;
    dm::SetT<BiggestId> meshesToWrite;

    // Write magic.
    const uint32_t magic = CMFTSTUDIO_CHUNK_MAGIC_PROJECT;
    fwrite(&magic, 1, sizeof(magic), file);

    // Write major version.
    const uint16_t versionMajor = g_versionMajor;
    fwrite(&versionMajor, 1, sizeof(versionMajor), file);

    // Write minor version.
    const uint16_t versionMinor = g_versionMinor;
    fwrite(&versionMinor, 1, sizeof(versionMinor), file);

    // Write compressed data from now on using DeflateFileWriter.
    cs::DeflateFileWriter writer(file, _stackAlloc, DM_MEGABYTES(100), DM_MEGABYTES(100), _compressionLevel);

    const uint64_t totalBefore      = writer.getTotal();
    const uint64_t compressedBefore = writer.getTotalCompressed();

    // Write materials.
    for (uint16_t ii = 0, end = _materialList.count(); ii < end; ++ii)
    {
        const cs::MaterialHandle material = _materialList[ii];
        const cs::Material& materialObj = cs::getObj(material);

        for (uint8_t jj = 0; jj < cs::Material::TextureCount; ++jj)
        {
            const cs::Material::Texture tex = (cs::Material::Texture)jj;
            const cs::TextureHandle texture = materialObj.get(tex);

            if (isValid(texture))
            {
                texturesToWrite.safeInsert(texture.m_idx);
            }
        }

        const uint64_t before = writer.getTotal();
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_MAT_BEGIN);
        cs::write(&writer, material);
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_MAT_END);
        const uint64_t after = writer.getTotal();

        const uint64_t size = after-before;
        outputWindowPrint("[Material] %79s - %4u.%03u MB", cs::getName(material), dm::U_UMB(size), size);
    }

    // Write used textures.
    for (uint16_t ii = 0, end = texturesToWrite.count(); ii < end; ++ii)
    {
        const cs::TextureHandle texture = { texturesToWrite.getValueAt(ii) };

        const uint64_t before = writer.getTotal();
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_TEX_BEGIN);
        cs::write(&writer, texture);
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_TEX_END);
        const uint64_t after = writer.getTotal();

        const uint64_t size = after-before;
        outputWindowPrint("[Texture]  %79s - %4u.%03u MB", cs::getName(texture), dm::U_UMB(size), size);
    }

    // Write mesh instances.
    for (uint16_t ii = 0, end = _meshInstList.count(); ii < end; ++ii)
    {
        meshesToWrite.safeInsert(_meshInstList[ii].m_mesh.m_idx);
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_MIN_BEGIN);
        cs::write(&writer, _meshInstList[ii]);
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_MIN_END);
    }

    // Write meshes.
    for (uint16_t ii = 0, end = meshesToWrite.count(); ii < end; ++ii)
    {
        const cs::MeshHandle mesh = { meshesToWrite.getValueAt(ii) };

        const uint64_t before = writer.getTotal();
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_MSH_BEGIN);
        cs::write(&writer, mesh);
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_MSH_END);
        const uint64_t after = writer.getTotal();

        const uint64_t size = after-before;
        outputWindowPrint("[Mesh]     %79s - %4u.%03u MB", cs::getName(mesh), dm::U_UMB(size), size);
    }

    // Write environments.
    for (uint16_t ii = 0, end = _envList.count(); ii < end; ++ii)
    {
        const cs::EnvHandle env = _envList[ii];

        const uint64_t before = writer.getTotal();
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_ENV_BEGIN);
        cs::write(&writer, env);
        bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_ENV_END);
        const uint64_t after = writer.getTotal();

        const uint64_t size = after-before;
        outputWindowPrint("[Env]      %79s - %4u.%03u MB", cs::getName(env), dm::U_UMB(size), size);
    }

    // Write settings.
    bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_SET_BEGIN);
    ::write(&writer, _settings);
    bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_SET_END);

    // Write project end magic.
    bx::write(&writer, CMFTSTUDIO_CHUNK_MAGIC_PROJECT_END);

    // All done.
    writer.flush();
    fclose(file);

    const uint64_t totalAfter = writer.getTotal();
    const uint64_t totalSize  = totalAfter-totalBefore;
    const uint64_t compressedAfter = writer.getTotalCompressed();
    const uint64_t compressedSize  = compressedAfter-compressedBefore;
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");
    outputWindowPrint("<Total>      %83u.%03u MB", dm::U_UMB(totalSize));
    outputWindowPrint("<Compressed> %83u.%03u MB", dm::U_UMB(compressedSize));
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");

    return true;
}

bool projectLoad(const char* _path
               , cs::TextureList& _textureList
               , cs::MaterialList& _materialList
               , cs::EnvList& _envList
               , cs::MeshInstanceList& _meshInstList
               , Settings& _settings
               , OnValidFile _validFileCallback
               , OnInvalidFile _invalidFileCallback
               , dm::StackAllocatorI* _stackAlloc
               )
{
    // Open file.
    bx::CrtFileReader fileReader;
    if (fileReader.open(_path))
    {
        if (NULL != _invalidFileCallback)
        {
            char msg[128] = "Error: Invalid input file specified!";
            if (NULL != _path && '\0' != _path[0])
            {
                bx::snprintf(msg, sizeof(msg), "Error: Unable to open input file \'%s\'!", _path);
            }
            _invalidFileCallback(CmftStudioProject::FileIOError, msg);
        }
        return false;
    }

    // Check magic.
    uint32_t magic = 0;
    fileReader.read(&magic, 4);
    if (magic != CMFTSTUDIO_CHUNK_MAGIC_PROJECT)
    {
        fileReader.close();

        if (NULL != _invalidFileCallback)
        {
            const char* msg = "Error: Selected file is not a valid cmftStudio project.";
            _invalidFileCallback(CmftStudioProject::InvalidMagic, msg);
        }

        return false;
    }

    // Check version.
    uint16_t versionMajor = 0;
    fileReader.read(&versionMajor, sizeof(versionMajor));

    uint16_t versionMinor = 0;
    fileReader.read(&versionMinor, sizeof(versionMinor));

    if (versionMajor != g_versionMajor
    ||  versionMinor != g_versionMinor)
    {
        fileReader.close();

        if (NULL != _invalidFileCallback)
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg)
                       , "Project file (v%d.%d) is not compatibile with the current version of cmftStudio (v%d.%d)!"
                       , versionMajor, versionMinor
                       , g_versionMajor, g_versionMinor
                       );
            _invalidFileCallback(CmftStudioProject::InvalidVersion, msg);
        }

        return false;
    }

    // File is valid.
    if (NULL != _validFileCallback)
    {
        _validFileCallback(0, NULL);
    }

    outputWindowPrint("Loading '%s'...", bx::baseName(_path));

    // Get remaining file size.
    const uint32_t curr = (uint32_t)fileReader.seek(0, bx::Whence::Current);
    const uint32_t end  = (uint32_t)fileReader.seek(0, bx::Whence::End);
    const uint32_t compressedSize = end-curr;
    fileReader.seek(curr, bx::Whence::Begin);

    // Read and decompress project data.
    dm::StackAllocScope scope(_stackAlloc);
    DynamicMemoryBlockWriter memory(_stackAlloc, DM_MEGABYTES(512));
    const bool result = cs::readInflate(&memory, &fileReader, compressedSize, _stackAlloc);
    CS_CHECK(result == true, "cs::readInflate() failed!");
    BX_UNUSED(result);

    fileReader.close();

    void*    data      = memory.getDataTrim();
    uint32_t dataSize  = memory.getDataSize();

    outputWindowPrint("--------------------------------------------------------------------------------------------------------");
    outputWindowPrint(" Resource  |                                                                         Name  |      Size  ");
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");

    dm::MemoryReader reader(data, dataSize);
    const uint64_t totalBefore = reader.seek(0, bx::Whence::Current);

    // Keep track of loaded meshes.
    HandleArrayT<cs::MeshHandle, CS_MAX_MESHES> meshList;

    bool done = false;
    uint32_t chunk;
    while (!done && 4 == bx::read(&reader, chunk) )
    {
        switch (chunk)
        {
        case CMFTSTUDIO_CHUNK_MAGIC_MSH_BEGIN:
            {
                const uint64_t before = reader.seek(0, bx::Whence::Current);
                const cs::MeshHandle mesh = cs::readMesh(&reader, _stackAlloc);
                meshList.add(mesh);
                const uint64_t after = reader.seek(0, bx::Whence::Current);

                uint32_t end;
                bx::read(&reader, end);
                DM_CHECK(CMFTSTUDIO_CHUNK_MAGIC_MSH_END == end, "Error reading file!");

                const uint64_t size = after-before;
                outputWindowPrint("[Mesh]     %79s - %4u.%03u MB", cs::getName(mesh), dm::U_UMB(size), size);
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_MIN_BEGIN:
            {
                cs::MeshInstance* obj = _meshInstList.addNew();
                cs::readMeshInstance(&reader, obj);

                uint32_t end;
                bx::read(&reader, end);
                DM_CHECK(CMFTSTUDIO_CHUNK_MAGIC_MIN_END == end, "Error reading file!");
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_ENV_BEGIN:
            {
                const uint64_t before = reader.seek(0, bx::Whence::Current);
                const cs::EnvHandle env = cs::readEnv(&reader, _stackAlloc);
                _envList.add(env);
                const uint64_t after = reader.seek(0, bx::Whence::Current);

                uint32_t end;
                bx::read(&reader, end);
                DM_CHECK(CMFTSTUDIO_CHUNK_MAGIC_ENV_END == end, "Error reading file!");

                const uint64_t size = after-before;
                outputWindowPrint("[Env]      %79s - %4u.%03u MB", cs::getName(env), dm::U_UMB(size), size);
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_MAT_BEGIN:
            {
                const uint64_t before = reader.seek(0, bx::Whence::Current);
                const cs::MaterialHandle material = cs::readMaterial(&reader, _stackAlloc);
                _materialList.add(material);
                const uint64_t after = reader.seek(0, bx::Whence::Current);

                uint32_t end;
                bx::read(&reader, end);
                DM_CHECK(CMFTSTUDIO_CHUNK_MAGIC_MAT_END == end, "Error reading file!");

                const uint64_t size = after-before;
                outputWindowPrint("[Material] %79s - %4u.%03u MB", cs::getName(material), dm::U_UMB(size), size);
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_TEX_BEGIN:
            {
                const uint64_t before = reader.seek(0, bx::Whence::Current);
                const cs::TextureHandle texture = cs::readTexture(&reader, _stackAlloc);
                _textureList.add(texture);
                const uint64_t after = reader.seek(0, bx::Whence::Current);

                uint32_t end;
                bx::read(&reader, end);
                DM_CHECK(CMFTSTUDIO_CHUNK_MAGIC_TEX_END == end, "Error reading file!");

                const uint64_t size = after-before;
                outputWindowPrint("[Texture]  %79s - %4u.%03u MB", cs::getName(texture), dm::U_UMB(size), size);
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_SET_BEGIN:
            {
                ::read(&reader, _settings);

                uint32_t end;
                bx::read(&reader, end);
                DM_CHECK(CMFTSTUDIO_CHUNK_MAGIC_SET_END == end, "Error reading file!");
            }
        break;

        case CMFTSTUDIO_CHUNK_MAGIC_PROJECT_END:
            {
                done = true;
            }
        break;

        default:
            {
                CS_CHECK(0, "Error reading project file! %08x at %lld", chunk, reader.seek(0, bx::Whence::Current));
                done = true;
            }
        break;
        }
    }

    const uint64_t totalAfter = reader.seek(0, bx::Whence::Current);
    const uint64_t totalSize = totalAfter-totalBefore;
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");
    outputWindowPrint("<File size>          %76u.%03u MB", dm::U_UMB(compressedSize));
    outputWindowPrint("<Decompressed>       %76u.%03u MB", dm::U_UMB(totalSize));
    outputWindowPrint("--------------------------------------------------------------------------------------------------------");

    // Resolve loaded resources.
    cs::resourceResolveAll();
    cs::resourceClearMappings();

    // Cleanup.
    for (uint32_t ii = meshList.count(); ii--; )
    {
        cs::release(meshList[ii]);
    }

    DM_FREE(_stackAlloc, data);

    return true;
}

/* vim: set sw=4 ts=4 expandtab: */
