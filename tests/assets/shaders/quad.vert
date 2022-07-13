#version 450

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 uv_coords;

layout(push_constant) uniform push {
        mat4 matrix;
} push_consts;

void main()
{
	gl_Position = push_consts.matrix * vec4(pos, 0.0, 1.0);
	uv_coords = uv;
}
