/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_APPCONFIG_H_HEADER_GUARD
#define CMFTSTUDIO_APPCONFIG_H_HEADER_GUARD

// Check.
//-----

#define DM_CHECK_CONFIG DM_CHECK_CONFIG_NOOP
#include <dm/check.h>
#define CS_CHECK DM_CHECK

// Allocator.
//-----

#define CS_ALLOC_PRINT_STATS  0
#define CS_ALLOC_PRINT_USAGE  0
#define CS_ALLOC_PRINT_STATIC 0
#define CS_ALLOC_PRINT_SMALL  0
#define CS_ALLOC_PRINT_STACK  0
#define CS_ALLOC_PRINT_HEAP   0
#define CS_ALLOC_PRINT_EXT    0
#define CS_ALLOC_PRINT_BGFX   0

#define CS_ALLOC_PRINT_FILELINE 0

#define IMGUI_CONFIG_CUSTOM_ALLOCATOR 1

#define CS_NATURAL_ALIGNMENT 16

// Bgfx.
//-----

#define BGFX_CONFIG_RENDERER_DIRECT3D9  1
#define BGFX_CONFIG_RENDERER_DIRECT3D11 1
#define BGFX_CONFIG_RENDERER_OPENGL     21

// Resources.
//-----

#define CS_MAX_LIGHTS        6
#define CS_MAX_TEXTURES    128
#define CS_MAX_MATERIALS   128
#define CS_MAX_MESHES       16
#define CS_MAX_MESH_GROUPS  16
#define CS_MAX_ENVMAPS      32

// Shaders.
//-----

// Load shaders from:
//   0 - local files
//   1 - executable's data segment (headers are generated with '/src/shaders/makefile')
#define CS_LOAD_SHADERS_FROM_DATA_SEGMENT 1

#endif // CMFTSTUDIO_APPCONFIG_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
