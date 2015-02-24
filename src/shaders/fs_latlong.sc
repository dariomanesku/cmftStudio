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
    vec3 color = textureCubeLod(u_texSkybox, dir, u_lod).xyz;
    float alpha = 0.2 + 0.8*u_enabled;

    gl_FragColor = vec4(color, alpha);
}

/* vim: set sw=4 ts=4 expandtab: */
