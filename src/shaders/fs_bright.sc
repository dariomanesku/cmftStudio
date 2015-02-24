$input v_texcoord0

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"
#include "tonemap.shdr"

SAMPLER2D(u_texColor, 0);
SAMPLER2D(u_texLum, 1);

void main()
{
    float lumAvg;
    if (1.0 == u_doLightAdapt)
    {
        lumAvg = clamp(decodeRE8(texture2D(u_texLum, v_texcoord0)), 0.1, 1.0);
    }
    else
    {
        lumAvg = 0.18;
    }
    //float middleGray = 1.03 - 2.0/(2.0+log10(lumAvg+1.0));
    float middleGray = u_middleGray;

    vec3 rgb = vec3(0.0, 0.0, 0.0);
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets0));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets1));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets2));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets3));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets4));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets5));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets6));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets7));
    rgb += decodeRGBE8(texture2D(u_texColor, v_texcoord0+u_offsets8));
    rgb *= 1.0/9.0;

    float lumRgb = lumf(rgb);
    float lumScale = lumRgb * middleGray / (lumAvg+0.0001);
    float lumScale2 = max(0.0, lumScale*(1.0+lumScale/u_whiteSqr)-u_treshold);
    rgb *= (lumScale2/lumRgb);

    rgb = tonemap(rgb, u_whiteSqr);

    gl_FragColor = vec4(rgb, 1.0);
}

/* vim: set sw=4 ts=4 expandtab: */
