#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;

	vec4 color;
} ubo;

layout(location = 0) in vec3 position;

layout(location = 0) out vec4 fragment_color;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	fragment_color = ubo.color;

	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0);
}