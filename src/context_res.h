/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#if !defined(TEXUNI_DESC)
    #define TEXUNI_DESC(_enum, _stage, _name)
#endif //!defined(TEXUNI_DESC)
TEXUNI_DESC( PmremPrev,    4, "u_texPmremPrev"    )
TEXUNI_DESC( Skybox,       5, "u_texSkybox"       )
TEXUNI_DESC( Pmrem,        6, "u_texPmrem"        )
TEXUNI_DESC( Iem,          7, "u_texIem"          )
TEXUNI_DESC( Lum,          1, "u_texLum"          )
TEXUNI_DESC( Blur,         2, "u_texBlur"         )
TEXUNI_DESC( Color,        0, "u_texColor"        )
TEXUNI_DESC( Normal,       1, "u_texNormal"       )
TEXUNI_DESC( Surface,      2, "u_texSurface"      )
TEXUNI_DESC( Reflectivity, 3, "u_texReflectivity" )
TEXUNI_DESC( Occlusion,    8, "u_texAO"           )
TEXUNI_DESC( Emissive,     9, "u_texEmissive"     )
#undef TEXUNI_DESC

#if !defined(PROG_DESC)
    #define PROG_DESC(_name, _vs, _fs)
#endif //!defined(PROG_DESC)
PROG_DESC( Mesh                 , vs_mesh           , fs_mesh                    )
PROG_DESC( MeshRgbe8            , vs_mesh           , fs_mesh_rgbe8              )
PROG_DESC( MeshRgbe8Trans       , vs_mesh           , fs_mesh_rgbe8_trans        )
PROG_DESC( MeshNormal           , vs_mesh_normal    , fs_mesh_normal             )
PROG_DESC( MeshNormalRgbe8      , vs_mesh_normal    , fs_mesh_normal_rgbe8       )
PROG_DESC( MeshNormalRgbe8Trans , vs_mesh_normal    , fs_mesh_normal_rgbe8_trans )
PROG_DESC( Sky                  , vs_skybox         , fs_skybox                  )
PROG_DESC( SkyTrans             , vs_skybox         , fs_skybox_trans            )
PROG_DESC( Tonemap              , vs_tonemap        , fs_tonemap                 )
PROG_DESC( Material             , vs_material       , fs_mesh                    )
PROG_DESC( MaterialNormal       , vs_material_normal, fs_mesh_normal             )
PROG_DESC( Blur                 , vs_blur           , fs_blur                    )
PROG_DESC( Color                , vs_plain          , fs_color                   )
PROG_DESC( Overlay              , vs_plain          , fs_overlay                 )
PROG_DESC( Wireframe            , vs_plain          , fs_wireframe               )
PROG_DESC( Bright               , vs_texcoord       , fs_bright                  )
PROG_DESC( Lum                  , vs_texcoord       , fs_lum                     )
PROG_DESC( LumAvg               , vs_texcoord       , fs_lumavg                  )
PROG_DESC( LumDownscale         , vs_texcoord       , fs_lumdownscale            )
PROG_DESC( Image                , vs_texcoord       , fs_image                   )
PROG_DESC( Latlong              , vs_texcoord       , fs_latlong                 )
PROG_DESC( ImageRe8             , vs_texcoord       , fs_image_re8               )
PROG_DESC( SunIcon              , vs_texcoord       , fs_sun_icon                )
PROG_DESC( CubemapTonemap       , vs_texcoord       , fs_cubemap_tonemap         )
PROG_DESC( Equals               , vs_texcoord       , fs_equals                  )
PROG_DESC( Fxaa                 , vs_texcoord       , fs_fxaa                    )
#undef PROG_DESC

#if !defined(PROG_NORM)
    #define PROG_NORM(_prog, _normal)
#endif //!defined(PROG_NORM)
PROG_NORM( Mesh           , MeshNormal           )
PROG_NORM( MeshRgbe8      , MeshNormalRgbe8      )
PROG_NORM( MeshRgbe8Trans , MeshNormalRgbe8Trans )
PROG_NORM( Material       , MaterialNormal       )
#undef PROG_NORM

#if !defined(PROG_TRANS)
    #define PROG_TRANS(_prog, _trans)
#endif //!defined(PROG_TRANS)
PROG_TRANS( MeshRgbe8       , MeshRgbe8Trans       )
PROG_TRANS( MeshNormalRgbe8 , MeshNormalRgbe8Trans )
#undef PROG_TRANS

/* vim: set sw=4 ts=4 expandtab: */
