/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_APPCONFIG_H_HEADER_GUARD
#define CMFTSTUDIO_APPCONFIG_H_HEADER_GUARD

// Check.
//-----

// Available options:
//     -DM_CHECK_CONFIG_NOOP
//     -DM_CHECK_CONFIG_PRINT
//     -DM_CHECK_CONFIG_DEBUG_BREAK
#define DM_CHECK_CONFIG DM_CHECK_CONFIG_NOOP
#include <dm/check.h>
#define CS_CHECK DM_CHECK

// Allocator.
//-----

#define DM_ALLOCATOR 1 // Using 0 here makes implementation fallback to C-runtime allocator functions.
#define DM_NATURAL_ALIGNMENT 16

#define DM_ALLOC_PRINT_STATS  0
#define DM_ALLOC_PRINT_USAGE  0
#define DM_ALLOC_PRINT_STATIC 0
#define DM_ALLOC_PRINT_SMALL  0
#define DM_ALLOC_PRINT_STACK  0
#define DM_ALLOC_PRINT_HEAP   0
#define DM_ALLOC_PRINT_EXT    0
#define DM_ALLOC_PRINT_BGFX   0

#define DM_ALLOC_PRINT_FILELINE 0

// Allocator overrides.
//-----

#define CS_OVERRIDE_NEWDELETE         1
#define CS_OVERRIDE_TINYSTL_ALLOCATOR 1
#define CS_OVERRIDE_STBI_ALLOCATOR    1
#define CS_OBJTOBIN_USES_TINYSTL      1 // 1 == tinystl, 0 == std containers.

// Resources.
//-----

#define CS_MAX_LIGHTS         6
#define CS_MAX_TEXTURES     128
#define CS_MAX_MATERIALS    256
#define CS_MAX_MESHES        16
#define CS_MAX_ENVIRONMENTS  32
#define CS_MAX_MESHINSTANCES 32

#define CS_MAX_GEOMETRY_LOADERS 8

// Shaders.
//-----

// Load shaders from:
//   0 - local files
//   1 - executable's data segment (headers are generated with '/src/shaders/makefile_headers')
#define CS_LOAD_SHADERS_FROM_DATA_SEGMENT 1

#endif // CMFTSTUDIO_APPCONFIG_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
