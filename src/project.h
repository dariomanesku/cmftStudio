/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_PROJECT_H_HEADER_GUARD
#define CMFTSTUDIO_PROJECT_H_HEADER_GUARD

#include "context.h" // cs::MaterialList,EnvList,MeshInstanceList

struct CmftStudioProject
{
    enum Error
    {
        FileIOError,
        InvalidMagic,
        InvalidVersion,
    };
};

typedef void (*OnValidFile)(uint32_t _flags, const void* _data);
typedef void (*OnInvalidFile)(uint32_t _flags, const void* _data);

struct Settings;

bool projectSave(const char* _path
               , const cs::MaterialList& _materialList
               , const cs::EnvList& _envList
               , const cs::MeshInstanceList& _meshInstList
               , const Settings& _savedSettings
               , int32_t _compressionLevel = 6 /*from 0 to 10*/
               , OnValidFile _validFileCallback = NULL
               , OnInvalidFile _invalidFileCallback = NULL
               , dm::StackAllocatorI* _stackAlloc = dm::stackAlloc
               );

bool projectLoad(const char* _path
               , cs::TextureList& _textureList
               , cs::MaterialList& _materialList
               , cs::EnvList& _envList
               , cs::MeshInstanceList& _meshInstList
               , Settings& _settings
               , OnValidFile _validFileCallback = NULL
               , OnInvalidFile _invalidFileCallback = NULL
               , dm::StackAllocatorI* _stackAlloc = dm::stackAlloc
               );

#endif // CMFTSTUDIO_PROJECT_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
