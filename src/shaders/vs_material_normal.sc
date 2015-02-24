$input a_position, a_normal, a_tangent, a_texcoord0
$output v_view, v_normal, v_tangent, v_bitangent, v_texcoord0

/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#define CameraPos u_matCam

#define NORMAL_MAP
#include "vs_mesh.shdr"
