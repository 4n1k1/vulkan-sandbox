#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;

	vec4 colors;

} ubo;

layout(location = 0) in vec3 position;
layout(location = 0) out vec4 fragColor;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	fragColor = vec4(ubo.colors[0]);
	gl_Position = vec4(position, 1.0);
}