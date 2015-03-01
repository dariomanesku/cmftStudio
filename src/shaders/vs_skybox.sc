$input a_position, a_texcoord0
$output v_dir

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );

    float height = tan(u_fov*0.5);
    float aspect = height*(4.0/3.0);

    vec2 tex = (2.0*a_texcoord0-1.0) * vec2(aspect, height);

    mat4 mtx;
    mtx[0] = u_mtx0;
    mtx[1] = u_mtx1;
    mtx[2] = u_mtx2;
    mtx[3] = u_mtx3;
    v_dir = instMul(mtx, vec4(tex, 1.0, 0.0) ).xyz;
}

/* vim: set sw=4 ts=4 expandtab: */
