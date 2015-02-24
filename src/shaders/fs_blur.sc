$input v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"
#include "utils.shdr"

SAMPLER2D(u_texColor, 0);

void main()
{
    gl_FragColor = blur9(u_texColor
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
}

/* vim: set sw=4 ts=4 expandtab: */
