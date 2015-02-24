$input v_dir

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"

SAMPLERCUBE(u_texSkybox, 5); // Destination skybox.
SAMPLERCUBE(u_texIem,    7); // Source skybox.

void main()
{
    vec3 dirSrc = normalize(v_dir);
    vec3 dirDst = dirSrc;

    if (0.0 != u_prevEdgeFixup) { dirSrc = fixCubeLookup(dirSrc, u_prevLod, u_prevMipSize); }
    if (0.0 != u_edgeFixup)     { dirDst = fixCubeLookup(dirDst, u_lod,     u_mipSize); }

    vec3 src = toLinear(textureCubeLod(u_texIem,    dirSrc, u_prevLod).xyz);
    vec3 dst = toLinear(textureCubeLod(u_texSkybox, dirDst, u_lod).xyz);

    float progress = clamp(u_skyboxTransition, 0.0, 1.0);

    vec3 color = mix(src, dst, progress);

    gl_FragColor = encodeRGBE8(color);
}

/* vim: set sw=4 ts=4 expandtab: */
