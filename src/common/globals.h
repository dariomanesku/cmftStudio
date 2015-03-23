/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_GLOBALS_H_HEADER_GUARD
#define CMFTSTUDIO_GLOBALS_H_HEADER_GUARD

#include <stdint.h>

extern uint16_t g_versionMajor;
extern uint16_t g_versionMinor;

extern float    g_texelHalf;        // 0.5f on DX9, 0.0f elsewhere.
extern bool     g_originBottomLeft; // false for DX9/DX11, true for OpenGL.
extern uint32_t g_frameNum;
extern uint32_t g_width;
extern uint32_t g_height;
extern uint32_t g_guiWidth;
extern uint32_t g_guiHeight;
extern float    g_widthf;
extern float    g_heightf;

#endif // CMFTSTUDIO_GLOBALS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
