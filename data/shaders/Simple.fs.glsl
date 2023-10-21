#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 v_uv;

layout (location = 0) out vec4 o_color;

layout(set = 0, binding = 1) uniform SceneData
{
    vec4 fog_color; // w is for exponent
    vec4 fog_distances; //x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction; //w for sun power
    vec4 sunlight_color;
} u_scene_data;

void main ()
{
    o_color = vec4(vec3(v_uv, 1.0) * u_scene_data.ambient_color.rgb, 1.0);
}
