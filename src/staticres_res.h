/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

// Resource headers.
//-----

#ifdef SR_INCLUDE
    #include "res/sphere.h"
    #include "res/sphereSurfaceTex.h"
    #include "res/loading_screen.h"
    #include "res/sun_icon.h"
    #include "res/logo_skybox.h"
    #include "res/logo_pmrem.h"
    #include "res/logo_iem.h"
    #include "res/brick_n.h"
    #include "res/brick_ao.h"
    #include "res/stripes_s.h"
    #include "res/droidsans.h"
    #include "res/droidsansmono.h"
    #undef SR_INCLUDE
#endif //SR_INCLUDE

// Uncompressed resources.
//-----

#ifndef SR_DESC
    #define SR_DESC(_name, _dataArray)
#endif //!defined(SR_DESC)
SR_DESC(sphereMesh, sc_sphereMesh)
#undef SR_DESC

// Compressed resources.
//-----

#ifndef SR_DESC_COMPR
    #define SR_DESC_COMPR(_name, _dataArray)
#endif //!defined(SR_DESC_COMPR)
// Textures:
SR_DESC_COMPR(loadingScreenTex , sc_loadingScreenCompressed)
SR_DESC_COMPR(sphereSurfaceTex , sc_sphereSurfaceTexCompressed)
SR_DESC_COMPR(sunIcon          , sc_sunIconCompressed)
SR_DESC_COMPR(bricksN          , sc_brickNCompressed)
SR_DESC_COMPR(bricksAo         , sc_brickAoCompressed)
SR_DESC_COMPR(stripesS         , sc_stripesSCompressed)
// Cubemaps:
SR_DESC_COMPR(logoSkybox , sc_logoSkyboxCompressed)
SR_DESC_COMPR(logoPmrem  , sc_logoPmremCompressed)
SR_DESC_COMPR(logoIem    , sc_logoIemCompressed)
// Fonts:
SR_DESC_COMPR(droidSans     , sc_droidSansCompressed)
SR_DESC_COMPR(droidSansMono , sc_droidSansMonoCompressed)
#undef SR_DESC_COMPR

/* vim: set sw=4 ts=4 expandtab: */

