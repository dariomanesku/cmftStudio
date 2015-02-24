/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

// This is left for debugging purposes !

// Textures.
//-----

#if !defined(TEX_DESC)
    #define TEX_DESC(_name, _path)
#endif //!defined(TEX_DESC)
//TEX_DESC( FieldstoneAlbedo, "textures/fieldstone-rgba.dds" )
//TEX_DESC( FieldstoneNormal, "textures/fieldstone-n.dds"    )
//TEX_DESC( StonesAlbedo,     "textures/stones-rgba.dds"     )
//TEX_DESC( StonesNormal,     "textures/stones-n.dds"        )
//TEX_DESC( MetalAlbedo,      "textures/metal-rgba.dds"      )
//TEX_DESC( MetalNormal,      "textures/metal-n.dds"         )
//TEX_DESC( CerberusAlbedo,   "textures/Cerberus_A.dds"      )
//TEX_DESC( CerberusNormal,   "textures/Cerberus_N.dds"      )
//TEX_DESC( CerberusRMAC,     "textures/Cerberus_RMAC.dds"   )
//TEX_DESC( GrenadeAlbedo,    "textures/Grenade_A.dds"       )
//TEX_DESC( GrenadeNormal,    "textures/Grenade_N.dds"       )
//TEX_DESC( GrenadeGAM,       "textures/Grenade_GAM.dds"     )
//TEX_DESC( GrenadeGlow,      "textures/Grenade_Glow.dds"    )
//TEX_DESC( FlintlockAlbedo,  "textures/Flintlock_A.dds"     )
//TEX_DESC( FlintlockNormal,  "textures/Flintlock_N.dds"     )
//TEX_DESC( FlintlockMAR,     "textures/Flintlock_MAR.dds"   )
//TEX_DESC( OrkAlbedo,        "textures/ork_albedo.dds"      )
//TEX_DESC( OrkNormal,        "textures/ork_normal.dds"      )
//TEX_DESC( OrkSpecular,      "textures/ork_specular.dds"    )
//TEX_DESC( OrkGloss,         "textures/ork_gloss.dds"       )
//TEX_DESC( OrkAo,            "textures/ork_ao.dds"          )
#undef TEX_DESC

// Materials.
//-----

#if !defined(MAT_DESC)
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
                   )
#endif //!defined(MAT_DESC)

//MAT_DESC(MatDefault
//       , 0.6f, 0.6f, 0.6f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 0.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 1.0f, 0.0f, 1.0f, 0.0f //glossNormal
//       , 1.0f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 0.0f, 1.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 1.0f, 0.0f, 0.0f //swizReflectivity
//       , 0.0f, 1.0f, 0.0f, 0.0f //swizOcclusion
//       , Invalid // texAlbedo
//       , Invalid // texNormal
//       , Invalid // texSurface
//       , Invalid // texReflectivity
//       , Invalid // texAmbientOcclusion
//       , Invalid // texEmissive
//       )
//MAT_DESC(Ork
//       , 0.6f, 0.6f, 0.6f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 0.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 1.0f, 0.0f, 1.0f, 0.0f //glossNormal
//       , 1.0f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizReflectivity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizOcclusion
//       , OrkAlbedo // texAlbedo
//       , OrkNormal // texNormal
//       , OrkSpecular // texSurface
//       , OrkGloss // texReflectivity
//       , OrkAo // texAmbientOcclusion
//       , Invalid // texEmissive
//       )
//MAT_DESC(Fieldstone
//       , 1.0f, 1.0f, 1.0f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 0.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 0.7f, 0.0f, 1.0f, 0.0f //glossNormal
//       , 0.5f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizReflectivity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizOcclusion
//       , FieldstoneAlbedo // texAlbedo
//       , FieldstoneNormal // texNormal
//       , Invalid          // texSurface
//       , Invalid          // texReflectivity
//       , Invalid          // texAmbientOcclusion
//       , Invalid          // texEmissive
//       )
//MAT_DESC(Stones
//       , 1.0f, 1.0f, 1.0f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 0.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 0.7f, 0.0f, 1.0f, 0.0f //glossNormal
//       , 0.5f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizReflectivity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizOcclusion
//       , StonesAlbedo // texAlbedo
//       , StonesNormal // texNormal
//       , Invalid      // texSurface
//       , Invalid      // texReflectivity
//       , Invalid      // texAmbientOcclusion
//       , Invalid      // texEmissive
//       )
//MAT_DESC(Metal
//       , 1.0f, 1.0f, 1.0f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 0.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 0.7f, 0.0f, 1.0f, 0.0f //glossNormal
//       , 0.5f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizReflectivity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizOcclusion
//       , MetalAlbedo // texAlbedo
//       , MetalNormal // texNormal
//       , Invalid     // texSurface
//       , Invalid     // texReflectivity
//       , Invalid     // texAmbientOcclusion
//       , Invalid     // texEmissive
//       )
//MAT_DESC(MatCerberus
//       , 1.0f, 1.0f, 1.0f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 1.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 1.0f, 1.0f, 1.0f, 0.0f //glossNormal
//       , 1.0f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.3f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 1.0f, 0.0f, 0.0f //swizReflectivity
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizOcclusion
//       , CerberusAlbedo // texAlbedo
//       , CerberusNormal // texNormal
//       , CerberusRMAC   // texSurface
//       , CerberusRMAC   // texReflectivity
//       , CerberusRMAC   // texAmbientOcclusion
//       , Invalid        // texEmissive
//       )
//MAT_DESC(MatGrenade
//       , 1.0f, 1.0f, 1.0f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 1.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 0.6f, 1.0f, 1.0f, 1.0f //glossNormal
//       , 1.0f, 0.0f, 1.0f, 1.2f //surface
//       , 1.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 2.5f, 0.0f //ao/emissiveIntensity
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizSurface
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizReflectivity
//       , 0.0f, 1.0f, 0.0f, 0.0f //swizOcclusion
//       , GrenadeAlbedo // texAlbedo
//       , GrenadeNormal // texNormal
//       , GrenadeGAM    // texSurface
//       , GrenadeGAM    // texReflectivity
//       , GrenadeGAM    // texAmbientOcclusion
//       , GrenadeGlow   // texEmissive
//       )
//MAT_DESC(Flintlock
//       , 1.0f, 1.0f, 1.0f, 0.0f //albedo
//       , 1.0f, 1.0f, 1.0f, 1.0f //specular
//       , 1.0f, 1.0f, 1.0f, 0.0f //emissive
//       , 1.0f, 1.0f, 1.0f, 1.0f //glossNormal
//       , 1.0f, 0.0f, 1.0f, 1.2f //surface
//       , 0.0f, 0.0f, 1.0f, 0.0f //misc
//       , 0.0f, 0.0f, 1.0f, 0.0f //ao/emissiveIntensity
//       , 0.0f, 0.0f, 1.0f, 0.0f //swizSurface
//       , 1.0f, 0.0f, 0.0f, 0.0f //swizReflectivity
//       , 0.0f, 1.0f, 0.0f, 0.0f //swizOcclusion
//       , FlintlockAlbedo // texAlbedo
//       , FlintlockNormal // texNormal
//       , FlintlockMAR    // texSurface
//       , FlintlockMAR    // texReflectivity
//       , FlintlockMAR    // texAmbientOcclusion
//       , Invalid         // texEmissive
//       )
#undef MAT_DESC

// Environments.
//-----

#if !defined(ENVMAP_DESC)
    #define ENVMAP_DESC(_name, _skybox, _pmrem, _iem)
#endif //!defined(ENVMAP_DESC)
//ENVMAP_DESC( Wells     , "textures/wells_skybox.dds"     , "textures/wells_pmrem.dds"     , "textures/wells_iem.dds"     )
//ENVMAP_DESC( Bolonga   , "textures/bolonga_skybox.dds"   , "textures/bolonga_pmrem.dds"   , "textures/bolonga_iem.dds"   )
//ENVMAP_DESC( Uffizi    , "textures/uffizi.dds"           , "textures/uffizi.dds"          , "textures/uffizi.dds"        )
//ENVMAP_DESC( Shack     , "textures/shack_skybox.dds"     , "textures/shack_pmrem.dds"     , "textures/shack_iem.dds"     )
//ENVMAP_DESC( Okretnica , "textures/okretnica_skybox.dds" , "textures/okretnica_pmrem.dds" , "textures/okretnica_iem.dds" )

//ENVMAP_DESC( CharlesRiver , "textures/charlesriver_skybox.dds" , "textures/charlesriver_pmrem.dds" , "textures/charlesriver_iem.dds" )
//ENVMAP_DESC( Alley        , "textures/alley_skybox.dds"        , "textures/alley_pmrem.dds"        , "textures/alley_iem.dds"        )
//ENVMAP_DESC( Milkyway     , "textures/milkyway_skybox.dds"     , "textures/milkyway_pmrem.dds"     , "textures/milkyway_iem.dds"     )

//ENVMAP_DESC( WinterForest , "textures/winterforest_skybox.dds" , "textures/winterforest_pmrem.dds" , "textures/winterforest_iem.dds" )
//ENVMAP_DESC( Cathedral    , "textures/cathedral_skybox.dds"    , "textures/cathedral_pmrem.dds"    , "textures/cathedral_iem.dds"    )
//ENVMAP_DESC( Sunset       , "textures/sunset_skybox.dds"       , "textures/sunset_pmrem.dds"       , "textures/sunset_iem.dds"       )
#undef ENVMAP_DESC

// Meshes.
//-----

#if !defined(MESH_DESC)
    #define MESH_DESC(_name, _path)
#endif //!defined(MESH_DESC)
//MESH_DESC( Sphere    , "meshes/sphere.bin"             )
//MESH_DESC( Grenade   , "meshes/grenade.bin"            )
//MESH_DESC( Flintlock , "meshes/flintlock.bin"          )
//MESH_DESC( Cerberus  , "meshes/cerberus_centered2.bin" )
//MESH_DESC( Ork       , "meshes/ork.bin"                )
#undef MESH_DESC

// Instances.
//-----

#if !defined(INST_DESC)
    #define INST_DESC(_name, _mesh, _mat0, _mat1, _mat2, _scale, _posx, _posy, _posz, _rotx, _roty, _rotz)
#endif //!defined(INST_DESC)
//INST_DESC( Cerberus , Cerberus    , MatCerberus , MatCerberus , Invalid , 2.0f , 0.0f , 0.0f , 0.0f , 0.0f , 0.0f , 0.0f )
//INST_DESC( Grenade  , Grenade     , MatGrenade  , Invalid     , Invalid , 1.0f , 0.0f , 0.0f , 0.0f , 0.0f , 0.0f , 0.0f )
//INST_DESC( Sphere   , Sphere      , MatDefault  , Invalid     , Invalid , 1.0f , 0.0f , 0.0f , 0.0f , 0.0f , 0.0f , 0.0f )
#undef INST_DESC

/* vim: set sw=4 ts=4 expandtab: */
