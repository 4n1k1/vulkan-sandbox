#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> /* includes <vulkan/vulkan.h> */

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <set>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <unordered_map>

#include "zGame.hpp"
#include "VDeleter.hpp"
#include "Vertex.hpp"

#ifndef WIN32
#define __stdcall
#endif

namespace zGame {

VkResult CreateDebugReportCallbackEXT(
	VkInstance instance,
	const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugReportCallbackEXT* pCallback
) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
		instance,
		"vkCreateDebugReportCallbackEXT"
	);

	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(
	VkInstance instance,
	VkDebugReportCallbackEXT callback,
	const VkAllocationCallbacks* pAllocator
) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		instance,
		"vkDestroyDebugReportCallbackEXT"
	);

	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}

const int WIDTH = 800;
const int HEIGHT = 600;

const std::string MODEL_PATH = "../../src/models/chalet.obj";
const std::string TEXTURE_PATH = "../../src/textures/chalet.jpg";

const std::vector<const char*> requiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};
const std::vector<const char*> requiredDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool _enable_validation_layers = true;
#endif

class GPUBridge {

	GLFWwindow* _window;
	VDeleter<VkInstance> _instance{ vkDestroyInstance };
	VDeleter<VkDebugReportCallbackEXT> _callback{ this->_instance, DestroyDebugReportCallbackEXT };
	VDeleter<VkSurfaceKHR> _surface{ this->_instance, vkDestroySurfaceKHR };
	std::vector<const char*> _validExtensions;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VDeleter<VkDevice> _device{ vkDestroyDevice };
	QueueFamilyIndices _queueFamilyIndices;
	VkQueue _graphicsQueue;
	VkQueue _presentQueue;
	VDeleter<VkSwapchainKHR> _swapChain{ this->_device, vkDestroySwapchainKHR };
	std::vector<VkImage> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;
	std::vector<VDeleter<VkImageView>> _swapChainImageViews;
	VDeleter<VkPipelineLayout> _pipelineLayout{ this->_device, vkDestroyPipelineLayout };
	VDeleter<VkRenderPass> _renderPass{ this->_device, vkDestroyRenderPass };
	VDeleter<VkPipeline> _graphicsPipeline{ this->_device, vkDestroyPipeline };
	std::vector<VDeleter<VkFramebuffer>> _swapChainFramebuffers;
	VDeleter<VkCommandPool> _commandPool{ this->_device, vkDestroyCommandPool };
	std::vector<VkCommandBuffer> _commandBuffers;
	VDeleter<VkSemaphore> _imageAvailableSemaphore{ this->_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> _renderFinishedSemaphore{ this->_device, vkDestroySemaphore };
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	VDeleter<VkBuffer> _vertexBuffer{ this->_device, vkDestroyBuffer };
	VDeleter<VkDeviceMemory> _vertexBufferMemory{ this->_device, vkFreeMemory };
	VDeleter<VkBuffer> _indexBuffer{ this->_device, vkDestroyBuffer };
	VDeleter<VkDeviceMemory> _indexBufferMemory{ this->_device, vkFreeMemory };
	VDeleter<VkBuffer> _uniformStagingBuffer{ this->_device, vkDestroyBuffer };
	VDeleter<VkDeviceMemory> _uniformStagingBufferMemory{ this->_device, vkFreeMemory };
	VDeleter<VkBuffer> _uniformBuffer{ this->_device, vkDestroyBuffer };
	VDeleter<VkDeviceMemory> _uniformBufferMemory{ this->_device, vkFreeMemory };
	VDeleter<VkDescriptorSetLayout> _descriptorSetLayout{ this->_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorPool> _descriptorPool{ this->_device, vkDestroyDescriptorPool };
	VkDescriptorSet _descriptorSet;
	VDeleter<VkImage> _stagingImage{ this->_device, vkDestroyImage };
	VDeleter<VkDeviceMemory> _stagingImageMemory{ this->_device, vkFreeMemory };
	VDeleter<VkImage> _textureImage{ this->_device, vkDestroyImage };
	VDeleter<VkDeviceMemory> _textureImageMemory{ this->_device, vkFreeMemory };
	VDeleter<VkImageView> _textureImageView{ this->_device, vkDestroyImageView };
	VDeleter<VkSampler> _textureSampler{ this->_device, vkDestroySampler };
	VDeleter<VkImage> _depthImage{ this->_device, vkDestroyImage };
	VDeleter<VkDeviceMemory> _depthImageMemory{ this->_device, vkFreeMemory };
	VDeleter<VkImageView> _depthImageView{ this->_device, vkDestroyImageView };

	void _initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // enable resizing

		this->_window = glfwCreateWindow(WIDTH, HEIGHT, "zGame", nullptr, nullptr);

		glfwSetWindowUserPointer(this->_window, this);
		glfwSetWindowSizeCallback(this->_window, GPUBridge::_onWindowResized);
	}

	void _initVulkan() {
		this->_createInstance();
		this->_setupDebugCallback();
		this->_createSurface();
		this->_pickPhysicalDevice();
		this->_createLogicalDevice();
		this->_createSwapChain();
		this->_createImageViews();
		this->_createRenderPass();
		this->_createDescriptorSetLayout();
		this->_createGraphicsPipeline();
		this->_createCommandPool();
		this->_createDepthResources();
		this->_createFramebuffers();
		this->_createTextureImage();
		this->_createTextureImageView();
		this->_createTextureSampler();
		this->_loadModel();
		this->_createVertexBuffer();
		this->_createIndexBuffer();
		this->_createUniformBuffer();
		this->_createDescriptorPool();
		this->_createDescriptorSet();
		this->_createCommandBuffers();
		this->_createSemaphores();
	}

	void _loadModel() {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.c_str())) {
			throw std::runtime_error(err);
		}

		std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex = {};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = (uint32_t)this->_vertices.size();
					this->_vertices.push_back(vertex);
				}

				this->_indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	void _createDepthResources() {
		VkFormat depthFormat = this->_findDepthFormat();

		this->_createImage(
			this->_swapChainExtent.width,
			this->_swapChainExtent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			this->_depthImage,
			this->_depthImageMemory
		);

		this->_createImageView(
			this->_depthImage,
			depthFormat,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			this->_depthImageView
		);

		this->_transitionImageLayout(
			this->_depthImage,
			depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		);
	}

	void _createTextureSampler() {
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		// samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(this->_device, &samplerInfo, nullptr, this->_textureSampler.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}
	}

	VkFormat _findSupportedFormat(
		const std::vector<VkFormat>& candidates,
		VkImageTiling tiling,
		VkFormatFeatureFlags features
	) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(this->_physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat _findDepthFormat() {
		return this->_findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool _hasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void _createImageView(
		VkImage image,
		VkFormat format,
		VkImageAspectFlags aspectFlags,
		VDeleter<VkImageView>& imageView
	) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.aspectMask = aspectFlags;

		if (vkCreateImageView(this->_device, &viewInfo, nullptr, imageView.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}
	}

	void _createTextureImageView() {
		this->_createImageView(
			this->_textureImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			this->_textureImageView
		);
	}

	void _transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = this->_beginSingleTimeCommands();

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (this->_hasStencilComponent(format)) {
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		this->_endSingleTimeCommands(commandBuffer);
	}

	void _copyImage(VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height) {
		VkCommandBuffer commandBuffer = this->_beginSingleTimeCommands();

		VkImageSubresourceLayers subResource = {};
		subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResource.baseArrayLayer = 0;
		subResource.mipLevel = 0;
		subResource.layerCount = 1;

		VkImageCopy region = {};
		region.srcSubresource = subResource;
		region.dstSubresource = subResource;
		region.srcOffset = { 0, 0, 0 };
		region.dstOffset = { 0, 0, 0 };
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		vkCmdCopyImage(
			commandBuffer,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region
		);

		this->_endSingleTimeCommands(commandBuffer);
	}

	VkCommandBuffer _beginSingleTimeCommands() {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = this->_commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(this->_device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void _endSingleTimeCommands(VkCommandBuffer commandBuffer) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(this->_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(this->_graphicsQueue);

		vkFreeCommandBuffers(this->_device, this->_commandPool, 1, &commandBuffer);
	}

	void _createImage(
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VDeleter<VkImage>& image,
		VDeleter<VkDeviceMemory>& imageMemory
	) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(this->_device, &imageInfo, nullptr, image.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(this->_device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = this->_findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(this->_device, &allocInfo, nullptr, imageMemory.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(this->_device, image, imageMemory, 0);
	}

	void _createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		this->_createImage(
			texWidth,
			texHeight,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_LINEAR,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			this->_stagingImage,
			this->_stagingImageMemory
		);

		void* data;
		vkMapMemory(this->_device, this->_stagingImageMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, (size_t)imageSize);
		vkUnmapMemory(this->_device, this->_stagingImageMemory);

		stbi_image_free(pixels);

		this->_createImage(
			texWidth, texHeight,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			this->_textureImage,
			this->_textureImageMemory
		);

		this->_transitionImageLayout(
			this->_stagingImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		);
		this->_transitionImageLayout(
			this->_textureImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		);
		this->_copyImage(this->_stagingImage, this->_textureImage, texWidth, texHeight);

		this->_transitionImageLayout(
			this->_textureImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	void _createDescriptorSet() {
		VkDescriptorSetLayout layouts[] = { this->_descriptorSetLayout };
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = this->_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts;

		if (
			vkAllocateDescriptorSets(
				this->_device,
				&allocInfo,
				&(this->_descriptorSet)
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to allocate descriptor set!");
		}

		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = this->_uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = this->_textureImageView;
		imageInfo.sampler = this->_textureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = this->_descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = this->_descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(this->_device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}

	void _createDescriptorPool() {
		std::array<VkDescriptorPoolSize, 2> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = 1;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		if (
			vkCreateDescriptorPool(
				this->_device,
				&poolInfo,
				nullptr,
				this->_descriptorPool.replace()
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void _createUniformBuffer() {
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		this->_createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			this->_uniformStagingBuffer,
			this->_uniformStagingBufferMemory
		);
		this->_createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			this->_uniformBuffer,
			this->_uniformBufferMemory
		);
	}

	void _updateUniformBuffer() {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

		UniformBufferObject ubo = {};

		ubo.model = glm::rotate(glm::mat4(), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), this->_swapChainExtent.width / (float)this->_swapChainExtent.height, 0.1f, 10.0f);

		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory(this->_device, this->_uniformStagingBufferMemory, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(this->_device, this->_uniformStagingBufferMemory);

		this->_copyBuffer(this->_uniformStagingBuffer, this->_uniformBuffer, sizeof(ubo));
	}

	void _createDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)bindings.size();
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(
				this->_device, 
				&layoutInfo,
				nullptr,
				this->_descriptorSetLayout.replace()
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	void _createIndexBuffer() {
		VkDeviceSize bufferSize = sizeof(this->_indices[0]) * this->_indices.size();

		VDeleter<VkBuffer> stagingBuffer{ this->_device, vkDestroyBuffer };
		VDeleter<VkDeviceMemory> stagingBufferMemory{ this->_device, vkFreeMemory };
		this->_createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory
		);

		void* data;

		vkMapMemory(this->_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, this->_indices.data(), (size_t)bufferSize);
		vkUnmapMemory(this->_device, stagingBufferMemory);

		this->_createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			this->_indexBuffer,
			this->_indexBufferMemory
		);

		this->_copyBuffer(stagingBuffer, this->_indexBuffer, bufferSize);
	}

	void _createVertexBuffer() {
		VkDeviceSize bufferSize = sizeof(this->_vertices[0]) * this->_vertices.size();

		VDeleter<VkBuffer> stagingBuffer{ this->_device, vkDestroyBuffer };
		VDeleter<VkDeviceMemory> stagingBufferMemory{ this->_device, vkFreeMemory };

		this->_createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory
		);

		void* data;

		vkMapMemory(this->_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, this->_vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(this->_device, stagingBufferMemory);

		this->_createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			this->_vertexBuffer,
			this->_vertexBufferMemory
		);

		this->_copyBuffer(stagingBuffer, this->_vertexBuffer, bufferSize);
	}

	void _createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VDeleter<VkBuffer>& buffer, VDeleter<VkDeviceMemory>& bufferMemory) {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(this->_device, &bufferInfo, nullptr, buffer.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create buffer!");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(this->_device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = this->_findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(this->_device, &allocInfo, nullptr, bufferMemory.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate buffer memory!");
		}

		vkBindBufferMemory(this->_device, buffer, bufferMemory, 0);
	}

	void _copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = this->_beginSingleTimeCommands();

		VkBufferCopy copyRegion = {};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		this->_endSingleTimeCommands(commandBuffer);
	}

	uint32_t _findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(this->_physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if (
				typeFilter & (1 << i) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties
			) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void _recreateSwapChain() {
		vkDeviceWaitIdle(this->_device);

		this->_createSwapChain();
		this->_createImageViews();
		this->_createRenderPass();
		this->_createGraphicsPipeline();
		this->_createDepthResources();
		this->_createFramebuffers();
		this->_createCommandBuffers();

		this->_drawFrame();
	}

	void _createSemaphores() {
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (
			vkCreateSemaphore(this->_device, &semaphoreInfo, nullptr, this->_imageAvailableSemaphore.replace()) != VK_SUCCESS ||
			vkCreateSemaphore(this->_device, &semaphoreInfo, nullptr, this->_renderFinishedSemaphore.replace()) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create semaphores!");
		}
	}

	void _createCommandBuffers() {
		if (this->_commandBuffers.size() > 0) {
			vkFreeCommandBuffers(
				this->_device,
				this->_commandPool,
				(uint32_t)this->_commandBuffers.size(),
				this->_commandBuffers.data()
			);
		}

		this->_commandBuffers.resize(
			this->_swapChainFramebuffers.size()
		);

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = this->_commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)this->_commandBuffers.size();

		if (vkAllocateCommandBuffers(this->_device, &allocInfo, this->_commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}

		for (size_t i = 0; i < this->_commandBuffers.size(); i++) {
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr; // Optional

			vkBeginCommandBuffer(this->_commandBuffers[i], &beginInfo);

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = this->_renderPass;
			renderPassInfo.framebuffer = this->_swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = this->_swapChainExtent;

			std::array<VkClearValue, 2> clearValues = {};
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].depthStencil = { 1.0f, 0 };

			renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
			renderPassInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(this->_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(this->_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->_graphicsPipeline);

			VkBuffer vertexBuffers[] = { this->_vertexBuffer };
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindVertexBuffers(this->_commandBuffers[i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(this->_commandBuffers[i], this->_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(this->_commandBuffers[i], (uint32_t)this->_indices.size(), 1, 0, 0, 0);

			vkCmdEndRenderPass(this->_commandBuffers[i]);

			vkCmdBindDescriptorSets(
				this->_commandBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				this->_pipelineLayout, 0, 1,
				&(this->_descriptorSet), 0, nullptr
			);

			if (vkEndCommandBuffer(this->_commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
		}
	}

	void _createCommandPool() {
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = this->_queueFamilyIndices.graphicsFamily;
		poolInfo.flags = 0; // Optional

		if (vkCreateCommandPool(this->_device, &poolInfo, nullptr, this->_commandPool.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void _createFramebuffers() {
		this->_swapChainFramebuffers.resize(
			this->_swapChainImageViews.size(),
			VDeleter<VkFramebuffer>{this->_device, vkDestroyFramebuffer}
		);

		for (size_t i = 0; i < this->_swapChainImageViews.size(); i++) {
			std::array<VkImageView, 2> attachments = {
				this->_swapChainImageViews[i],
				this->_depthImageView
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = this->_renderPass;
			framebufferInfo.attachmentCount = (uint32_t)attachments.size();
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = this->_swapChainExtent.width;
			framebufferInfo.height = this->_swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (
				vkCreateFramebuffer(
					this->_device,
					&framebufferInfo,
					nullptr,
					this->_swapChainFramebuffers[i].replace()
				) != VK_SUCCESS
			) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void _createRenderPass() {
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = this->_swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = this->_findDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subPass = {};
		subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subPass.colorAttachmentCount = 1;
		subPass.pColorAttachments = &colorAttachmentRef;
		subPass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = (uint32_t)attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subPass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (
			vkCreateRenderPass(
				this->_device,
				&renderPassInfo,
				nullptr,
				this->_renderPass.replace()
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create render pass!");
		}

	}

	void _createGraphicsPipeline() {
		auto vertShaderCode = this->_readFile("../../src/shaders/vert.spv");
		auto fragShaderCode = this->_readFile("../../src/shaders/frag.spv");

		VDeleter<VkShaderModule> vertShaderModule{ this->_device, vkDestroyShaderModule };
		VDeleter<VkShaderModule> fragShaderModule{ this->_device, vkDestroyShaderModule };

		this->_createShaderModule(vertShaderCode, vertShaderModule);
		this->_createShaderModule(fragShaderCode, fragShaderModule);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)(this->_swapChainExtent.width);
		viewport.height = (float)(this->_swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = this->_swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE; // VK_TRUE for shadow maps requires enabling GPU feature
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_LINE or VK_POLYGON_MODE_POINT
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		VkDescriptorSetLayout setLayouts[] = { this->_descriptorSetLayout };
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = setLayouts; // Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

		if (
			vkCreatePipelineLayout(
				this->_device,
				&pipelineLayoutInfo,
				nullptr,
				this->_pipelineLayout.replace()
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr; // Optional
		pipelineInfo.layout = this->_pipelineLayout;
		pipelineInfo.renderPass = this->_renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1; // Optional
		pipelineInfo.pDepthStencilState = &depthStencil;

		if (
			vkCreateGraphicsPipelines(
				this->_device,
				VK_NULL_HANDLE,
				1,
				&pipelineInfo,
				nullptr,
				this->_graphicsPipeline.replace()
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}
	}

	void _createShaderModule(const std::vector<char>& code, VDeleter<VkShaderModule>& shaderModule) {
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = (uint32_t*)code.data();

		if (vkCreateShaderModule(this->_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
	}

	void _createImageViews() {
		this->_swapChainImageViews.resize(
			this->_swapChainImages.size(),
			VDeleter<VkImageView>{this->_device, vkDestroyImageView}
		);

		for (uint32_t i = 0; i < this->_swapChainImages.size(); i++) {
			this->_createImageView(
				this->_swapChainImages[i],
				this->_swapChainImageFormat,
				VK_IMAGE_ASPECT_COLOR_BIT,
				this->_swapChainImageViews[i]
			);
		}
	}

	void _createSurface() {
		if (
			glfwCreateWindowSurface(
				this->_instance,
				this->_window,
				nullptr,
				&(this->_surface)
			) != VK_SUCCESS
			) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void _createLogicalDevice() {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = this->_queueFamilyIndices.graphicsFamily;
		queueCreateInfo.queueCount = 1;
		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = 0;

		if (_enable_validation_layers) {
			createInfo.enabledLayerCount = (uint32_t)requiredLayers.size();
			createInfo.ppEnabledLayerNames = requiredLayers.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		createInfo.enabledExtensionCount = (uint32_t)requiredDeviceExtensions.size();
		createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

		if (
			vkCreateDevice(
				this->_physicalDevice,
				&createInfo,
				nullptr,
				&(this->_device)
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(
			this->_device,
			this->_queueFamilyIndices.graphicsFamily,
			0,
			&(this->_graphicsQueue)
		);

		vkGetDeviceQueue(
			this->_device,
			this->_queueFamilyIndices.presentFamily,
			0,
			&(this->_presentQueue)
		);
	}

	void _setQueueFamilyIndicies(VkPhysicalDevice device) {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;

		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				this->_queueFamilyIndices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, this->_surface, &presentSupport);

			if (queueFamily.queueCount > 0 && presentSupport) {
				this->_queueFamilyIndices.presentFamily = i;
			}

			if (this->_queueFamilyIndices.isComplete()) {
				break;
			}

			i++;
		}
	}

	void _pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(this->_instance, &deviceCount, nullptr);

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(this->_instance, &deviceCount, devices.data());

		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		} else {
			for (const auto& device : devices) {
				if (this->_isPhysicalDeviceSuitable(device)) {
					this->_physicalDevice = device;

					break;
				}
			}

			if (this->_physicalDevice == nullptr) {
				throw std::runtime_error("failed to find suitable GPU!");
			}
		}
	};

	bool _isPhysicalDeviceSuitable(const VkPhysicalDevice& device) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		this->_setQueueFamilyIndicies(device);

		bool extensionsSupported = this->_checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;

		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = this->_querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			this->_queueFamilyIndices.isComplete() && this->_queueFamilyIndices.useSameFamily() &&
			extensionsSupported && swapChainAdequate;
	}

	bool _checkDeviceExtensionSupport(const VkPhysicalDevice& device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, supportedExtensions.data());

		std::set<std::string> requiredExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

		for (const auto& extension : supportedExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	void _setupDebugCallback() {
		if (!_enable_validation_layers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)(_debugCallback);

		if (
			CreateDebugReportCallbackEXT(
				this->_instance,
				&createInfo,
				nullptr,
				&(this->_callback)
			) != VK_SUCCESS
		) {
			throw std::runtime_error("failed to set up debug callback!");
		}
	}

	void _mainLoop() {
		while (!glfwWindowShouldClose(this->_window)) {
			glfwPollEvents();

			this->_updateUniformBuffer();
			this->_drawFrame();
		}

		vkDeviceWaitIdle(this->_device);
	}

	void _drawFrame() {
		uint32_t imageIndex;

		vkAcquireNextImageKHR(
			this->_device,
			this->_swapChain,
			std::numeric_limits<uint64_t>::max(),
			this->_imageAvailableSemaphore,
			VK_NULL_HANDLE,
			&imageIndex
		);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { this->_imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &(this->_commandBuffers[imageIndex]);

		VkSemaphore signalSemaphores[] = { this->_renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(this->_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { this->_swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional

		VkResult result = vkQueuePresentKHR(this->_presentQueue, &presentInfo);

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
	}

	void _createInstance() {
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "zGame";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "zEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		if (_enable_validation_layers && !this->_checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		if (!this->_checkGLFWExtensionsSupport(glfwExtensionCount, glfwExtensions)) {
			throw std::runtime_error("glfw extensions are not supported!");
		}

		// assuming VK_EXT_DEBUG_REPORT_EXTENSION_NAME extension is supported
		this->_validExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		if (_enable_validation_layers) {
			createInfo.enabledLayerCount = (uint32_t)requiredLayers.size();
			createInfo.ppEnabledLayerNames = requiredLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		createInfo.enabledExtensionCount = (uint32_t)this->_validExtensions.size();
		createInfo.ppEnabledExtensionNames = this->_validExtensions.data();

		VkResult result = vkCreateInstance(&createInfo, nullptr, &(this->_instance));

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	bool _checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> supportedLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());

		for (const char* layerName : requiredLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : supportedLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	bool _checkGLFWExtensionsSupport(unsigned int glfwExtensionCount, const char** glfwExtensions) {
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.data());

		std::ostringstream log_record;

		log_record << "Supported extensions: ";

		for (const auto& extension : supportedExtensions) {
			log_record << extension.extensionName;

			if (strcmp(extension.extensionName, supportedExtensions.back().extensionName) != 0) {
				log_record << ", ";
			}
		}

		LOG_DEBUG(log_record.str());

		log_record.str("");
		log_record.clear();

		log_record << "GLFW required extensions: ";

		for (unsigned int i = 0; i < glfwExtensionCount; i++) {
			bool extensionFound = false;

			log_record << glfwExtensions[i];

			if (i != glfwExtensionCount - 1) {
				log_record << ", ";
			}

			for (const auto& extension : supportedExtensions) {
				if (strcmp(glfwExtensions[i], extension.extensionName) == 0) {
					extensionFound = true;

					this->_validExtensions.push_back(glfwExtensions[i]);

					break;
				}
			}

			if (!extensionFound) {
				return false;
			}
		}

		LOG_DEBUG(log_record.str());

		return true;
	}

	SwapChainSupportDetails _querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, this->_surface, &(details.capabilities));

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->_surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->_surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, this->_surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, this->_surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR _chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR _chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D _chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		} else {
			VkExtent2D actualExtent = { WIDTH, HEIGHT };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void _createSwapChain() {
		SwapChainSupportDetails swapChainSupport = this->_querySwapChainSupport(this->_physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = this->_chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = this->_chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = this->_chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = this->_surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(this->_device, &createInfo, nullptr, &(this->_swapChain)) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(this->_device, this->_swapChain, &imageCount, nullptr);
		this->_swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(this->_device, this->_swapChain, &imageCount, this->_swapChainImages.data());

		this->_swapChainImageFormat = surfaceFormat.format;
		this->_swapChainExtent = extent;
	}

	static VkBool32 __stdcall _debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData
	) {
		LOG_DEBUG("validation layer: " + string(msg));

		return VK_FALSE;
	}

	static std::vector<char> _readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	static void _onWindowResized(GLFWwindow* window, int width, int height) {
		if (width == 0 || height == 0) return;

		GPUBridge* app = reinterpret_cast<GPUBridge*>(glfwGetWindowUserPointer(window));
		app->_recreateSwapChain();
	}

public:

	void run() {
		this->_initWindow();
		this->_initVulkan();
		this->_mainLoop();
	}
};

}