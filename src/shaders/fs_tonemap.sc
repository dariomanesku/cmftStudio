$input v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "tonemap.shdr"
#include "utils.shdr"

SAMPLER2D(u_texColor, 0);
SAMPLER2D(u_texLum,   1);
SAMPLER2D(u_texBlur,  2);

void main()
{
    vec3 rgb = decodeRGBE8(texture2D(u_texColor, v_texcoord0));
    float lumAvg;
    if (1.0 == u_doLightAdapt)
    {
        lumAvg = clamp(decodeRE8(texture2D(u_texLum, v_texcoord0)), 0.1, 1.0);
    }
    else
    {
        lumAvg = 0.18;
    }

    float middleGray = u_middleGray;

    float lumRgb = lumf(rgb);
    float lumScale = lumRgb * middleGray / (lumAvg+0.0001);
    rgb *= (lumScale / lumRgb);

    rgb = tonemap(rgb, u_whiteSqr);

    // Bloom.
    if (1.0 == u_doBloom)
    {
        // Vertical blur.
        vec4 blur = blur9(u_texBlur
                        , v_texcoord0
                        , v_texcoord1
                        , v_texcoord2
                        , v_texcoord3
                        , v_texcoord4
                        , u_weight0
                        , u_weight1
                        , u_weight2
                        , u_weight3
                        , u_weight4
                        );

        rgb += u_blurCoeff * blur.xyz;
    }

    // Apply exposure.
    rgb *= exp2(u_exposure);

    // Apply contrast.
    rgb = mix(vec3_splat(0.18), rgb, u_contrast);

    // Apply brightness.
    rgb = max(vec3_splat(0.0), rgb+u_brightness);

    // Apply gamma.
    rgb = pow(rgb, vec3_splat(u_gamma));

    // Apply saturation.
    rgb = mix(luma(rgb), rgb, u_saturation);

    // Vignette.
    vec2 coord = (gl_FragCoord.xy * u_viewTexel.xy)*2.0-1.0;
    coord.y *= 0.35;
    float dist = distance(vec2_splat(0.0), coord)*0.5;
    float vignette = (1.0 - dist*dist*u_vignette);
    rgb *= vignette;

    gl_FragColor.xyz = rgb;
    gl_FragColor.w = 1.0;
}

/* vim: set sw=4 ts=4 expandtab: */
