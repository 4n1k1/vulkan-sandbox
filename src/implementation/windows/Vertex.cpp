#include "Vertex.hpp"

VkVertexInputBindingDescription Vertex::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription = {};

	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
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
