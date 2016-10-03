#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		VkVertexInputAttributeDescription vertexDescription;

		vertexDescription.binding = 0;
		vertexDescription.location = 0;
		vertexDescription.format = VK_FORMAT_R32G32_SFLOAT;
		vertexDescription.offset = offsetof(Vertex, pos);

		VkVertexInputAttributeDescription colorDescription;

		colorDescription.binding = 0;
		colorDescription.location = 1;
		colorDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		colorDescription.offset = offsetof(Vertex, color);

		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {
			vertexDescription, colorDescription
		};

		return attributeDescriptions;
	}
};

const std::vector<Vertex> vertices = {
	// interleaving vertex attributes

	{ { 0.0f, -0.5f },{ 1.0f, 0.0f, 0.0f } }, // 2d pos, color
	{ { 0.5f, 0.5f },{ 0.0f, 1.0f, 0.0f } },  // 2d pos, color
	{ { -0.5f, 0.5f },{ 0.0f, 0.0f, 1.0f } }  // 2d pos, color
};

