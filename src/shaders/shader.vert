#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;

	uint particle_count;
	float particle_radius;

} ubo;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 fragColor;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	fragColor = color;
	gl_Position = position;
}
