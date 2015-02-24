$input v_texcoord0

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"

SAMPLER2D(u_texColor, 0);

void main()
{
    float sum;
    sum  = decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets0));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets1));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets2));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets3));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets4));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets5));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets6));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets7));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets8));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets9));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets10));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets11));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets12));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets13));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets14));
    sum += decodeRE8(texture2D(u_texColor, v_texcoord0+u_offsets15));
    float avg = sum*(1.0/16.0);
    gl_FragColor = encodeRE8(avg);
}

/* vim: set sw=4 ts=4 expandtab: */
