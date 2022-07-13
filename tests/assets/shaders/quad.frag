#version 450

layout(location = 0) in vec2 uv_coords;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D tex_sampler;

void main()
{
	color = texture(tex_sampler, uv_coords);
}
