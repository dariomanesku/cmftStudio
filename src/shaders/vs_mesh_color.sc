$input a_position, a_normal
$output v_view, v_normal

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );

    vec4 normal = a_normal * 2.0 - 1.0;
    v_normal = mul(u_model[0], vec4(normal.xyz, 0.0) ).xyz;
    v_view = normalize(u_camPos - mul(u_model[0], vec4(a_position, 1.0)).xyz);
}
