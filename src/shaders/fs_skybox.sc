$input v_dir

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"

SAMPLERCUBE(u_texSkybox, 5);

void main()
{
    vec3 dir = normalize(v_dir);

    if (0.0 != u_edgeFixup)
    {
        dir = fixCubeLookup(dir, u_lod, u_mipSize);
    }

    vec3 color = toLinear(textureCubeLod(u_texSkybox, dir, u_lod).xyz);

    gl_FragColor = encodeRGBE8(color);
}

/* vim: set sw=4 ts=4 expandtab: */
