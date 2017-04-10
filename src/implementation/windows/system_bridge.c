#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "system_bridge.h"
#include "math3d.h"

#define DEVICE_QUEUES_COUNT 3
#define GPU_DATA_BINDINGS_COUNT 4
#define PARTICLE_COUNT 8
#define PHYSICAL_DEVICE_EXTENSIONS_COUNT 1
#define VALIDATION_LAYERS_COUNT 1

/* Module state */

static const uint32_t _DEFAULT_WINDOW_WIDTH = 800;
static const uint32_t _DEFAULT_WINDOW_HEIGHT = 600;

static const Vector3 _eye = { 0.0f, 0.0f, -1.0f };
static const Vector3 _up = { 0.0f, 1.0f, -1.0f };
static const Vector3 _look_at = { 0.0f, 0.0f, 0.0f };

static Matrix4x4 _projection = {
	.data = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static Matrix4x4 _view = {
	.data = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static Matrix4x4 _model = {
	.data = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static OperationQueueFamilies _operation_queue_families = {
	.graphics_family_idx = -1,
	.compute_family_idx = -1,
	.present_family_idx = -1,

	.use_same_family = false,
};

static GLFWwindow *_window = NULL;
static VkInstance _instance;

#ifdef _DEBUG
static VkDebugReportCallbackEXT _debug_callback_object;
static Extensions _required_validation_layers;
#endif

static Extensions _required_instance_extensions;
static Extensions _required_physical_device_extensions;

static VkSurfaceKHR _surface;
static VkSurfaceCapabilitiesKHR _surface_capabilities;
static SurfaceFormats _surface_formats;
static PresentModes _present_modes;
static VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
static VkDevice _device;
static VkQueue _graphics_queue;
static VkQueue _compute_queue;
static VkQueue _present_queue;
static VkRenderPass _render_pass;
static VkCommandPool _long_live_buffers_pool;
static VkCommandPool _one_time_buffers_pool;
static CommandBuffers _command_buffers;

static uint32_t _one_time_command_buffer_idx;
static uint32_t _image_draw_command_buffers_begin_idx;
static uint32_t _compute_command_buffer_idx;

static VkSwapchainKHR _swap_chain;

static VkExtent2D _swap_chain_image_extent;
static VkFormat _swap_chain_image_format;
static SwapChainImages _swap_chain_images;
static SwapChainImageViews _swap_chain_image_views;
static SwapChainFramebuffers _swap_chain_framebuffers;

static VkImage _depth_image;
static VkImageView _depth_image_view;
static VkFormat _depth_image_format;
static VkDeviceMemory _depth_image_memory;

static VkDescriptorPool _descriptor_pool;
static VkDescriptorSetLayout _descriptor_set_layout;
static VkDescriptorSet _descriptor_set;

static VkPipelineLayout _graphics_pipeline_layout;
static VkPipeline _graphics_pipeline;

static VkShaderModule _vertex_shader;
static char *_vertex_shader_code;
static VkShaderModule _fragment_shader;
static char *_fragment_shader_code;
static VkShaderModule _compute_shader;
static char *_compute_shader_code;

static VkPipelineLayout _compute_pipeline_layout;
static VkPipeline _compute_pipeline;
static VkFence _compute_finished_fence;

static VkBuffer _host_uniform_data_buffer;
static VkDeviceMemory _host_uniform_data_buffer_memory;
static VkBuffer _device_uniform_data_buffer;
static VkDeviceMemory _device_uniform_data_buffer_memory;

static VkBuffer _device_vertex_buffer;
static VkDeviceMemory _device_vertex_buffer_memory;
static VkBuffer _host_vertex_buffer;
static VkDeviceMemory _host_vertex_buffer_memory;

static VkBuffer _device_index_buffer;
static VkDeviceMemory _device_index_buffer_memory;
static VkBuffer _host_index_buffer;
static VkDeviceMemory _host_index_buffer_memory;

static VkBuffer _device_particle_buffer;
static VkDeviceMemory _device_particle_buffer_memory;
static VkBuffer _host_particle_buffer;
static VkDeviceMemory _host_particle_buffer_memory;

static VkSemaphore _image_available;
static VkSemaphore _render_finished;

static Vertices _vertices;
static Indices _indices;
static UniformData _uniform_data;
static Particle *_particles;

/* Helper functions */

static bool _begin_one_time_command()
{
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	return vkBeginCommandBuffer(
		_command_buffers.data[_one_time_command_buffer_idx],
		&beginInfo
	) == VK_SUCCESS;
}
static bool _submit_one_time_command()
{
	vkEndCommandBuffer(
		_command_buffers.data[_one_time_command_buffer_idx]
	);

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = _command_buffers.data + _one_time_command_buffer_idx,
	};

	return (
		vkQueueSubmit(_present_queue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS &&
		vkQueueWaitIdle(_present_queue) == VK_SUCCESS
	);
}
static bool _create_image(VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *image_memory, VkImageLayout layout)
{
	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.extent.width = _swap_chain_image_extent.width,
		.extent.height = _swap_chain_image_extent.height,
		.extent.depth = 1,
		.mipLevels = 1,
		.arrayLayers = 1,
		.format = format,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.initialLayout = layout,
		.usage = usage,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	CHECK_VK_RESULT(vkCreateImage(_device, &imageInfo, NULL, image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
	};

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(_physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (
			memRequirements.memoryTypeBits & (1 << i) &&
			(memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		) {
			allocInfo.memoryTypeIndex = i;
		}
	}

	CHECK_VK_RESULT(vkAllocateMemory(_device, &allocInfo, NULL, image_memory));

	return vkBindImageMemory(_device, *image, *image_memory, 0) == VK_SUCCESS;
}
static bool _create_image_view(const VkImage *image, VkImageView *imageView, VkFormat format, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = *image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange.aspectMask = aspectFlags,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
	};

	return vkCreateImageView(_device, &viewInfo, NULL, imageView) == VK_SUCCESS;
}
static bool _find_memory_type(uint32_t *memory_type, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(_physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (
			typeFilter & (1 << i) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties
		) {
			*memory_type = i;

			return true;
		}
	}

	return false;
}
/*
	Copy buffer from host visible GPU memory to faster GPU memory.
	Command is written to _one_time_command_buffer (present queue).
	Host waits for device present queue to get idle after execution.
*/
static bool _copy_buffer(VkBuffer *dst_buffer, VkBuffer *src_buffer, VkDeviceSize size)
{
	CHECK_RESULT(_begin_one_time_command());

	VkBufferCopy copy_attrs = { .size = size };

	vkCmdCopyBuffer(
		_command_buffers.data[_one_time_command_buffer_idx],
		*src_buffer,
		*dst_buffer,
		1, &copy_attrs
	);

	return _submit_one_time_command();
}

/* Secondary logic */

static void _setup_required_extensions()
{
	const char **glfw_names;

	uint32_t glfw_count;
	uint32_t total_count;

	glfw_names = glfwGetRequiredInstanceExtensions(&glfw_count);

	total_count = glfw_count;

#ifdef _DEBUG
	total_count += 1;  /* VK_EXT_DEBUG_REPORT_EXTENSION_NAME */
#endif

	_required_instance_extensions.names = (char**)malloc(sizeof(char*) * total_count);
	_required_instance_extensions.count = total_count;

	for (uint32_t i = 0; i < glfw_count; i += 1)
	{
		_required_instance_extensions.names[i] = glfw_names[i];
	}

#ifdef _DEBUG
	_required_instance_extensions.names[glfw_count] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
#endif
}
static bool _physical_device_supports_required_extensions(const VkPhysicalDevice* physical_device) {
	uint32_t supported_exts_num;
	vkEnumerateDeviceExtensionProperties(*physical_device, NULL, &supported_exts_num, NULL);

		VkExtensionProperties *supported_extensions = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * supported_exts_num);

		vkEnumerateDeviceExtensionProperties(*physical_device, NULL, &supported_exts_num, supported_extensions);

		uint32_t matches = 0;

		for (uint32_t i = 0; i < _required_physical_device_extensions.count; i += 1)
		{
			for (uint32_t j = 0; j < supported_exts_num; j += 1)
			{
				VkExtensionProperties *supported_extension = supported_extensions + j;

				if (strcmp(supported_extension->extensionName, _required_physical_device_extensions.names[i]) == 0)
				{
					matches += 1;
				}
			}
		}

		bool result = (matches == _required_physical_device_extensions.count);

		free(supported_extensions);

		return result;
}
static VkBool32 __stdcall _debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, uint32_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	printf("Validation layer issue: %s.\n", msg);

	return VK_FALSE;
}

static void _set_surface_properties(const VkPhysicalDevice *physical_device)
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physical_device, _surface, &_surface_capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, _surface, &(_surface_formats.count), NULL);

	if (_surface_formats.count != 0) {
		_surface_formats.data = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * _surface_formats.count);

		vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, _surface, &(_surface_formats.count), _surface_formats.data);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device, _surface, &(_present_modes.count), NULL);

	if (_present_modes.count != 0) {
		_present_modes.data = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * _present_modes.count);

		vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device, _surface, &(_present_modes.count), _present_modes.data);
	}
}
static void _pick_swap_chain_surface_format(VkSurfaceFormatKHR *picked_surface_format)
{
	if (_surface_formats.count == 1 && _surface_formats.data[0].format == VK_FORMAT_UNDEFINED) {
		picked_surface_format->format = VK_FORMAT_B8G8R8A8_UNORM;
		picked_surface_format->colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

		return;
	}

	for (uint32_t i = 0; i < _surface_formats.count; i += 1)
	{
		if (
			_surface_formats.data[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
			_surface_formats.data[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		) {
			picked_surface_format->format = _surface_formats.data[i].format;
			picked_surface_format->colorSpace = _surface_formats.data[i].colorSpace;

			return;
		}
	}
}
static void _pick_swap_chain_present_mode(VkPresentModeKHR *present_mode)
{
#ifdef NDEBUG
	for (uint32_t i = 0; i < _present_modes.count; i += 1) {
		if (_present_modes.data[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			*present_mode = _present_modes.data[i];
		}
	}
#endif

	*present_mode = VK_PRESENT_MODE_FIFO_KHR;
}

static bool _instance_supports_required_extensions()
{
	uint32_t supported_exts_num = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_num, NULL);

	VkExtensionProperties *supported_exts = malloc(sizeof(VkExtensionProperties) * supported_exts_num);
	vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_num, supported_exts);

	bool result = true;

	for (uint32_t i = 0; i < _required_instance_extensions.count; i += 1) {
		bool ext_found = false;

		const char* ext_name = _required_instance_extensions.names[i];

		for (uint32_t j = 0; j < supported_exts_num; j += 1)
		{
			const char* s_ext_name = supported_exts[j].extensionName;

			if (strcmp(supported_exts[j].extensionName, _required_instance_extensions.names[i]) == 0) {
				ext_found = true;
				break;
			}
		}

		if (!ext_found)
		{
			result = false;
			break;
		}

	}

	free(supported_exts);

	return result;
}
static bool _set_queue_family_properties(const VkPhysicalDevice *physical_device)
{
	uint32_t queue_families_num = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(*physical_device, &queue_families_num, NULL);

	VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_families_num);
	vkGetPhysicalDeviceQueueFamilyProperties(*physical_device, &queue_families_num, queue_families);

	bool result = false;

	for (uint32_t i = 0; i < queue_families_num; i += 1)
	{
		if (
			queue_families[i].queueCount > 0 &&
			queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
		) {
			_operation_queue_families.graphics_family_idx = (int)i;
		}

		if (
			queue_families[i].queueCount > 0 &&
			queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT
		) {
			_operation_queue_families.compute_family_idx = (int)i;
		}

		VkBool32 present_support;

		vkGetPhysicalDeviceSurfaceSupportKHR(*physical_device, i, _surface, &present_support);

		if (queue_families[i].queueCount > 0 && present_support)
		{
			_operation_queue_families.present_family_idx = (int)i;
		}

		if (
			_operation_queue_families.graphics_family_idx >= 0 &&
			_operation_queue_families.present_family_idx >= 0 &&
			_operation_queue_families.compute_family_idx >= 0
		) {
			result = true;

			_operation_queue_families.use_same_family = (
				_operation_queue_families.graphics_family_idx == _operation_queue_families.present_family_idx &&
				_operation_queue_families.present_family_idx == _operation_queue_families.compute_family_idx
			);

			break;
		}
	}

	free(queue_families);

	return result;
}
static bool _physical_device_suitable(const VkPhysicalDevice* physical_device)
{
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(*physical_device, &device_properties);

	if (!_set_queue_family_properties(physical_device))
	{
		return false;
	}

	bool extensions_supported = _physical_device_supports_required_extensions(physical_device);

	_set_surface_properties(physical_device);

	bool surface_output_supported = (
		_surface_formats.count != 0 &&
		extensions_supported &&
		_present_modes.count != 0
	);

	return (
		device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		_operation_queue_families.use_same_family &&
		surface_output_supported
	);
}
static bool _create_shader_module(VkShaderModule *shader_module, const char bin_shader_file_path[], char* shader_code)
{
	FILE *file_descriptor = fopen(bin_shader_file_path, "rb");

	if (!file_descriptor)
	{
		return false;
	}

	fseek(file_descriptor, 0L, SEEK_END);
	uint32_t code_size = ftell(file_descriptor);

	rewind(file_descriptor);

	shader_code = malloc(code_size);

	fread(shader_code, code_size, 1, file_descriptor);

	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code_size,
		.pCode = (const uint32_t *)shader_code,
	};

	return vkCreateShaderModule(_device, &create_info, NULL, shader_module) == VK_SUCCESS;
}
static bool _create_memory_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *buffer_memory)
{
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	CHECK_VK_RESULT(vkCreateBuffer(_device, &bufferInfo, NULL, buffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(_device, *buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
	};

	CHECK_RESULT(_find_memory_type(&(allocInfo.memoryTypeIndex), memRequirements.memoryTypeBits, properties));
	CHECK_VK_RESULT(vkAllocateMemory(_device, &allocInfo, NULL, buffer_memory))

	return vkBindBufferMemory(_device, *buffer, *buffer_memory, 0) == VK_SUCCESS;
}
static bool _pick_depth_buffer_format()
{
	VkFormat suitable_formats[3] = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};

	VkFormatProperties props;

	for (uint32_t i = 0; i < 3; i += 1)
	{
		vkGetPhysicalDeviceFormatProperties(_physical_device, suitable_formats[i], &props);

		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			_depth_image_format = suitable_formats[i];

			return true;
		}
	}

	return false;
}

/* Primary logic */

#ifdef _DEBUG
static bool _instance_supports_required_layers()
{
	uint32_t supported_layers_num;
	vkEnumerateInstanceLayerProperties(&supported_layers_num, NULL);

	VkLayerProperties *supported_layers = malloc(sizeof(VkLayerProperties) * supported_layers_num);

	vkEnumerateInstanceLayerProperties(&supported_layers_num, supported_layers);

	bool result = true;

	for (uint32_t i = 0; i < _required_validation_layers.count; i += 1)
	{
		bool layer_found = false;

		for (uint32_t j = 0; j < supported_layers_num; j += 1) {
			if (strcmp(supported_layers[j].layerName, _required_validation_layers.names[i]) == 0) {
				layer_found = true;
				break;
			}
		}

		if (!layer_found)
		{
			result = false;
			break;
		}
	}

	free(supported_layers);

	return result;
}
static bool _setup_debug_callback()
{
	VkDebugReportCallbackCreateInfoEXT create_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
		.pfnCallback = (PFN_vkDebugReportCallbackEXT)(_debug_callback)
	};

	PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
		_instance, "vkCreateDebugReportCallbackEXT"
	);

	if (func == NULL)
	{
		return false;
	}

	func(_instance, &create_info, NULL, &_debug_callback_object);

	return true;
}
#endif

static bool _create_instance()
{
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "zGame",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "zEngine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
	};

	create_info.ppEnabledExtensionNames = _required_instance_extensions.names;
	create_info.enabledExtensionCount = _required_instance_extensions.count;

#ifdef _DEBUG
	create_info.enabledLayerCount = _required_validation_layers.count;
	create_info.ppEnabledLayerNames = _required_validation_layers.names;
#endif

	VkResult result = vkCreateInstance(&create_info, NULL, &_instance);

	return result == VK_SUCCESS;
}
static bool _pick_physical_device()
{
	uint32_t devices_num = 0;

	vkEnumeratePhysicalDevices(_instance, &devices_num, NULL);

	VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * devices_num);
	vkEnumeratePhysicalDevices(_instance, &devices_num, devices);

	bool result = true;

	if (devices_num == 0)
	{
		result = false;
	}
	else
	{
		for (uint32_t i = 0; i < devices_num; i += 1) {
			if (_physical_device_suitable(devices + i)) {
				_physical_device = devices[i];

				break;
			}
		}

		if (_physical_device == VK_NULL_HANDLE) {
			result = false;
		}
	}

	free(devices);

	return result;
};
static bool _create_logical_device()
{
	float queuePriorities[DEVICE_QUEUES_COUNT] = { 1.0f, 0.5f, 0.0f };

	VkDeviceQueueCreateInfo queue_create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = _operation_queue_families.graphics_family_idx,  // all queues in this family
		.queueCount = DEVICE_QUEUES_COUNT,
		.pQueuePriorities = queuePriorities,
	};

	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceFeatures(_physical_device, &device_features);

	VkDeviceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pQueueCreateInfos = &queue_create_info,
		.queueCreateInfoCount = 1,
		.pEnabledFeatures = &device_features,
		.enabledExtensionCount = _required_physical_device_extensions.count,
		.ppEnabledExtensionNames = _required_physical_device_extensions.names,
	};

	CHECK_VK_RESULT(vkCreateDevice(_physical_device, &create_info, NULL, &_device));

	vkGetDeviceQueue(_device, _operation_queue_families.compute_family_idx, 0, &_compute_queue);
	vkGetDeviceQueue(_device, _operation_queue_families.graphics_family_idx, 1, &_graphics_queue);
	vkGetDeviceQueue(_device, _operation_queue_families.present_family_idx, 2, &_present_queue);

	return true;
}
static bool _create_swap_chain()
{
	VkSurfaceFormatKHR picked_surface_format;
	_pick_swap_chain_surface_format(&picked_surface_format);

	VkPresentModeKHR picked_present_mode;
	_pick_swap_chain_present_mode(&picked_present_mode);

	_swap_chain_image_extent.width = _DEFAULT_WINDOW_WIDTH;
	_swap_chain_image_extent.height = _DEFAULT_WINDOW_HEIGHT;

	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = _surface,
		.minImageCount = _surface_capabilities.minImageCount,
		.imageFormat = picked_surface_format.format,
		.imageColorSpace = picked_surface_format.colorSpace,
		.imageExtent = _swap_chain_image_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = _surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = picked_present_mode,
		.clipped = VK_TRUE,
	};

	CHECK_VK_RESULT(vkCreateSwapchainKHR(_device, &createInfo, NULL, &(_swap_chain)));

	vkGetSwapchainImagesKHR(_device, _swap_chain, &(_swap_chain_images.count), NULL);

	_swap_chain_images.data = malloc(sizeof(VkImage) * _swap_chain_images.count);

	vkGetSwapchainImagesKHR(_device, _swap_chain, &(_swap_chain_images.count), _swap_chain_images.data);

	_swap_chain_image_format = picked_surface_format.format;

	_swap_chain_image_views.count = _swap_chain_images.count;
	_swap_chain_image_views.data = malloc(
		sizeof(VkImageView) * _swap_chain_image_views.count
	);

	for (uint32_t i = 0; i < _swap_chain_images.count; i++)
	{
		CHECK_RESULT(
			_create_image_view(
				_swap_chain_images.data + i,
				_swap_chain_image_views.data + i,
				_swap_chain_image_format,
				VK_IMAGE_ASPECT_COLOR_BIT
			)
		);
	}

	return true;

}
static bool _create_render_pass()
{
	VkAttachmentDescription color_attachment = {
		.format = _swap_chain_image_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference color_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkAttachmentDescription depth_attachment = {
		.format = _depth_image_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkAttachmentReference depth_attachment_ref = {
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subPass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
		.pDepthStencilAttachment = &depth_attachment_ref,
	};

	VkSubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	};

	VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 2,
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subPass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	};

	return vkCreateRenderPass(_device, &render_pass_info, NULL, &_render_pass) == VK_SUCCESS;
}
static bool _create_graphics_pipeline()
{
	CHECK_RESULT(_create_shader_module(&_vertex_shader, "../../src/shaders/vertex.spv", _vertex_shader_code));
	CHECK_RESULT(_create_shader_module(&_fragment_shader, "../../src/shaders/fragment.spv", _fragment_shader_code));

	VkPipelineShaderStageCreateInfo vertex_shader_stage_ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = _vertex_shader,
		.pName = "main",
	};

	VkPipelineShaderStageCreateInfo fragment_shader_stage_ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = _fragment_shader,
		.pName = "main",
	};

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		vertex_shader_stage_ci,
		fragment_shader_stage_ci,
	};

	VkVertexInputBindingDescription binding_description = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription attribute_descriptions[2];

	attribute_descriptions[0].binding = 0;
	attribute_descriptions[0].location = 0;
	attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribute_descriptions[0].offset = 0;

	attribute_descriptions[1].binding = 0;
	attribute_descriptions[1].location = 1;
	attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribute_descriptions[1].offset = sizeof(Vector4);


	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.vertexAttributeDescriptionCount = 2,
		.pVertexBindingDescriptions = &binding_description,
		.pVertexAttributeDescriptions = attribute_descriptions,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.stencilTestEnable = VK_FALSE,
	};

	VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)(_swap_chain_image_extent.width),
		.height = (float)(_swap_chain_image_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	VkOffset2D offset = {
		.x = 0,
		.y = 0,
	};

	VkRect2D scissor = {
		.offset = offset,
		.extent = _swap_chain_image_extent,
	};

	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};

	VkPipelineRasterizationStateCreateInfo rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // VK_TRUE for shadow maps requires enabling GPU feature
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f, // Optional
		.depthBiasClamp = 0.0f, // Optional
		.depthBiasSlopeFactor = 0.0f, // Optional
	};

	VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable = VK_FALSE,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading = 1.0f, // Optional
		.pSampleMask = NULL, // Optional
		.alphaToCoverageEnable = VK_FALSE, // Optional
		.alphaToOneEnable = VK_FALSE, // Optional
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {
		.colorWriteMask = (
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT
		),
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
	};

	VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY, // Optional
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants[0] = 0.0f, // Optional
		.blendConstants[1] = 0.0f, // Optional
		.blendConstants[2] = 0.0f, // Optional
		.blendConstants[3] = 0.0f, // Optional
	};

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &_descriptor_set_layout,
		.pushConstantRangeCount = 0, // Optional
		.pPushConstantRanges = 0, // Optional
	};

	CHECK_VK_RESULT(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, NULL, &_graphics_pipeline_layout));

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.layout = _graphics_pipeline_layout,
		.renderPass = _render_pass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.pDepthStencilState = &depthStencil,
	};

	return vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &_graphics_pipeline) == VK_SUCCESS;
}
static bool _create_compute_pipeline()
{
	CHECK_RESULT(_create_shader_module(&_compute_shader, "../../src/shaders/compute.spv", _compute_shader_code));

	VkPipelineShaderStageCreateInfo compute_shader_stage_ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = _compute_shader,
		.pName = "main",
	};

	VkPipelineLayoutCreateInfo pipeline_layout_ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.setLayoutCount = 1,
		.pSetLayouts = &_descriptor_set_layout,
	};

	CHECK_VK_RESULT(vkCreatePipelineLayout(_device, &pipeline_layout_ci, NULL, &_compute_pipeline_layout));

	VkComputePipelineCreateInfo pipeline_ci = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.layout = _compute_pipeline_layout,
		.flags = 0,
		.stage = compute_shader_stage_ci,
	};

	return vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &_compute_pipeline) == VK_SUCCESS;
}
static bool _create_depth_resources()
{
	CHECK_RESULT(_pick_depth_buffer_format());
	CHECK_RESULT(
		_create_image(
			_depth_image_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_depth_image,
			&_depth_image_memory,
			VK_IMAGE_LAYOUT_UNDEFINED
		)
	);
	
	CHECK_RESULT(
		_create_image_view(
			&_depth_image,
			&_depth_image_view,
			_depth_image_format,
			VK_IMAGE_ASPECT_DEPTH_BIT
		)
	);

	CHECK_RESULT(_begin_one_time_command());

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = _depth_image,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
	};

	if (
		_depth_image_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
		_depth_image_format == VK_FORMAT_D24_UNORM_S8_UINT
	) {
		barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	vkCmdPipelineBarrier(
		_command_buffers.data[_one_time_command_buffer_idx],
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier
	);

	return _submit_one_time_command();
}
static bool _create_descriptor_pool()
{
	VkDescriptorPoolSize uniform_buffer_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
	};

	VkDescriptorPoolSize storage_buffer_size = {
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 3,
	};

	VkDescriptorPoolSize pool_sizes[] = {
		uniform_buffer_size,
		storage_buffer_size
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 2,
		.pPoolSizes = pool_sizes,
		.maxSets = GPU_DATA_BINDINGS_COUNT,
	};

	return vkCreateDescriptorPool(_device, &pool_info, NULL, &_descriptor_pool) == VK_SUCCESS;
}
static bool _create_descriptor_set_layout()
{
	VkDescriptorSetLayoutBinding verticies_layout_binding = {
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.binding = 0,
		.descriptorCount = 1,
	};

	VkDescriptorSetLayoutBinding indices_layout_binding = {
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.binding = 1,
		.descriptorCount = 1,
	};

	VkDescriptorSetLayoutBinding uniform_data_layout_binding = {
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
		.binding = 2,
		.descriptorCount = 1,
	};

	VkDescriptorSetLayoutBinding particles_layout_binding = {
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.binding = 3,
		.descriptorCount = 1,
	};

	VkDescriptorSetLayoutBinding bindings[GPU_DATA_BINDINGS_COUNT] = {
		verticies_layout_binding,
		indices_layout_binding,
		uniform_data_layout_binding,
		particles_layout_binding,
	};

	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.pBindings = bindings,
		.bindingCount = GPU_DATA_BINDINGS_COUNT,
	};

	return vkCreateDescriptorSetLayout(_device, &descriptor_set_layout_ci, NULL, &_descriptor_set_layout) == VK_SUCCESS;
}
static bool _create_descriptor_sets()
{
	VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = _descriptor_pool,
		.pSetLayouts = &_descriptor_set_layout,
		.descriptorSetCount = 1,
	};

	CHECK_VK_RESULT(vkAllocateDescriptorSets(_device, &alloc_info, &_descriptor_set));

	VkDescriptorBufferInfo vertex_buffer_info = {
		.buffer = _device_vertex_buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};

	VkDescriptorBufferInfo index_buffer_info = {
		.buffer = _device_index_buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};

	VkDescriptorBufferInfo mvp_buffer_info = {
		.buffer = _device_uniform_data_buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};

	VkDescriptorBufferInfo particle_buffer_info = {
		.buffer = _device_particle_buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};

	VkWriteDescriptorSet vertex_write_descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = _descriptor_set,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &vertex_buffer_info,
	};

	VkWriteDescriptorSet index_write_descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = _descriptor_set,
		.dstBinding = 1,
		.dstArrayElement = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &index_buffer_info,
	};

	VkWriteDescriptorSet uniform_data_write_descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = _descriptor_set,
		.dstBinding = 2,
		.dstArrayElement = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &mvp_buffer_info,
	};

	VkWriteDescriptorSet particle_write_descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = _descriptor_set,
		.dstBinding = 3,
		.dstArrayElement = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &particle_buffer_info,
	};

	VkWriteDescriptorSet write_descriptor_sets[GPU_DATA_BINDINGS_COUNT] = {
		vertex_write_descriptor_set,
		index_write_descriptor_set,
		uniform_data_write_descriptor_set,
		particle_write_descriptor_set
	};

	vkUpdateDescriptorSets(_device, GPU_DATA_BINDINGS_COUNT, write_descriptor_sets, 0, NULL);

	return true;
}
static bool _create_uniform_data_buffer()
{
	VkDeviceSize buffer_size = sizeof(UniformData);

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_host_uniform_data_buffer,
			&_host_uniform_data_buffer_memory
		)
	);

	void *data;

	vkMapMemory(_device, _host_uniform_data_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, &_uniform_data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_uniform_data_buffer_memory);

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_device_uniform_data_buffer,
			&_device_uniform_data_buffer_memory
		)
	);

	return _copy_buffer(&_device_uniform_data_buffer, &_host_uniform_data_buffer, buffer_size);
}
static bool _create_vertex_buffer()
{
	VkDeviceSize buffer_size = sizeof(Vertex) * _vertices.count;

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_host_vertex_buffer,
			&_host_vertex_buffer_memory
		)
	);

	void* data;

	vkMapMemory(_device, _host_vertex_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, _vertices.data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_vertex_buffer_memory);

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_device_vertex_buffer,
			&_device_vertex_buffer_memory
		)
	);

	return _copy_buffer(&_device_vertex_buffer, &_host_vertex_buffer, buffer_size);
}
static bool _create_index_buffer()
{
	VkDeviceSize buffer_size = sizeof(uint32_t) * _indices.count;

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_host_index_buffer,
			&_host_index_buffer_memory
		)
	);

	void* data;

	vkMapMemory(_device, _host_index_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, _indices.data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_index_buffer_memory);

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_device_index_buffer,
			&_device_index_buffer_memory
		)
	);

	return _copy_buffer(&_device_index_buffer, &_host_index_buffer, buffer_size);
}
static bool _create_particle_buffer()
{
	VkDeviceSize buffer_size = sizeof(Particle) * PARTICLE_COUNT;

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_host_particle_buffer,
			&_host_particle_buffer_memory
		)
	);

	void* data;

	vkMapMemory(_device, _host_particle_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, _particles, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_particle_buffer_memory);

	CHECK_RESULT(
		_create_memory_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_device_particle_buffer,
			&_device_particle_buffer_memory
		)
	);

	return _copy_buffer(&_device_particle_buffer, &_host_particle_buffer, buffer_size);
}
static bool _create_framebuffers()
{
	_swap_chain_framebuffers.count = _swap_chain_image_views.count;
	_swap_chain_framebuffers.data = malloc(sizeof(SwapChainFramebuffers) * _swap_chain_framebuffers.count);

	for (uint32_t i = 0; i < _swap_chain_framebuffers.count; i++)
	{
		VkImageView attachments[2] = {
			_swap_chain_image_views.data[i],
			_depth_image_view
		};

		VkFramebufferCreateInfo framebufferInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = _render_pass,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.width = _swap_chain_image_extent.width,
			.height = _swap_chain_image_extent.height,
			.layers = 1,
		};

		CHECK_VK_RESULT(vkCreateFramebuffer(_device, &framebufferInfo, NULL, _swap_chain_framebuffers.data + i));
	}

	return true;
}
static bool _create_command_pools_and_allocate_buffers()
{
	VkCommandPoolCreateInfo long_live_buffers_pool_ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = _operation_queue_families.graphics_family_idx,  // _operation_queue_families.use_same_family == true
	};

	VkCommandPoolCreateInfo one_time_buffers_pool_ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = _operation_queue_families.graphics_family_idx,  // _operation_queue_families.use_same_family == true
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
	};

	_one_time_command_buffer_idx = 0;
	_image_draw_command_buffers_begin_idx = 1;
	_compute_command_buffer_idx = _image_draw_command_buffers_begin_idx + _swap_chain_images.count;

	_command_buffers.count = 2 + _swap_chain_images.count;
	_command_buffers.data = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * _command_buffers.count);

	CHECK_VK_RESULT(vkCreateCommandPool(_device, &long_live_buffers_pool_ci, NULL, &_long_live_buffers_pool));
	CHECK_VK_RESULT(vkCreateCommandPool(_device, &one_time_buffers_pool_ci, NULL, &_one_time_buffers_pool));

	VkCommandBufferAllocateInfo logn_live_buffers_ai = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = _long_live_buffers_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = _command_buffers.count - 1,
	};

	VkCommandBufferAllocateInfo one_time_buffer_ai = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = _one_time_buffers_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	return (
		vkAllocateCommandBuffers(_device, &logn_live_buffers_ai, _command_buffers.data + 1) == VK_SUCCESS &&
		vkAllocateCommandBuffers(_device, &one_time_buffer_ai, _command_buffers.data) == VK_SUCCESS
	);
}
static bool _write_image_draw_command_buffers()
{
	for (uint32_t i = 0; i < _swap_chain_images.count; i++)
	{
		uint32_t idx = _image_draw_command_buffers_begin_idx + i;

		VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
			.pInheritanceInfo = NULL, // Optional
		};
		vkBeginCommandBuffer(_command_buffers.data[idx], &beginInfo);

		VkRenderPassBeginInfo renderPassInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = _render_pass,
			.framebuffer = _swap_chain_framebuffers.data[i],
			.renderArea.offset = { 0, 0 },
			.renderArea.extent = _swap_chain_image_extent,
		};

		VkClearValue clearValues[2];
		VkClearColorValue clear_color = {
			.float32 = { 0.0f, 0.0f, 0.0f, 0.0f },
		};
		VkClearDepthStencilValue clear_depth_stencil = {
			.depth = 1.0f,
			.stencil = 0,
		};
		clearValues[0].color = clear_color;
		clearValues[1].depthStencil = clear_depth_stencil;

		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(_command_buffers.data[idx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(_command_buffers.data[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

		VkDeviceSize offsets[] = { 0 };

		vkCmdBindDescriptorSets(
			_command_buffers.data[idx],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			_graphics_pipeline_layout, 0, 1,
			&_descriptor_set, 0, NULL
		);

		vkCmdBindVertexBuffers(_command_buffers.data[idx], 0, 1, &_device_vertex_buffer, offsets);
		vkCmdBindIndexBuffer(_command_buffers.data[idx], _device_index_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(_command_buffers.data[idx], _indices.count, 1, 0, 0, 0);
		vkCmdEndRenderPass(_command_buffers.data[idx]);

		CHECK_VK_RESULT(vkEndCommandBuffer(_command_buffers.data[idx]));
	}

	return true;
}
static bool _write_compute_command_buffer()
{
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};

	CHECK_VK_RESULT(
		vkBeginCommandBuffer(
			_command_buffers.data[_compute_command_buffer_idx],
			&beginInfo
		)
	);

	/*
		Add memory barrier to ensure that the (graphics) vertex shader
		has fetched attributes before compute starts to write to the buffer.
	*/
	VkBufferMemoryBarrier bufferMemoryBarrier = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.buffer = _device_particle_buffer,
		.size = VK_WHOLE_SIZE,
		.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,  // Vertex shader invocations have finished reading from the buffer
		.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,           // Compute shader wants to write to the buffer
		/*
			Compute and graphics queue may have different queue families.
			For the barrier to work across different queues,
			we need to set their family indices.
		*/
		.srcQueueFamilyIndex = _operation_queue_families.graphics_family_idx,  // Required as compute and graphics queue may have different families
		.dstQueueFamilyIndex = _operation_queue_families.compute_family_idx,    // Required as compute and graphics queue may have different families
	};

	vkCmdPipelineBarrier(
		_command_buffers.data[_compute_command_buffer_idx],
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE,
		0, NULL,
		1, &bufferMemoryBarrier,
		0, NULL
	);

	vkCmdBindPipeline(
		_command_buffers.data[_compute_command_buffer_idx],
		VK_PIPELINE_BIND_POINT_COMPUTE,
		_compute_pipeline
	);
	vkCmdBindDescriptorSets(
		_command_buffers.data[_compute_command_buffer_idx],
		VK_PIPELINE_BIND_POINT_COMPUTE,
		_compute_pipeline_layout, 0, 1, &_descriptor_set, 0, 0
	);

	// Dispatch the compute job
	vkCmdDispatch(_command_buffers.data[_compute_command_buffer_idx], PARTICLE_COUNT, 1, 1);

	// Add memory barrier to ensure that compute shader has finished writing to the buffer
	// Without this the (rendering) vertex shader may display incomplete results (partial data from last frame) 
	bufferMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;          // Compute shader has finished writes to the buffer
	bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT; // Vertex shader invocations want to read from the buffer
	bufferMemoryBarrier.buffer = _device_particle_buffer;
	bufferMemoryBarrier.size = VK_WHOLE_SIZE;
	bufferMemoryBarrier.srcQueueFamilyIndex = _operation_queue_families.compute_family_idx;
	bufferMemoryBarrier.dstQueueFamilyIndex = _operation_queue_families.graphics_family_idx;

	vkCmdPipelineBarrier(
		_command_buffers.data[_compute_command_buffer_idx],
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VK_FLAGS_NONE,
		0, NULL, 1, &bufferMemoryBarrier, 0, NULL
	);

	return vkEndCommandBuffer(_command_buffers.data[_compute_command_buffer_idx]) == VK_SUCCESS;
}
static bool _create_semaphores()
{
	VkSemaphoreCreateInfo semaphore_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	return (
		vkCreateSemaphore(_device, &semaphore_ci, NULL, &_image_available) == VK_SUCCESS &&
		vkCreateSemaphore(_device, &semaphore_ci, NULL, &_render_finished) == VK_SUCCESS
	);
}
static bool _create_compute_finished_fence()
{
	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	return vkCreateFence(_device, &fence_ci, NULL, &_compute_finished_fence) == VK_SUCCESS;
}
static bool _draw_frame()
{
	vkResetFences(_device, 1, &_compute_finished_fence);

	VkSubmitInfo compute_queue_submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = NULL,
		.commandBufferCount = 1,
		.pCommandBuffers = _command_buffers.data + _compute_command_buffer_idx,
	};

	if (
		vkQueueSubmit(
			_compute_queue,
			1,
			&compute_queue_submit_info,
			_compute_finished_fence
		) != VK_SUCCESS
	) {
		return false;
	}

	uint32_t image_index;

	if (
		vkAcquireNextImageKHR(
			_device,
			_swap_chain,
			1000,
			_image_available,
			VK_NULL_HANDLE,
			&image_index
		) != VK_SUCCESS
	) {
		return false;
	}

	VkFlags wait_dst_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo graphics_queue_submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_image_available,
		.pWaitDstStageMask = &wait_dst_stage_flags,
		.commandBufferCount = 1,
		.pCommandBuffers = (
			_command_buffers.data + _image_draw_command_buffers_begin_idx + image_index
		),
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &_render_finished,
	};

	vkWaitForFences(_device, 1, &_compute_finished_fence, VK_TRUE, UINT64_MAX);

	if (vkQueueSubmit(_graphics_queue, 1, &graphics_queue_submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		return false;
	}

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_render_finished,
		.swapchainCount = 1,
		.pSwapchains = &_swap_chain,
		.pImageIndices = &image_index,
	};

	return vkQueuePresentKHR(_present_queue, &presentInfo) == VK_SUCCESS;
}
static void _window_resize_callback(GLFWwindow* window, int width, int height)
{
	vkDeviceWaitIdle(_device);

	if (
		_create_swap_chain() &&
		_create_render_pass() &&
		_create_graphics_pipeline() &&
		_create_framebuffers() &&
		_write_image_draw_command_buffers()
	) {
		_draw_frame();
	}
	else {
		printf("Failed to resize window.\n");

		glfwSetWindowShouldClose(_window, true);
	}
}
static bool _init_window()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // enable resizing

	_window = glfwCreateWindow(_DEFAULT_WINDOW_WIDTH, _DEFAULT_WINDOW_HEIGHT, "zGame", NULL, NULL);

	if (!_window)
	{
		return false;
	}

	glfwSetWindowSizeCallback(_window, _window_resize_callback);

	return true;
}
static bool _update_uniform_data_buffer()
{
	VkDeviceSize buffer_size = sizeof(UniformData);

	void *data;

	vkMapMemory(_device, _host_uniform_data_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, &_uniform_data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_uniform_data_buffer_memory);

	return _copy_buffer(&_device_uniform_data_buffer, &_host_uniform_data_buffer, buffer_size);

	return true;
}

/* Module interface */

bool setup_window_and_gpu()
{
	CHECK_GLFW_RESULT_M(glfwInit(), "Failed to initialize GLFW.\n"); // to know which extensions GLFW requires

	_setup_required_extensions();

	CHECK_RESULT_M(_instance_supports_required_extensions(), "Some extensions are not supported.");

#ifdef _DEBUG
	_required_validation_layers.names = (char**)malloc(sizeof(char*) * VALIDATION_LAYERS_COUNT);
	_required_validation_layers.names[0] = "VK_LAYER_LUNARG_standard_validation";
	_required_validation_layers.count = VALIDATION_LAYERS_COUNT;

	CHECK_RESULT_M(_instance_supports_required_layers(), "Validation layers are not supported.")
#endif

	CHECK_RESULT_M(_create_instance(), "Failed to create instance.");

#ifdef _DEBUG
	CHECK_RESULT_M(_setup_debug_callback(), "Failed to set up debug callback.");
#endif

	CHECK_RESULT_M(_init_window(), "Failed to init window.");
	CHECK_VK_RESULT_M(
		glfwCreateWindowSurface(_instance, _window, NULL, &(_surface)),
		"Failed to create surface."
	);

	_required_physical_device_extensions.names = (char**)malloc(sizeof(char*) * PHYSICAL_DEVICE_EXTENSIONS_COUNT);
	_required_physical_device_extensions.names[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	_required_physical_device_extensions.count = PHYSICAL_DEVICE_EXTENSIONS_COUNT;

	CHECK_RESULT_M(_pick_physical_device(), "Failed to find suitable GPU.");
	CHECK_RESULT_M(_create_logical_device(), "Failed to create logical device.");
	CHECK_RESULT_M(_create_swap_chain(), "Failed to create swap chain.");
	CHECK_RESULT_M(
		_create_command_pools_and_allocate_buffers(),
		"Failed to create command pool or allocate buffers."
	);
	CHECK_RESULT_M(_create_depth_resources(), "Failed to create depth resources.");
	CHECK_RESULT_M(_create_vertex_buffer(), "Failed to create vertex buffer.");
	CHECK_RESULT_M(_create_index_buffer(), "Failed to create index buffer.");
	CHECK_RESULT_M(_create_particle_buffer(), "Failed to create particle buffer.");
	CHECK_RESULT_M(_create_uniform_data_buffer(), "Failed to create uniform data buffer.");
	CHECK_RESULT_M(_create_descriptor_pool(), "Failed to create descriptor pool.");
	CHECK_RESULT_M(_create_descriptor_set_layout(), "Failed to create descriptor set layout.");
	CHECK_RESULT_M(_create_descriptor_sets(), "Failed to allocate descriptor sets.");
	CHECK_RESULT_M(_create_render_pass(), "Failed to create render pass.");
	CHECK_RESULT_M(_create_compute_pipeline(), "Failed to create compute pipeline.");
	CHECK_RESULT_M(_create_compute_finished_fence(), "Failed to create compute finished fence.");
	CHECK_RESULT_M(_create_graphics_pipeline(), "Failed to create graphics pipeline.");
	CHECK_RESULT_M(_create_framebuffers(), "Failed to create swap chain framebuffers.");
	CHECK_RESULT_M(_create_semaphores(), "Failed to create semaphores.");
	CHECK_RESULT_M(_write_image_draw_command_buffers(), "Couldn't write draw commands.");
	CHECK_RESULT_M(_write_compute_command_buffer(), "Couldn't write compute commands.");

	return true;
}

void create_particles()
{
	_particles = (Particle*)malloc(sizeof(Particle) * PARTICLE_COUNT);

	Particle p0 = {
		.position = { 0.5f, 0.5f, 0.0f, 1.0f },
		.color = { 1.0f, 0.0f, 0.0f, 1.0f },
	};
	_particles[0] = p0;

	Particle p1 = {
		.position = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color = { 0.0f, 1.0f, 0.0f, 1.0f },
	};
	_particles[1] = p1;

	Particle p2 = {
		.position = { 0.5f, 0.0f, 0.0f, 1.0f },
		.color = { 0.0f, 0.0f, 1.0f, 1.0f },
	};
	_particles[2] = p2;

	Particle p3 = {
		.position = { 0.0f, 0.5f, 0.0f, 1.0f },
		.color = { 1.0f, 1.0f, 1.0f, 1.0f },
	};
	_particles[3] = p3;

	Particle p4 = {
		.position = { 0.5f, 0.5f, 0.5f, 1.0f },
		.color = { 1.0f, 1.0f, 0.0f, 1.0f },
	};
	_particles[4] = p4;

	Particle p5 = {
		.position = { 0.0f, 0.0f, 0.5f, 1.0f },
		.color = { 0.0f, 1.0f, 1.0f, 1.0f },
	};
	_particles[5] = p5;

	Particle p6 = {
		.position = { 0.5f, 0.0f, 0.5f, 1.0f },
		.color = { 1.0f, 0.0f, 1.0f, 1.0f },
	};
	_particles[6] = p6;

	Particle p7 = {
		.position = { 0.0f, 0.5f, 0.5f, 1.0f },
		.color = { 0.7f, 0.2f, 0.1f, 1.0f },
	};
	_particles[7] = p7;

	_vertices.count = PARTICLE_COUNT * 4;
	_vertices.data = (Vertex*)malloc(sizeof(Vertex) * _vertices.count);

	_indices.count = PARTICLE_COUNT * 6;
	_indices.data = (uint32_t*)malloc(sizeof(uint32_t) * _indices.count);

	update_perspective_projection_matrix(
		&_projection,
		(float)M_PI / 2.0f,
		(float)(_DEFAULT_WINDOW_WIDTH / _DEFAULT_WINDOW_HEIGHT),
		0.1f,
		10.0f
	);

	update_view_matrix(&_view, &_eye, &_look_at, &_up);

	_uniform_data.model = _model;
	_uniform_data.view = _view;
	_uniform_data.projection = _projection;

	_uniform_data.particle_count = PARTICLE_COUNT;
	_uniform_data.particle_radius = 0.08f;
}
void destroy_particles()
{
	free(_particles);
	free(_vertices.data);
	free(_indices.data);
}
void render()
{
	bool draw_success = true;

	clock_t t0, t1;
	float time_diff = 0.0f;
	Vector3 rotation_axis = { .x = 1.0f, .y = 0.0f, .z = 0.0f };
	Quaternion base = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };

	while (!glfwWindowShouldClose(_window) && draw_success)
	{
		glfwPollEvents();

		t0 = clock();

		Quaternion rotated = get_quaternion(((float)M_PI / 2.0f) * time_diff, &rotation_axis);
		base = get_multiplied_q(&base, &rotated);

		_uniform_data.model = get_transform(&base);

		draw_success = (
			_update_uniform_data_buffer() &&
			_draw_frame()
		);

		t1 = clock();

		time_diff = (float)(t1 - t0) / CLOCKS_PER_SEC;
	}

	vkDeviceWaitIdle(_device);
}
void destroy_window_and_free_gpu()
{
	vkDestroyDescriptorSetLayout(_device, _descriptor_set_layout, NULL);
	vkDestroyDescriptorPool(_device, _descriptor_pool, NULL);

	vkDestroyBuffer(_device, _host_particle_buffer, NULL);
	vkFreeMemory(_device, _host_particle_buffer_memory, NULL);
	vkDestroyBuffer(_device, _device_particle_buffer, NULL);
	vkFreeMemory(_device, _device_particle_buffer_memory, NULL);

	vkDestroyBuffer(_device, _host_uniform_data_buffer, NULL);
	vkFreeMemory(_device, _host_uniform_data_buffer_memory, NULL);
	vkDestroyBuffer(_device, _device_uniform_data_buffer, NULL);
	vkFreeMemory(_device, _device_uniform_data_buffer_memory, NULL);

	vkDestroyBuffer(_device, _host_vertex_buffer, NULL);
	vkFreeMemory(_device, _host_vertex_buffer_memory, NULL);
	vkDestroyBuffer(_device, _device_vertex_buffer, NULL);
	vkFreeMemory(_device, _device_vertex_buffer_memory, NULL);

	vkDestroyBuffer(_device, _host_index_buffer, NULL);
	vkFreeMemory(_device, _host_index_buffer_memory, NULL);
	vkDestroyBuffer(_device, _device_index_buffer, NULL);
	vkFreeMemory(_device, _device_index_buffer_memory, NULL);

	vkDestroyCommandPool(_device, _long_live_buffers_pool, NULL);
	vkDestroyCommandPool(_device, _one_time_buffers_pool, NULL);

	vkDestroyFence(_device, _compute_finished_fence, NULL);
	vkDestroySemaphore(_device, _image_available, NULL);
	vkDestroySemaphore(_device, _render_finished, NULL);

	free(_vertex_shader_code);
	free(_fragment_shader_code);
	free(_compute_shader_code);

	for (uint32_t i = 0; i < _swap_chain_framebuffers.count; i += 1)
	{
		vkDestroyFramebuffer(_device, _swap_chain_framebuffers.data[i], NULL);
	}

	free(_swap_chain_framebuffers.data);
	
	vkDestroyImageView(_device, _depth_image_view, NULL);
	vkDestroyImage(_device, _depth_image, NULL);
	vkFreeMemory(_device, _depth_image_memory, NULL);
	vkDestroyShaderModule(_device, _vertex_shader, NULL);
	vkDestroyShaderModule(_device, _fragment_shader, NULL);
	vkDestroyShaderModule(_device, _compute_shader, NULL);
	vkDestroyPipeline(_device, _compute_pipeline, NULL);
	vkDestroyPipelineLayout(_device, _compute_pipeline_layout, NULL);
	vkDestroyPipeline(_device, _graphics_pipeline, NULL);
	vkDestroyPipelineLayout(_device, _graphics_pipeline_layout, NULL);
	vkDestroyRenderPass(_device, _render_pass, NULL);

	for (uint32_t i = 0; i < _swap_chain_image_views.count; i += 1)
	{
		vkDestroyImageView(_device, _swap_chain_image_views.data[i], NULL);
	}

	free(_swap_chain_image_views.data);
	free(_swap_chain_images.data);

	vkDestroySwapchainKHR(_device, _swap_chain, NULL);
	vkDestroyDevice(_device, NULL);

	free(_surface_formats.data);
	free(_present_modes.data);

	free((void *)_required_instance_extensions.names);
	free((void*)_required_physical_device_extensions.names);

	vkDestroySurfaceKHR(_instance, _surface, NULL);

#ifdef _DEBUG
	free((void*)_required_validation_layers.names);

	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		_instance, "vkDestroyDebugReportCallbackEXT"
	);

	func(_instance, _debug_callback_object, NULL);
#endif

	vkDestroyInstance(_instance, NULL);
}