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
    vec4 tex = texture2D(u_texColor, v_texcoord0);

    float stroke = tex.b;
    float sun    = tex.r * clamp(u_rgba.a, 0.0, 1.0);
    float bloom  = tex.g * clamp(u_rgba.a-1.0, 0.0, 5.0)*0.2;

    vec3 color = u_rgba.rgb*(sun + bloom - stroke*8.0*u_selectedLight);
    float alpha = max(sun, bloom-0.1) + stroke;

    gl_FragColor.rgb = color;
    gl_FragColor.a   = clamp(alpha, 0.0, 1.0);
}

/* vim: set sw=4 ts=4 expandtab: */
