/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "assets.h"

#include <stdint.h>
#include "common/datastructures.h"

#define CS_INCLUDE_RESOURCE_ARRAYS
#include "assets_res.h"

// Use as: 'cs::TextureListT<32>::Type m_textureList;'
template <uint16_t MaxHandlesT> struct TextureListT  { typedef HandleArrayT<cs::TextureHandle,  MaxHandlesT> Type; };
template <uint16_t MaxHandlesT> struct MaterialListT { typedef HandleArrayT<cs::MaterialHandle, MaxHandlesT> Type; };
template <uint16_t MaxHandlesT> struct MeshListT     { typedef HandleArrayT<cs::MeshHandle,     MaxHandlesT> Type; };
template <uint16_t MaxHandlesT> struct EnvListT      { typedef HandleArrayT<cs::EnvHandle,      MaxHandlesT> Type; };

struct AssetsImpl
{
    AssetsImpl()
    {
        m_texturesLoaded     = false;
        m_materialsLoaded    = false;
        m_meshesLoaded       = false;
        m_environmentsLoaded = false;
    }

    void loadTextures()
    {
        if (m_texturesLoaded)
        {
            return;
        }

        m_textureList.reset();
        #define TEX_DESC(_name, _path)                                                                  \
        {                                                                                               \
            m_textureList.add(cs::textureLoad(#_name, _path));                                          \
            BX_CHECK((m_textureList.count()-1) == Assets::Textures:: ##_name, "Initialization error."); \
        }
        #include "assets_res.h"

        m_texturesLoaded = true;
    }

    void loadMaterials()
    {
        if (m_materialsLoaded)
        {
            return;
        }

        if (!m_texturesLoaded)
        {
            loadTextures();
        }

        m_materialList.reset();
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
                       , _texAlbedo             \
                       , _texNormal             \
                       , _texSurface            \
                       , _texRefl               \
                       , _texAo                 \
                       , _texEmissive           \
                       ) \
        { \
            const float _name ## Data[cs::Material::Size] = \
            {                           \
                _d00, _d01, _d02, _d03, \
                _d10, _d11, _d12, _d13, \
                _d20, _d21, _d22, _d23, \
                _d30, _d31, _d32, _d33, \
                _d40, _d41, _d42, _d43, \
                _d50, _d51, _d52, _d53, \
                _d60, _d61, _d62, _d63, \
                _d70, _d71, _d72, _d73, \
                _d80, _d81, _d82, _d83, \
                _d90, _d91, _d92, _d93, \
            };                          \
                                        \
            const cs::MaterialHandle handle = cs::materialCreate(#_name, _name ## Data); \
            cs::Material& mat = cs::getObj(handle);                                      \
            if (UINT16_MAX != Assets::Textures::_texAlbedo)   { mat.set(cs::Material::Albedo,       m_textureList[Assets::Textures::_texAlbedo]);   } \
            if (UINT16_MAX != Assets::Textures::_texNormal)   { mat.set(cs::Material::Normal,       m_textureList[Assets::Textures::_texNormal]);   } \
            if (UINT16_MAX != Assets::Textures::_texSurface)  { mat.set(cs::Material::Surface,      m_textureList[Assets::Textures::_texSurface]);  } \
            if (UINT16_MAX != Assets::Textures::_texRefl)     { mat.set(cs::Material::Reflectivity, m_textureList[Assets::Textures::_texRefl]);     } \
            if (UINT16_MAX != Assets::Textures::_texAo)       { mat.set(cs::Material::Occlusion,    m_textureList[Assets::Textures::_texAo]);       } \
            if (UINT16_MAX != Assets::Textures::_texEmissive) { mat.set(cs::Material::Emissive,     m_textureList[Assets::Textures::_texEmissive]); } \
            m_materialList.add(handle); \
            BX_CHECK((m_materialList.count()-1) == Assets::Materials:: ##_name, "Initialization error."); \
        }
        #include "assets_res.h"

        m_materialsLoaded = true;
    }

    void loadMeshes()
    {
        if (m_meshesLoaded)
        {
            return;
        }

        if (!m_materialsLoaded)
        {
            loadMaterials();
        }

        m_meshList.reset();
        #define MESH_DESC(_name, _path)                                                            \
        {                                                                                          \
            m_meshList.add(cs::meshLoad(#_name, _path));                                           \
            BX_CHECK((m_meshList.count()-1) == Assets::Meshes:: ##_name, "Initialization error."); \
        }
        #include "assets_res.h"

        m_meshesLoaded = true;
    }

    void loadEnvironments()
    {
        if (m_environmentsLoaded)
        {
            return;
        }

        m_envList.reset();
        #define ENVMAP_DESC(_name, _skybox, _pmrem, _iem)                                               \
        {                                                                                               \
            m_envList.add(cs::envCreate(#_name, _skybox, _pmrem, _iem));                                \
            BX_CHECK((m_envList.count()-1) == Assets::Environments:: ##_name, "Initialization error."); \
        }
        #include "assets_res.h"

        m_environmentsLoaded = true;
    }

    template <typename ListT>
    inline void release(ListT& _list)
    {
        for (uint32_t ii = 0, end = _list.count(); ii < end; ++ii)
        {
            cs::release(_list[ii]);
        }
        _list.reset();
    }

    void releaseTextures()
    {
        release(m_textureList);
    }

    void releaseMaterials()
    {
        release(m_materialList);
    }

    void releaseMeshes()
    {
        release(m_meshList);
    }

    void releaseEnvironments()
    {
        release(m_envList);
    }

    cs::TextureHandle getTexture(Assets::Textures::Enum _texture)
    {
        const uint16_t idx = (uint16_t)_texture;
        return m_textureList.get(idx);
    }

    cs::MaterialHandle getMaterial(Assets::Materials::Enum _material)
    {
        const uint16_t idx = (uint16_t)_material;
        return m_materialList.get(idx);
    }

    cs::MeshHandle getMesh(Assets::Meshes::Enum _mesh)
    {
        const uint16_t idx = (uint16_t)_mesh;
        return m_meshList.get(idx);
    }

    cs::EnvHandle getEnv(Assets::Environments::Enum _env)
    {
        const uint16_t idx = (uint16_t)_env;
        return m_envList.get(idx);
    }

private:
    bool m_texturesLoaded;
    bool m_materialsLoaded;
    bool m_meshesLoaded;
    bool m_environmentsLoaded;
    TextureListT<32>::Type  m_textureList;
    MaterialListT<32>::Type m_materialList;
    MeshListT<32>::Type     m_meshList;
    EnvListT<32>::Type      m_envList;
};
static AssetsImpl s_assetsImpl;

void Assets::loadTextures()
{
    s_assetsImpl.loadTextures();
}

void Assets::loadMaterials()
{
    s_assetsImpl.loadMaterials();
}

void Assets::loadMeshes()
{
    s_assetsImpl.loadMeshes();
}

void Assets::loadEnvironments()
{
    s_assetsImpl.loadEnvironments();
}

void Assets::loadResources()
{
    Assets::loadTextures();
    Assets::loadMaterials();
    Assets::loadMeshes();
    Assets::loadEnvironments();
}

void Assets::releaseTextures()
{
    s_assetsImpl.releaseTextures();
}

void Assets::releaseMaterials()
{
    s_assetsImpl.releaseMaterials();
}

void Assets::releaseMeshes()
{
    s_assetsImpl.releaseMeshes();
}

void Assets::releaseEnvironments()
{
    s_assetsImpl.releaseEnvironments();
}

void Assets::releaseResources()
{
    releaseTextures();
    releaseMaterials();
    releaseMeshes();
    releaseEnvironments();
}

cs::TextureHandle Assets::get(Assets::Textures::Enum _texture)
{
    return s_assetsImpl.getTexture(_texture);
}

cs::MaterialHandle Assets::get(Assets::Materials::Enum _material)
{
    return s_assetsImpl.getMaterial(_material);
}

cs::MeshHandle Assets::get(Assets::Meshes::Enum _mesh)
{
    return s_assetsImpl.getMesh(_mesh);
}

cs::EnvHandle Assets::get(Assets::Environments::Enum _env)
{
    return s_assetsImpl.getEnv(_env);
}

void Assets::getAll(cs::TextureList& _textures)
{
    for (uint16_t ii = 0, end = Assets::Textures::Count; ii < end; ++ii)
    {
        const Assets::Textures::Enum tex = (Assets::Textures::Enum)ii;
        _textures.add(cs::acquire(Assets::get(tex)));
    }
}

void Assets::getAll(cs::MaterialList& _materials)
{
    for (uint16_t ii = 0, end = Assets::Materials::Count; ii < end; ++ii)
    {
        const Assets::Materials::Enum mat = (Assets::Materials::Enum)ii;
        _materials.add(cs::acquire(Assets::get(mat)));
    }
}

void Assets::getAll(cs::MeshList& _meshes)
{
    for (uint16_t ii = 0, end = Assets::Meshes::Count; ii < end; ++ii)
    {
        const Assets::Meshes::Enum mesh = (Assets::Meshes::Enum)ii;
        _meshes.add(cs::acquire(Assets::get(mesh)));
    }
}

void Assets::getAll(cs::EnvList& _environments)
{
    for (uint16_t ii = 0, end = Assets::Environments::Count; ii < end; ++ii)
    {
        const Assets::Environments::Enum env = (Assets::Environments::Enum)ii;
        _environments.add(cs::acquire(Assets::get(env)));
    }
}

void Assets::getAll(cs::MeshInstanceList& _meshInstances)
{
    BX_UNUSED(_meshInstances);

    #define INST_DESC(_name, _mesh, _mat0, _mat1, _mat2, _scale, _posx, _posy, _posz, _rotx, _roty, _rotz) \
    {                                                            \
        cs::MeshInstance& inst = _meshInstances.addNew();        \
        inst.set(Assets::get(Assets::Meshes::_mesh));            \
        inst.set(Assets::get(Assets::Materials::_mat0));         \
        inst.m_scaleAdj = _scale;                                \
        inst.m_pos[0] = _posx;                                   \
        inst.m_pos[1] = _posy;                                   \
        inst.m_pos[2] = _posz;                                   \
        inst.m_rot[0] = _rotx;                                   \
        inst.m_rot[1] = _roty;                                   \
        inst.m_rot[2] = _rotz;                                   \
        if (Assets::Materials::Invalid != Assets::Materials::_mat0) { inst.set(Assets::get(Assets::Materials::_mat0), 0); } \
        if (Assets::Materials::Invalid != Assets::Materials::_mat1) { inst.set(Assets::get(Assets::Materials::_mat1), 1); } \
        if (Assets::Materials::Invalid != Assets::Materials::_mat2) { inst.set(Assets::get(Assets::Materials::_mat2), 2); } \
    }
    #include "assets_res.h"
}

/* vim: set sw=4 ts=4 expandtab: */
