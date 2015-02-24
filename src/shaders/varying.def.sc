vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec4 v_texcoord1 : TEXCOORD1 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_texcoord2 : TEXCOORD2 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_texcoord3 : TEXCOORD3 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_texcoord4 : TEXCOORD4 = vec4(0.0, 0.0, 0.0, 0.0);
vec3 v_dir       : TEXCOORD5 = vec3(0.0, 0.0, 0.0);
vec3 v_view      : TEXCOORD6 = vec3(0.0, 0.0, 0.0);
vec3 v_normal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec3 v_tangent   : TANGENT   = vec3(1.0, 0.0, 0.0);
vec3 v_bitangent : BINORMAL  = vec3(0.0, 1.0, 0.0);

vec4 v_normal4   : NORMAL    = vec4(0.0, 0.0, 1.0, 1.0);
vec4 v_tangent4  : TANGENT   = vec4(1.0, 0.0, 0.0, 1.0);

vec3 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
vec4 a_normal    : NORMAL;
vec4 a_tangent   : TANGENT;
