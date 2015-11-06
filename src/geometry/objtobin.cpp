/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "../common/common.h"
#include "objtobin.h"

#include "../guimanager.h"
#define CS_PRINT(...) printf(__VA_ARGS__); outputWindowPrint(__VA_ARGS__)

#include "geometry.h"           // write(vertices, indices..)
#include "../common/utils.h"
#include "../common/memblock.h"
#include <dm/misc.h>            // dm::NoCopyNoAssign

// This code is altered from: https://github.com/bkaradzic/bgfx/blob/master/tools/geometryc/geometryc.cpp
/*
 * Copyright 2011-2014 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include <bgfx/bgfx.h>
#include <bounds.h> //Aabb, Obb, Sphere
#include <../../src/vertexdecl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <algorithm>
#if CS_OBJTOBIN_USES_TINYSTL
#   include "../common/tinystl.h"
#   define CS_STL stl
#else
#   include <vector>
#   include <unordered_map>
#   define CS_STL std
#endif

#include <forsythtriangleorderoptimizer.h>

#include <bx/bx.h>
#include <bx/debug.h>
#include <bx/commandline.h>
#include <bx/timer.h>
#include <bx/readerwriter.h>
#include <bx/hash.h>
#include <bx/uint32_t.h>
#include <bx/fpumath.h>
#include <bx/tokenizecmd.h>

struct Vector3
{
    float x;
    float y;
    float z;
};

typedef CS_STL::vector<Vector3> Vector3Array;

struct Index3
{
    int32_t m_position;
    int32_t m_texcoord;
    int32_t m_normal;
    int32_t m_vertexIndex;
};

typedef CS_STL::unordered_map<uint64_t, Index3> Index3Map;

struct Triangle
{
    uint64_t m_index[3];
};

typedef CS_STL::vector<Triangle> TriangleArray;

struct MeshGroup
{
    uint32_t m_startTriangle;
    uint32_t m_numTriangles;
    std::string m_name;
    std::string m_material;
};

typedef CS_STL::vector<MeshGroup> BgfxGroupArray;

typedef CS_STL::vector<Primitive> BgfxPrimitiveArray;

#define BGFX_CHUNK_MAGIC_GEO BX_MAKEFOURCC('G', 'E', 'O', 0x0)
#define BGFX_CHUNK_MAGIC_VB  BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB  BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

static void triangleReorder(uint16_t* _indices, uint32_t _numIndices, uint32_t _numVertices, uint16_t _cacheSize)
{
    uint16_t* newIndexList = new uint16_t[_numIndices];
    Forsyth::OptimizeFaces(_indices, _numIndices, _numVertices, newIndexList, _cacheSize);
    memcpy(_indices, newIndexList, _numIndices*2);
    delete [] newIndexList;
}

static void calculateTangents(void* _vertices, uint16_t _numVertices, bgfx::VertexDecl _decl, const uint16_t* _indices, uint32_t _numIndices)
{
    struct PosTexcoord
    {
        float m_x;
        float m_y;
        float m_z;
        float m_pad0;
        float m_u;
        float m_v;
        float m_pad1;
        float m_pad2;
    };

    float* tangents = new float[6*_numVertices];
    memset(tangents, 0, 6*_numVertices*sizeof(float) );

    PosTexcoord v0;
    PosTexcoord v1;
    PosTexcoord v2;

    for (uint32_t ii = 0, num = _numIndices/3; ii < num; ++ii)
    {
        const uint16_t* indices = &_indices[ii*3];
        uint32_t i0 = indices[0];
        uint32_t i1 = indices[1];
        uint32_t i2 = indices[2];

        bgfx::vertexUnpack(&v0.m_x, bgfx::Attrib::Position, _decl, _vertices, i0);
        bgfx::vertexUnpack(&v0.m_u, bgfx::Attrib::TexCoord0, _decl, _vertices, i0);

        bgfx::vertexUnpack(&v1.m_x, bgfx::Attrib::Position, _decl, _vertices, i1);
        bgfx::vertexUnpack(&v1.m_u, bgfx::Attrib::TexCoord0, _decl, _vertices, i1);

        bgfx::vertexUnpack(&v2.m_x, bgfx::Attrib::Position, _decl, _vertices, i2);
        bgfx::vertexUnpack(&v2.m_u, bgfx::Attrib::TexCoord0, _decl, _vertices, i2);

        const float bax = v1.m_x - v0.m_x;
        const float bay = v1.m_y - v0.m_y;
        const float baz = v1.m_z - v0.m_z;
        const float bau = v1.m_u - v0.m_u;
        const float bav = v1.m_v - v0.m_v;

        const float cax = v2.m_x - v0.m_x;
        const float cay = v2.m_y - v0.m_y;
        const float caz = v2.m_z - v0.m_z;
        const float cau = v2.m_u - v0.m_u;
        const float cav = v2.m_v - v0.m_v;

        const float det = (bau * cav - bav * cau);
        const float invDet = 1.0f / det;

        const float tx = (bax * cav - cax * bav) * invDet;
        const float ty = (bay * cav - cay * bav) * invDet;
        const float tz = (baz * cav - caz * bav) * invDet;

        const float bx = (cax * bau - bax * cau) * invDet;
        const float by = (cay * bau - bay * cau) * invDet;
        const float bz = (caz * bau - baz * cau) * invDet;

        for (uint32_t jj = 0; jj < 3; ++jj)
        {
            float* tanu = &tangents[indices[jj]*6];
            float* tanv = &tanu[3];
            tanu[0] += tx;
            tanu[1] += ty;
            tanu[2] += tz;

            tanv[0] += bx;
            tanv[1] += by;
            tanv[2] += bz;
        }
    }

    for (uint32_t ii = 0; ii < _numVertices; ++ii)
    {
        const float* tanu = &tangents[ii*6];
        const float* tanv = &tangents[ii*6 + 3];

        float normal[4];
        bgfx::vertexUnpack(normal, bgfx::Attrib::Normal, _decl, _vertices, ii);
        float ndt = bx::vec3Dot(normal, tanu);

        float nxt[3];
        bx::vec3Cross(nxt, normal, tanu);

        float tmp[3];
        tmp[0] = tanu[0] - normal[0] * ndt;
        tmp[1] = tanu[1] - normal[1] * ndt;
        tmp[2] = tanu[2] - normal[2] * ndt;

        float tangent[4];
        bx::vec3Norm(tangent, tmp);

        tangent[3] = bx::vec3Dot(nxt, tanv) < 0.0f ? -1.0f : 1.0f;
        bgfx::vertexPack(tangent, true, bgfx::Attrib::Tangent, _decl, _vertices, ii);
    }

    delete [] tangents;
}

inline uint32_t rgbaToAbgr(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a)
{
    return (uint32_t(_r)<<0)
         | (uint32_t(_g)<<8)
         | (uint32_t(_b)<<16)
         | (uint32_t(_a)<<24)
         ;
}

struct GroupSortByMaterial
{
    bool operator()(const MeshGroup& _lhs, const MeshGroup& _rhs)
    {
        return _lhs.m_material < _rhs.m_material;
    }
};

uint32_t objToBin(const uint8_t* _objData
                , bx::WriterSeekerI* _writer
                , uint32_t _packUv
                , uint32_t _packNormal
                , bool _ccw
                , bool _flipV
                , bool _hasTangent
                , float _scale
                )
{
    int64_t parseElapsed = -bx::getHPCounter();
    int64_t triReorderElapsed = 0;

    const int64_t begin = _writer->seek();

    Vector3Array positions;
    Vector3Array normals;
    Vector3Array texcoords;
    Index3Map indexMap;
    TriangleArray triangles;
    BgfxGroupArray groups;

    uint32_t num = 0;

    MeshGroup group;
    group.m_startTriangle = 0;
    group.m_numTriangles = 0;
    group.m_name = "";
    group.m_material = "";

    char commandLine[2048];
    uint32_t len = sizeof(commandLine);
    int argc;
    char* argv[64];
    const char* next = (const char*)_objData;
    do
    {
        next = bx::tokenizeCommandLine(next, commandLine, len, argc, argv, BX_COUNTOF(argv), '\n');
        if (0 < argc)
        {
            if (0 == strcmp(argv[0], "#") )
            {
                if (2 < argc
                &&  0 == strcmp(argv[2], "polygons") )
                {
                }
            }
            else if (0 == strcmp(argv[0], "f") )
            {
                Triangle triangle;
                memset(&triangle, 0, sizeof(Triangle) );

                const int numNormals   = (int)normals.size();
                const int numTexcoords = (int)texcoords.size();
                const int numPositions = (int)positions.size();
                for (uint32_t edge = 0, numEdges = argc-1; edge < numEdges; ++edge)
                {
                    Index3 index;
                    index.m_texcoord = 0;
                    index.m_normal = 0;
                    index.m_vertexIndex = -1;

                    char* vertex = argv[edge+1];
                    char* texcoord = strchr(vertex, '/');
                    if (NULL != texcoord)
                    {
                        *texcoord++ = '\0';

                        char* normal = strchr(texcoord, '/');
                        if (NULL != normal)
                        {
                            *normal++ = '\0';
                            const int nn = atoi(normal);
                            index.m_normal = (nn < 0) ? nn+numNormals : nn-1;
                        }

                        const int tex = atoi(texcoord);
                        index.m_texcoord = (tex < 0) ? tex+numTexcoords : tex-1;
                    }

                    const int pos = atoi(vertex);
                    index.m_position = (pos < 0) ? pos+numPositions : pos-1;

                    uint64_t hash0 = index.m_position;
                    uint64_t hash1 = uint64_t(index.m_texcoord)<<20;
                    uint64_t hash2 = uint64_t(index.m_normal)<<40;
                    uint64_t hash = hash0^hash1^hash2;

                    CS_STL::pair<Index3Map::iterator, bool> result = indexMap.insert(CS_STL::make_pair(hash, index) );
                    if (!result.second)
                    {
                        Index3& oldIndex = result.first->second;
                        BX_UNUSED(oldIndex);
                        BX_CHECK(oldIndex.m_position == index.m_position
                            && oldIndex.m_texcoord == index.m_texcoord
                            && oldIndex.m_normal == index.m_normal
                            , "Hash collision!"
                            );
                    }

                    switch (edge)
                    {
                    case 0:
                    case 1:
                    case 2:
                        triangle.m_index[edge] = hash;
                        if (2 == edge)
                        {
                            if (_ccw)
                            {
                                std::swap(triangle.m_index[1], triangle.m_index[2]);
                            }
                            triangles.push_back(triangle);
                        }
                        break;

                    default:
                        if (_ccw)
                        {
                            triangle.m_index[2] = triangle.m_index[1];
                            triangle.m_index[1] = hash;
                        }
                        else
                        {
                            triangle.m_index[1] = triangle.m_index[2];
                            triangle.m_index[2] = hash;
                        }
                        triangles.push_back(triangle);
                        break;
                    }
                }
            }
            else if (0 == strcmp(argv[0], "g") )
            {
                if (1 >= argc)
                {
                    CS_PRINT("Error parsing *.obj file.\n");
                    return 0;
                }
                group.m_name = argv[1];
            }
            else if (*argv[0] == 'v')
            {
                group.m_numTriangles = (uint32_t)(triangles.size() ) - group.m_startTriangle;
                if (0 < group.m_numTriangles)
                {
                    groups.push_back(group);
                    group.m_startTriangle = (uint32_t)(triangles.size() );
                    group.m_numTriangles = 0;
                }

                if (0 == strcmp(argv[0], "vn") )
                {
                    Vector3 normal;
                    normal.x = (float)atof(argv[1]);
                    normal.y = (float)atof(argv[2]);
                    normal.z = (float)atof(argv[3]);

                    normals.push_back(normal);
                }
                else if (0 == strcmp(argv[0], "vp") )
                {
                    static bool once = true;
                    if (once)
                    {
                        once = false;
                        CS_PRINT("warning: 'parameter space vertices' are unsupported.\n");
                    }
                }
                else if (0 == strcmp(argv[0], "vt") )
                {
                    Vector3 texcoord;
                    texcoord.x = (float)atof(argv[1]);
                    texcoord.y = 0.0f;
                    texcoord.z = 0.0f;
                    switch (argc)
                    {
                    case 4:
                        texcoord.z = (float)atof(argv[3]);
                        // fallthrough
                    case 3:
                        texcoord.y = (float)atof(argv[2]);
                        break;

                    default:
                        break;
                    }

                    texcoords.push_back(texcoord);
                }
                else
                {
                    float px = (float)atof(argv[1]);
                    float py = (float)atof(argv[2]);
                    float pz = (float)atof(argv[3]);
                    float pw = 1.0f;
                    if (argc > 4)
                    {
                        pw = (float)atof(argv[4]);
                    }

                    float invW = _scale/pw;
                    px *= invW;
                    py *= invW;
                    pz *= invW;

                    Vector3 pos;
                    pos.x = px;
                    pos.y = py;
                    pos.z = pz;

                    positions.push_back(pos);
                }
            }
            else if (0 == strcmp(argv[0], "usemtl") )
            {
                std::string material(argv[1]);

                if (material != group.m_material)
                {
                    group.m_numTriangles = (uint32_t)(triangles.size() ) - group.m_startTriangle;
                    if (0 < group.m_numTriangles)
                    {
                        groups.push_back(group);
                        group.m_startTriangle = (uint32_t)(triangles.size() );
                        group.m_numTriangles = 0;
                    }
                }

                group.m_material = material;
            }
// unsupported tags
//              else if (0 == strcmp(argv[0], "mtllib") )
//              {
//              }
//              else if (0 == strcmp(argv[0], "o") )
//              {
//              }
//              else if (0 == strcmp(argv[0], "s") )
//              {
//              }
        }

        ++num;
    }
    while ('\0' != *next);

    group.m_numTriangles = (uint32_t)(triangles.size() ) - group.m_startTriangle;
    if (0 < group.m_numTriangles)
    {
        groups.push_back(group);
        group.m_startTriangle = (uint32_t)(triangles.size() );
        group.m_numTriangles = 0;
    }

    int64_t now = bx::getHPCounter();
    parseElapsed += now;
    int64_t convertElapsed = -now;

    std::sort(groups.begin(), groups.end(), GroupSortByMaterial() );

    bool hasColor = false;
    bool hasNormal;
    bool hasTexcoord;
    {
        Index3Map::const_iterator it = indexMap.begin();
        hasNormal   = 0 != it->second.m_normal;
        hasTexcoord = 0 != it->second.m_texcoord;

        if (!hasTexcoord
        &&  texcoords.size() == positions.size() )
        {
            hasTexcoord = true;

            for (Index3Map::iterator it = indexMap.begin(), itEnd = indexMap.end(); it != itEnd; ++it)
            {
                it->second.m_texcoord = it->second.m_position;
            }
        }

        if (!hasNormal
        &&  normals.size() == positions.size() )
        {
            hasNormal = true;

            for (Index3Map::iterator it = indexMap.begin(), itEnd = indexMap.end(); it != itEnd; ++it)
            {
                it->second.m_normal = it->second.m_position;
            }
        }
    }

    bgfx::VertexDecl decl;
    decl.begin();
    decl.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);

    if (hasColor)
    {
        decl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
    }

    if (hasTexcoord)
    {
        switch (_packUv)
        {
        default:
        case 0:
            decl.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
            break;

        case 1:
            decl.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Half);
            break;
        }
    }

    if (hasNormal)
    {
        _hasTangent &= hasTexcoord;

        switch (_packNormal)
        {
        default:
        case 0:
            decl.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);
            if (_hasTangent)
            {
                decl.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float);
            }
            break;

        case 1:
            decl.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true);
            if (_hasTangent)
            {
                decl.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Uint8, true, true);
            }
            break;
        }
    }
    decl.end();

    uint32_t stride = decl.getStride();
    uint8_t* vertexData = new uint8_t[triangles.size() * 3 * stride];
    uint16_t* indexData = new uint16_t[triangles.size() * 3];
    int32_t numVertices = 0;
    int32_t numIndices = 0;
    int32_t numPrimitives = 0;

    uint8_t* vertices = vertexData;
    uint16_t* indices = indexData;

    std::string material = groups.begin()->m_material;

    BgfxPrimitiveArray primitives;

    Primitive prim;
    prim.m_startVertex = 0;
    prim.m_startIndex  = 0;

    uint32_t positionOffset = decl.getOffset(bgfx::Attrib::Position);
    uint32_t color0Offset   = decl.getOffset(bgfx::Attrib::Color0);

    uint32_t ii = 0;
    for (BgfxGroupArray::const_iterator groupIt = groups.begin(); groupIt != groups.end(); ++groupIt, ++ii)
    {
        for (uint32_t tri = groupIt->m_startTriangle, end = tri + groupIt->m_numTriangles; tri < end; ++tri)
        {
            if (material != groupIt->m_material
            ||  65533 < numVertices)
            {
                prim.m_numVertices = numVertices - prim.m_startVertex;
                prim.m_numIndices = numIndices - prim.m_startIndex;
                if (0 < prim.m_numVertices)
                {
                    primitives.push_back(prim);
                }

                triReorderElapsed -= bx::getHPCounter();
                for (BgfxPrimitiveArray::const_iterator primIt = primitives.begin(); primIt != primitives.end(); ++primIt)
                {
                    const Primitive& prim = *primIt;
                    triangleReorder(indexData + prim.m_startIndex, prim.m_numIndices, numVertices, 32);
                }
                triReorderElapsed += bx::getHPCounter();

                if (_hasTangent)
                {
                    calculateTangents(vertexData, numVertices, decl, indexData, numIndices);
                }

                write(_writer
                    , vertexData
                    , numVertices
                    , decl
                    , indexData
                    , numIndices
                    , material.c_str()
                    , primitives.data()
                    , (uint32_t)primitives.size()
                    );
                primitives.clear();

                for (Index3Map::iterator indexIt = indexMap.begin(); indexIt != indexMap.end(); ++indexIt)
                {
                    indexIt->second.m_vertexIndex = -1;
                }

                vertices = vertexData;
                indices = indexData;
                numVertices = 0;
                numIndices = 0;
                prim.m_startVertex = 0;
                prim.m_startIndex = 0;
                ++numPrimitives;

                material = groupIt->m_material;
            }

            Triangle& triangle = triangles[tri];
            for (uint32_t edge = 0; edge < 3; ++edge)
            {
                uint64_t hash = triangle.m_index[edge];
                Index3& index = indexMap[hash];
                if (index.m_vertexIndex == -1)
                {
                    index.m_vertexIndex = numVertices++;

                    float* position = (float*)(vertices + positionOffset);
                    memcpy(position, &positions[index.m_position], 3*sizeof(float) );

                    if (hasColor)
                    {
                        uint32_t* color0 = (uint32_t*)(vertices + color0Offset);
                        *color0 = rgbaToAbgr(numVertices%255, numIndices%255, 0, 0xff);
                    }

                    if (hasTexcoord)
                    {
                        float uv[2];
                        memcpy(uv, &texcoords[index.m_texcoord], 2*sizeof(float) );

                        if (_flipV)
                        {
                            uv[1] = -uv[1];
                        }

                        bgfx::vertexPack(uv, true, bgfx::Attrib::TexCoord0, decl, vertices);
                    }

                    if (hasNormal)
                    {
                        float normal[4];
                        bx::vec3Norm(normal, (float*)&normals[index.m_normal]);
                        bgfx::vertexPack(normal, true, bgfx::Attrib::Normal, decl, vertices);
                    }

                    vertices += stride;
                }

                *indices++ = (uint16_t)index.m_vertexIndex;
                ++numIndices;
            }
        }

        if (0 < numVertices)
        {
            prim.m_numVertices = numVertices - prim.m_startVertex;
            prim.m_numIndices = numIndices - prim.m_startIndex;
            bx::strlcpy(prim.m_name, groupIt->m_name.c_str(), 128);
            primitives.push_back(prim);
            prim.m_startVertex = numVertices;
            prim.m_startIndex = numIndices;
        }

        //CS_PRINT("%3d: s %5d, n %5d, %s\n"
        //    , ii
        //    , groupIt->m_startTriangle
        //    , groupIt->m_numTriangles
        //    , groupIt->m_material.c_str()
        //    );
    }

    if (0 < primitives.size() )
    {
        triReorderElapsed -= bx::getHPCounter();
        for (BgfxPrimitiveArray::const_iterator primIt = primitives.begin(); primIt != primitives.end(); ++primIt)
        {
            const Primitive& prim = *primIt;
            triangleReorder(indexData + prim.m_startIndex, prim.m_numIndices, numVertices, 32);
        }
        triReorderElapsed += bx::getHPCounter();

        if (_hasTangent)
        {
            calculateTangents(vertexData, numVertices, decl, indexData, numIndices);
        }

        write(_writer, vertexData, numVertices, decl, indexData, numIndices, material.c_str(), primitives.data(), (uint32_t)primitives.size());
    }

    delete [] indexData;
    delete [] vertexData;

    now = bx::getHPCounter();
    convertElapsed += now;

    const int64_t end = _writer->seek();
    const uint32_t dataSize = uint32_t(end-begin);
    CS_PRINT("size: %u\n", dataSize);

    CS_PRINT("parse %f [s]\ntri reorder %f [s]\nconvert %f [s]\n# %d, g %d, p %d, v %d, i %d\n"
           , double(parseElapsed)/bx::getHPFrequency()
           , double(triReorderElapsed)/bx::getHPFrequency()
           , double(convertElapsed)/bx::getHPFrequency()
           , num
           , uint32_t(groups.size() )
           , numPrimitives
           , numVertices
           , numIndices
           );

    return dataSize;
}

uint32_t objToBin(const char* _filePath
                , bx::WriterSeekerI* _writer
                , uint32_t _packUv
                , uint32_t _packNormal
                , bool _ccw
                , bool _flipV
                , bool _hasTangent
                , float _scale
                )
{
    FILE* file = fopen(_filePath, "rb");
    if (NULL == file)
    {
        CS_PRINT("Unable to open input file '%s'.", _filePath);
        return 0;
    }

    uint32_t size = (uint32_t)dm::fsize(file);
    char* data = new char[size+1];
    size = (uint32_t)fread(data, 1, size, file);
    data[size] = '\0';
    fclose(file);

    const uint32_t dataSize = objToBin((uint8_t*)data, _writer, _packUv, _packNormal, _ccw, _flipV, _hasTangent, _scale);

    delete [] data;

    return dataSize;
}

uint32_t objToBin(const char* _filePath
                , void*& _outData
                , uint32_t& _outDataSize
                , bx::ReallocatorI* _allocator
                , uint32_t _packUv
                , uint32_t _packNormal
                , bool _ccw
                , bool _flipV
                , bool _hasTangent
                , float _scale
                )
{
    enum { InitialDataSize = DM_MEGABYTES(200) };

    DynamicMemoryBlockWriter writer(_allocator, InitialDataSize);
    const uint32_t dataSize = objToBin(_filePath, &writer, _packUv, _packNormal, _ccw, _flipV, _hasTangent, _scale);

    _outData     = writer.getData();
    _outDataSize = writer.getDataSize();

    return dataSize;
}

uint32_t objToBin(const uint8_t* _objData
                , void*& _outData
                , uint32_t& _outDataSize
                , bx::ReallocatorI* _allocator
                , uint32_t _packUv
                , uint32_t _packNormal
                , bool _ccw
                , bool _flipV
                , bool _hasTangent
                , float _scale
                )
{
    enum { InitialDataSize = DM_MEGABYTES(200) };

    DynamicMemoryBlockWriter writer(_allocator, InitialDataSize);
    const uint32_t dataSize = objToBin(_objData, &writer, _packUv, _packNormal, _ccw, _flipV, _hasTangent, _scale);

    _outData     = writer.getData();
    _outDataSize = writer.getDataSize();

    return dataSize;
}

/* vim: set sw=4 ts=4 expandtab: */
