/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ASSETS_H_HEADER_GUARD
#define CMFTSTUDIO_ASSETS_H_HEADER_GUARD

#include "context.h"

struct Assets
{
    struct Textures
    {
        enum Enum
        {
            #define TEX_DESC(_name, _path) _name,
            #include "assets_res.h"

            Count,
            Invalid = UINT16_MAX
        };
    };

    struct Materials
    {
        enum Enum
        {
            #define MAT_DESC(_name \
                           , _d00, _d01, _d02, _d03 \
                           , _d10, _d11, _d12, _d13 \
                           , _d20, _d21, _d22, _d23 \
                           , _d30, _d31, _d32, _d33 \
                           , _d40, _d41, _d42, _d43 \
                           , _d50, _d51, _d52, _d53 \
                           , _d60, _d61, _d62, _d63 \
                           , _d70, _d71, _d72, _d73 \
                           , _d80, _d81, _d82, _d83 \
                           , _d90, _d91, _d92, _d93 \
                           , _texAlbedo           \
                           , _texNormal           \
                           , _texSurface          \
                           , _texReflectivity     \
                           , _texAmbientOcclusion \
                           , _texEmissive         \
                           ) \
            _name ,

            #include "assets_res.h"

            Count,
            Invalid = UINT16_MAX
        };
    };

    struct Meshes
    {
        enum Enum
        {
            #define MESH_DESC(_name, _path) _name,
            #include "assets_res.h"

            Count,
            Invalid = UINT16_MAX
        };
    };

    struct Environments
    {
        enum Enum
        {
            #define ENVMAP_DESC(_name, _skybox, _pmrem, _iem) _name,
            #include "assets_res.h"

            Count,
        };
    };

    struct MeshInstances
    {
        enum Enum
        {
            #define INST_DESC(_name, _mesh, _mat0, _mat1, _mat2, _scale, _posx, _posy, _posz, _rotx, _roty, _rotz) _name,
            #include "assets_res.h"

            Count,
        };
    };

    static void loadTextures();
    static void loadMaterials();
    static void loadMeshes();
    static void loadEnvironments();
    static void loadResources(); // Loads all at once.

    static cs::TextureHandle  get(Textures::Enum _texture);
    static cs::MaterialHandle get(Materials::Enum _material);
    static cs::MeshHandle     get(Meshes::Enum _mesh);
    static cs::EnvHandle      get(Environments::Enum _env);

    static void getAll(cs::TextureList& _textures);
    static void getAll(cs::MaterialList& _materials);
    static void getAll(cs::MeshList& _meshes);
    static void getAll(cs::EnvList& _environments);

    static void getAll(cs::MeshInstanceList& _meshInstances);

    static void releaseTextures();
    static void releaseMaterials();
    static void releaseMeshes();
    static void releaseEnvironments();
    static void releaseResources(); // Release all at once.
};

#endif // CMFTSTUDIO_ASSETS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
