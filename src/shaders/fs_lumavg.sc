$input v_texcoord0

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"

SAMPLER2D(u_texColor, 0);
SAMPLER2D(u_texLum, 1);

void main()
{
    float last = decodeRE8(texture2D(u_texLum, v_texcoord0));

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
    float curr = sum*(1.0/16.0);

    float updateSpeed;
    if (0.1 < u_skyboxTransition && u_skyboxTransition < 0.95)
    {
        updateSpeed = mix(0.1, u_time, u_skyboxTransition);
    }
    else
    {
        updateSpeed = u_time;
    }

    float adapted = last + min(0.01, (curr - last)*updateSpeed);

    gl_FragColor = encodeRE8(adapted);
}

/* vim: set sw=4 ts=4 expandtab: */
