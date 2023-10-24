#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 i_position;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uv;

layout (location = 0) out vec2 v_uv;

layout (push_constant) uniform push_constants_t
{
    mat4 world_matrix;
} u_pc;

layout(set = 0, binding = 0) uniform CameraBuffer
{
    mat4 projection_matrix;
    mat4 view_matrix;
    mat4 view_projection_matrix;
} u_camera;

struct ObjectData
{
	mat4 world_matrix;
};

layout(set = 1, binding = 0, std140) readonly buffer ObjectBuffer
{
	ObjectData objects[];
} u_object_buffer;

void main()
{
    mat4 object_world_matrix = u_object_buffer.objects[gl_BaseInstance].world_matrix;
    gl_Position = u_camera.projection_matrix * u_camera.view_matrix * object_world_matrix * vec4(i_position, 1.0f);
    v_uv = i_uv;
}
