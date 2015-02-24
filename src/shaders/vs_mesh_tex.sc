$input a_position, a_normal, a_tangent, a_texcoord0
$output v_view, v_normal4, v_tangent4, v_bitangent, v_texcoord0

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "shader.shdr"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );

    vec4 normal  = a_normal  * 2.0 - 1.0;
    vec4 tangent = a_tangent;
    tangent.xyz = tangent.xyz * 2.0 - 1.0;

    //TODO
    //vec3 viewNormal  = normalize(mul(u_model[0], vec4(normal.xyz,  0.0)).xyz);
    //vec3 viewTangent = normalize(mul(u_model[0], vec4(tangent.xyz, 0.0)).xyz);
    //vec3 viewBitangent = cross(viewNormal, viewTangent) * tangent.w;
    //mat3 tbn = mat3(viewTangent, viewBitangent, viewNormal);

    v_view = normalize(u_camPos - mul(u_model[0], vec4(a_position, 1.0)).xyz);

    //v_normal = viewNormal;
    //v_tangent = viewTangent;
    v_normal4 = normal;
    v_tangent4 = tangent;
    v_bitangent = vec3_splat(1.0);
    v_texcoord0 = a_texcoord0;
}

/* vim: set sw=4 ts=4 expandtab: */
