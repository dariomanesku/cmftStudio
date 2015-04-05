/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_LOADERS_H_HEADER_GUARD
#define CMFTSTUDIO_LOADERS_H_HEADER_GUARD

#include "../common/common.h"
#include "loadermanager.h"

#include "loader_bgfxbin.h"
#include "loader_obj.h"

static inline void initGeometryLoaders()
{
    // Initialize geometry loaders.
    geometryLoaderRegister("Wavefront obj",   "obj", loaderObj);
    geometryLoaderRegister("Bgfx bin loader", "bin", loaderBgfxBin);
}

#endif // CMFTSTUDIO_LOADERS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
