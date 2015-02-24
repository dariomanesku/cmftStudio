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
    vec3 rgb0  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets0));
    vec3 rgb1  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets1));
    vec3 rgb2  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets2));
    vec3 rgb3  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets3));
    vec3 rgb4  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets4));
    vec3 rgb5  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets5));
    vec3 rgb6  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets6));
    vec3 rgb7  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets7));
    vec3 rgb8  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets8));
    vec3 rgb9  = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets9));
    vec3 rgb10 = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets10));
    vec3 rgb11 = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets11));
    vec3 rgb12 = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets12));
    vec3 rgb13 = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets13));
    vec3 rgb14 = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets14));
    vec3 rgb15 = decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets15));
    float avg = log(lumf(rgb0)  + 0.00001)
              + log(lumf(rgb1)  + 0.00001)
              + log(lumf(rgb2)  + 0.00001)
              + log(lumf(rgb3)  + 0.00001)
              + log(lumf(rgb4)  + 0.00001)
              + log(lumf(rgb5)  + 0.00001)
              + log(lumf(rgb6)  + 0.00001)
              + log(lumf(rgb7)  + 0.00001)
              + log(lumf(rgb8)  + 0.00001)
              + log(lumf(rgb9)  + 0.00001)
              + log(lumf(rgb10) + 0.00001)
              + log(lumf(rgb11) + 0.00001)
              + log(lumf(rgb12) + 0.00001)
              + log(lumf(rgb13) + 0.00001)
              + log(lumf(rgb14) + 0.00001)
              + log(lumf(rgb15) + 0.00001)
              ;
    avg = exp(avg*(1.0/16.0));

    gl_FragColor = encodeRE8(avg);
}

/* vim: set sw=4 ts=4 expandtab: */
