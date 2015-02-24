$input v_texcoord0

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"

SAMPLERCUBE(u_texSkybox, 5);

void main()
{
    vec3 dir = vecFromLatLong(v_texcoord0);
    vec3 color = textureCube(u_texSkybox, dir).xyz;

    // Apply gamma.
    vec3 gamma = vec3_splat(u_tonemapGamma);
    color = pow(abs(color), gamma);

    // Normalize.
    color = (color-u_tonemapMinLum)/u_tonemapLumRange;

    gl_FragColor.xyz = color;
    gl_FragColor.w = 1.0;
}

/* vim: set sw=4 ts=4 expandtab: */
