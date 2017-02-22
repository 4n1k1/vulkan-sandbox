#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "system_bridge.h"
#include "math3d.h"

#ifdef NDEBUG
const bool _enable_validation_layers = false;
#else
const bool _enable_validation_layers = true;
#endif

#define DEVICE_QUEUES_COUNT 3
#define GPU_DATA_BINDINGS_COUNT 3
#define PARTICLE_COUNT 8

/*
	Core concerns.

	1) Code requires GPU to be able to present, render and compute;
	2) Command pools/buffers are not optimized (
		typedef enum VkCommandPoolCreateFlagBits {
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = 0x00000001,
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002,
		} VkCommandPoolCreateFlagBits;
	).
*/

/* Module state */

static const uint32_t _DEFAULT_WINDOW_WIDTH = 800;
static const uint32_t _DEFAULT_WINDOW_HEIGHT = 600;

static const float _vertical_fov = 50.0f;
static const float _display_aspect_ratio = 16.0f / 9.0f;

static const Vector_3 _look_at = { .x = 0.0f, .y = 0.0f, .z = 1.0f };  // look at vector
static const Vector_3 _eye = { .x = 0.0f, .y = 0.0f, .z = 0.0f };      // eye position
static const Vector_3 _up = { .x = 0.0f, .y = 1.0f,.y =  0.0f };       // up vector

static Matrix_4x4 _projection = {
	.data = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static Matrix_4x4 _view = {
	.data = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static Matrix_4x4 _model = {
	.data = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static RequiredValidationLayers _required_validation_layers = {
	.names = { "VK_LAYER_LUNARG_standard_validation" },
	.count = 1,
};

static RequiredPhysicalDeviceExtensions _required_physical_device_extensions = {
	.names = { VK_KHR_SWAPCHAIN_EXTENSION_NAME },
	.count = 1,
};

static OperationQueueFamilies _operation_queue_families = {
	.graphics_family_idx = -1,
	.compute_family_idx = -1,
	.present_family_idx = -1,

	.use_same_family = false,
};

static GLFWwindow *_window = NULL;
static RequiredGLFWExtensions _required_glfw_extensions;
static ExtensionsToEnable _instance_extensions_to_enable;
static VkInstance _instance;
static VkDebugReportCallbackEXT _debug_callback_object;
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
static bool _create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *image_memory) {
	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.extent.width = width,
		.extent.height = height,
		.extent.depth = 1,
		.mipLevels = 1,
		.arrayLayers = 1,
		.format = format,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vkCreateImage(_device, &imageInfo, NULL, image) != VK_SUCCESS)
	{
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
	};

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(_physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (
			memRequirements.memoryTypeBits & (1 << i) &&
			(memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			) {
			allocInfo.memoryTypeIndex = i;
		}
	}

	if (vkAllocateMemory(_device, &allocInfo, NULL, image_memory) != VK_SUCCESS)
	{
		return false;
	}

	vkBindImageMemory(_device, _depth_image, _depth_image_memory, 0);

	return true;
}
static bool _create_image_view(const VkImage *image, VkImageView *imageView, VkFormat format, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = *image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
		.subresourceRange.aspectMask = aspectFlags,
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
			return *memory_type = i;

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
	if (!_begin_one_time_command()) { return false; }

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

static VkBool32 __stdcall _debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, uint32_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	printf("Validation layer error: %s.\n", msg);

	return VK_FALSE;
}

static void _set_extensions_to_enable()
{
	uint32_t extensions_num = _required_glfw_extensions.count + 1;

	_instance_extensions_to_enable.names = malloc(
		sizeof(char **) * extensions_num
	);

	uint32_t extension_to_enable_idx = 0;

	for (uint32_t i = 0; i < _required_glfw_extensions.count; i += 1)
	{
		_instance_extensions_to_enable.names[extension_to_enable_idx] = _required_glfw_extensions.names[i];
		extension_to_enable_idx += 1;
	}

	_instance_extensions_to_enable.names[extension_to_enable_idx] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

	_instance_extensions_to_enable.count = extensions_num;
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
	for (uint32_t i = 0; i < _present_modes.count; i += 1) {
		if (_present_modes.data[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			*present_mode = _present_modes.data[i];
		}
	}

	*present_mode = VK_PRESENT_MODE_FIFO_KHR;
}

static bool _instance_supports_required_layers()
{
	uint32_t supported_layers_num;
	vkEnumerateInstanceLayerProperties(&supported_layers_num, NULL);

	VkLayerProperties *supported_layers = malloc(sizeof(VkLayerProperties) * supported_layers_num);

	vkEnumerateInstanceLayerProperties(&supported_layers_num, supported_layers);

	bool result = true;

	for (uint32_t i = 0; i < _required_validation_layers.count; i += 1) {
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
static bool _instance_supports_required_extensions()
{
	uint32_t supported_exts_num = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_num, NULL);

	VkExtensionProperties *supported_exts = malloc(sizeof(VkExtensionProperties) * supported_exts_num);
	vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_num, supported_exts);

	bool result = true;

	for (uint32_t i = 0; i < _required_glfw_extensions.count; i += 1) {
		bool ext_found = false;

		for (uint32_t j = 0; j < supported_exts_num; j += 1) {
			if (strcmp(supported_exts[j].extensionName, _required_glfw_extensions.names[i]) == 0) {
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
		_present_modes.count != 0
	);

	return (
		device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		_operation_queue_families.use_same_family &&
		extensions_supported &&
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

	if (vkCreateBuffer(_device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(_device, *buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
	};

	if (!_find_memory_type(&(allocInfo.memoryTypeIndex), memRequirements.memoryTypeBits, properties))
	{
		return false;
	}

	if (vkAllocateMemory(_device, &allocInfo, NULL, buffer_memory) != VK_SUCCESS) {
		return false;
	}

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

	if (_enable_validation_layers)
	{
		create_info.enabledLayerCount = _required_validation_layers.count;
		create_info.ppEnabledLayerNames = _required_validation_layers.names;
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	create_info.enabledExtensionCount = _instance_extensions_to_enable.count;
	create_info.ppEnabledExtensionNames = _instance_extensions_to_enable.names;

	return VK_SUCCESS == vkCreateInstance(&create_info, NULL, &_instance);
}
static bool _setup_debug_callback()
{
	if (!_enable_validation_layers) return true;

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
		.enabledExtensionCount = 0
	};

/*
	if (_enable_validation_layers) {
		create_info.enabledLayerCount = _required_validation_layers.count;
		create_info.ppEnabledLayerNames = _required_validation_layers.names;
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}
*/

	create_info.enabledExtensionCount = _required_physical_device_extensions.count;
	create_info.ppEnabledExtensionNames = _required_physical_device_extensions.names;

	if (
		vkCreateDevice(_physical_device, &create_info, NULL, &_device) != VK_SUCCESS
	) {
		return false;
	}

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

	if (vkCreateSwapchainKHR(_device, &createInfo, NULL, &(_swap_chain)) != VK_SUCCESS)
	{
		return false;
	}

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
		if (!_create_image_view(
			_swap_chain_images.data + i,
			_swap_chain_image_views.data + i,
			_swap_chain_image_format,
			VK_IMAGE_ASPECT_COLOR_BIT)
		) {
			return false;
		}
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

	if (!_pick_depth_buffer_format())
	{
		return false;
	}

	VkAttachmentDescription depth_attachment = {
		.format = _depth_image_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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
		.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
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
	if (!_create_shader_module(&_vertex_shader, "../../src/shaders/vertex.spv", _vertex_shader_code))
	{
		return false;
	}

	if (!_create_shader_module(&_fragment_shader, "../../src/shaders/fragment.spv", _fragment_shader_code))
	{
		return false;
	}

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
		.stride = sizeof(Vector_3),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription attribute_descriptions[1];

	attribute_descriptions[0].binding = 0;
	attribute_descriptions[0].location = 0;
	attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[0].offset = 0;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.vertexAttributeDescriptionCount = 1,
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
		.depthBoundsTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f, // Optional
		.maxDepthBounds = 1.0f, // Optional
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
		.polygonMode = VK_POLYGON_MODE_FILL, // VK_POLYGON_MODE_LINE or VK_POLYGON_MODE_POINT
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

	if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, NULL, &_graphics_pipeline_layout) != VK_SUCCESS)
	{
		return false;
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = NULL, // Optional
		.pColorBlendState = &colorBlending,
		.pDynamicState = NULL, // Optional
		.layout = _graphics_pipeline_layout,
		.renderPass = _render_pass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1, // Optional
		.pDepthStencilState = &depthStencil,
	};

	return vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &_graphics_pipeline) == VK_SUCCESS;
}
static bool _create_compute_pipeline()
{
	if (!_create_shader_module(&_compute_shader, "../../src/shaders/compute.spv", _compute_shader_code))
	{
		return false;
	}

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

	if (
		vkCreatePipelineLayout(
			_device,
			&pipeline_layout_ci,
			NULL,
			&_compute_pipeline_layout
		) != VK_SUCCESS
	) {
		return false;
	}

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
	if (!_create_image(
		_swap_chain_image_extent.width,
		_swap_chain_image_extent.height,
		_depth_image_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&_depth_image,
		&_depth_image_memory))
	{
		return false;
	}
	
	if (!_create_image_view(&_depth_image, &_depth_image_view, _depth_image_format, VK_IMAGE_ASPECT_DEPTH_BIT))
	{
		return false;
	}

	return true;
}
static bool _create_descriptor_pool()
{
	VkDescriptorPoolSize uniform_buffer_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
	};

	VkDescriptorPoolSize storage_buffer_size = {
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 2,
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

	VkDescriptorSetLayoutBinding bindings[GPU_DATA_BINDINGS_COUNT] = {
		verticies_layout_binding,
		indices_layout_binding,
		uniform_data_layout_binding,
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

	if (vkAllocateDescriptorSets(_device, &alloc_info, &_descriptor_set) != VK_SUCCESS)
	{
		return false;
	}

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

	VkWriteDescriptorSet write_descriptor_sets[GPU_DATA_BINDINGS_COUNT] = {
		vertex_write_descriptor_set,
		index_write_descriptor_set,
		uniform_data_write_descriptor_set,
	};

	vkUpdateDescriptorSets(_device, GPU_DATA_BINDINGS_COUNT, write_descriptor_sets, 0, NULL);
	
	return true;
}
static bool _create_uniform_data_buffer()
{
	VkDeviceSize buffer_size = sizeof(UniformData);

	if (!_create_memory_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&_host_uniform_data_buffer,
		&_host_uniform_data_buffer_memory
	)) {
		return false;
	}

	void* data;

	vkMapMemory(_device, _host_uniform_data_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, &_uniform_data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_uniform_data_buffer_memory);

	if (!_create_memory_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&_device_uniform_data_buffer,
		&_device_uniform_data_buffer_memory
	)) {
		return false;
	}

	return _copy_buffer(&_device_uniform_data_buffer, &_host_uniform_data_buffer, buffer_size);
}
static bool _create_vertex_buffer()
{
	VkDeviceSize buffer_size = sizeof(float) * _vertices.count;

	if (!_create_memory_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&_host_vertex_buffer,
		&_host_vertex_buffer_memory
	)) {
		return false;
	}

	void* data;

	vkMapMemory(_device, _host_vertex_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, _vertices.data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_vertex_buffer_memory);

	if (!_create_memory_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&_device_vertex_buffer,
		&_device_vertex_buffer_memory
	)) {
		return false;
	}

	return _copy_buffer(&_device_vertex_buffer, &_host_vertex_buffer, buffer_size);
}
static bool _create_index_buffer()
{
	VkDeviceSize buffer_size = sizeof(uint32_t) * _indices.count;

	if (!_create_memory_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&_host_index_buffer,
		&_host_index_buffer_memory
	)) {
		return false;
	}

	void* data;

	vkMapMemory(_device, _host_index_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, _indices.data, (uint32_t)buffer_size);
	vkUnmapMemory(_device, _host_index_buffer_memory);

	if (!_create_memory_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&_device_index_buffer,
		&_device_index_buffer_memory
	)) {
		return false;
	}

	return _copy_buffer(&_device_index_buffer, &_host_index_buffer, buffer_size);
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

		if (vkCreateFramebuffer(_device, &framebufferInfo, NULL, _swap_chain_framebuffers.data + i) != VK_SUCCESS)
		{
			return false;
		}
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
	_image_draw_command_buffers_begin_idx = 0;
	_compute_command_buffer_idx = _image_draw_command_buffers_begin_idx + _swap_chain_framebuffers.count;

	_command_buffers.count = 2 + _swap_chain_framebuffers.count;
	_command_buffers.data = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * _command_buffers.count);

	if (
		vkCreateCommandPool(_device, &long_live_buffers_pool_ci, NULL, &_long_live_buffers_pool) != VK_SUCCESS ||
		vkCreateCommandPool(_device, &one_time_buffers_pool_ci, NULL, &_one_time_buffers_pool) != VK_SUCCESS)
	{
		return false;
	}

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
	for (uint32_t i = _image_draw_command_buffers_begin_idx; i < _command_buffers.count; i++)
	{
		VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
			.pInheritanceInfo = NULL, // Optional
		};
		vkBeginCommandBuffer(_command_buffers.data[i], &beginInfo);

		VkRenderPassBeginInfo renderPassInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = _render_pass,
			.framebuffer = _swap_chain_framebuffers.data[i],
			.renderArea.offset = { 0, 0 },
			.renderArea.extent = _swap_chain_image_extent,
		};

		VkClearValue clearValues[2];
		VkClearColorValue clear_color = {
			.float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
		};
		VkClearDepthStencilValue clear_depth_stencil = {
			.depth = 1.0f,
			.stencil = 0,
		};
		clearValues[0].color = clear_color;
		clearValues[1].depthStencil = clear_depth_stencil;

		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(_command_buffers.data[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(_command_buffers.data[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(_command_buffers.data[i], 0, 1, &_device_vertex_buffer, offsets);
		vkCmdBindIndexBuffer(_command_buffers.data[i], _device_index_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(_command_buffers.data[i], _indices.count, 1, 0, 0, 0);
		vkCmdEndRenderPass(_command_buffers.data[i]);

		if (vkEndCommandBuffer(_command_buffers.data[i]) != VK_SUCCESS) {
			return false;
		}
	}

	return true;
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
static bool _draw_frame()
{
	uint32_t image_index;

	if (vkAcquireNextImageKHR(_device, _swap_chain, 1000, _image_available, VK_NULL_HANDLE, &image_index) != VK_SUCCESS)
	{
		return false;
	}

	VkFlags wait_dst_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo = {
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

	if (vkQueueSubmit(_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
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
static void _update_uniform_data_buffer()
{

}

/* Module interface */

bool setup_window_and_gpu()
{
	if (!glfwInit()) // to know which extensions GLFW requires
	{
		printf("Failed to initialize GLFW.\n");
		return false;
	}

	_required_glfw_extensions.names = glfwGetRequiredInstanceExtensions(
		&(_required_glfw_extensions.count)
	);

	_set_extensions_to_enable();

	if (_enable_validation_layers) // to handle VK errors in our function
	{
		if (!_instance_supports_required_layers())
		{
			printf("Validation layers requested, but not available.\n");
			return false;
		}
	}

	if (!_instance_supports_required_extensions())
	{
		printf("Some extensions are not supported.\n");
		return false;
	}

	if (!_create_instance())
	{
		printf("Failed to create instance.\n");
		return false;
	}

	if (!_setup_debug_callback())
	{
		printf("Failed to set up debug callback.\n");
		return false;
	}

	if (!_init_window())
	{
		printf("Failed to init window.\n");
		return false;
	}

	if (VK_SUCCESS != glfwCreateWindowSurface(_instance, _window, NULL, &(_surface)))
	{
		printf("Failed to create surface.\n");
		return false;
	}

	if (!_pick_physical_device())
	{
		printf("Failed to find suitable GPU.\n");
		return false;
	}

	if (!_create_logical_device())
	{
		printf("Failed to create logical device.\n");
		return false;
	}

	if (!_create_swap_chain())
	{
		printf("Failed to create swap chain.\n");
		return false;
	}

	if (!_create_command_pools_and_allocate_buffers())
	{
		printf("Failed to create command pool or allocate buffers.\n");
		return false;
	}

	if (!_create_vertex_buffer())
	{
		printf("Failed to create vertex buffer.\n");
		return false;
	}

	if (!_create_index_buffer())
	{
		printf("Failed to create index buffer.\n");
		return false;
	}

	if (!_create_uniform_data_buffer())
	{
		printf("Failed to create uniform data buffer.\n");
		return false;
	}

	if (!_create_descriptor_pool())
	{
		printf("Failed to create descriptor pool.\n");
		return false;
	}

	if (!_create_descriptor_set_layout())
	{
		printf("Failed to create descriptor set layout.\n");
		return false;
	}

	if (!_create_descriptor_sets())
	{
		printf("Failed to allocate descriptor sets.\n");
		return false;
	}

	if (!_create_render_pass())
	{
		printf("Failed to create render pass.\n");
		return false;
	}

	if (!_create_graphics_pipeline())
	{
		printf("Failed to create graphics pipeline.\n");
		return false;
	}
	/*
	if (!_create_compute_pipeline())
	{
		printf("Failed to create compute pipeline.");
		return false;
	}
	*/
	if (!_create_depth_resources())
	{
		printf("Failed to create depth resources.\n");
		return false;
	}

	if (!_create_framebuffers())
	{
		printf("Failed to create swap chain framebuffers.\n");
		return false;
	}

	if (!_create_semaphores())
	{
		printf("Failed to create semaphores.\n");
		return false;
	}

	return true;
}

void create_particles()
{
	_particles = (Particle*)malloc(sizeof(Particle) * PARTICLE_COUNT);

	_particles[0].position.x = 0.0f;
	_particles[0].position.y = 0.0f;
	_particles[0].position.z = 0.0f;
	_particles[0].color_idx = 0;

	_particles[1].position.x = 1.0f;
	_particles[1].position.y = 0.0f;
	_particles[1].position.z = 0.0f;
	_particles[1].color_idx = 0;

	_particles[2].position.x = 1.0f;
	_particles[2].position.y = 0.0f;
	_particles[2].position.z = 1.0f;
	_particles[2].color_idx = 0;

	_particles[3].position.x = 0.0f;
	_particles[3].position.y = 0.0f;
	_particles[3].position.z = 1.0f;
	_particles[3].color_idx = 0;

	_particles[4].position.x = 0.0f;
	_particles[4].position.y = 1.0f;
	_particles[4].position.z = 0.0f;
	_particles[4].color_idx = 0;

	_particles[5].position.x = 1.0f;
	_particles[5].position.y = 1.0f;
	_particles[5].position.z = 0.0f;
	_particles[5].color_idx = 0;

	_particles[6].position.x = 1.0f;
	_particles[6].position.y = 1.0f;
	_particles[6].position.z = 1.0f;
	_particles[6].color_idx = 0;

	_particles[7].position.x = 0.0f;
	_particles[7].position.y = 1.0f;
	_particles[7].position.z = 1.0f;
	_particles[7].color_idx = 0;

	Vector_3 top_left = { .x = -0.5, .y = 0.5f, .z = 0.0f };
	Vector_3 bot_left = { .x = -0.5f, .y = -0.5f, .z = 0.0f };
	Vector_3 bot_right = { .x = 0.5f, .y = -0.5f, .z = 0.0f };
	Vector_3 top_right = { .x = 0.5f, .y = 0.5f, .z = 0.0f };

	_vertices.count = 4;
	_vertices.data = (Vector_3*)malloc(sizeof(Vector_3) * _vertices.count);

	_vertices.data[0] = top_left;
	_vertices.data[1] = bot_left;
	_vertices.data[2] = bot_right;
	_vertices.data[3] = top_right;

	_indices.count = 6;
	_indices.data = (uint32_t*)malloc(sizeof(uint32_t) * _indices.count);

	_indices.data[0] = 0;
	_indices.data[1] = 1;
	_indices.data[2] = 2;
	_indices.data[3] = 0;
	_indices.data[4] = 2;
	_indices.data[5] = 3;

	_uniform_data.mvp.model = _model;
	_uniform_data.mvp.view = _view;
	_uniform_data.mvp.projection = _projection;
	_uniform_data.color.red = 1.0f;
	_uniform_data.color.green = 0.0f;
	_uniform_data.color.blue = 0.0f;
	_uniform_data.color.alpha = 1.0f;

	/*
	_projection = get_perspective_projection_matrix(_vertical_fov, _display_aspect_ratio, 0.1f, 9.0f);
	_view = get_view_matrix(_eye, _look_at, _up);
	*/
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

	while (!glfwWindowShouldClose(_window) && draw_success)
	{
		glfwPollEvents();

		_update_uniform_data_buffer();
		draw_success = _draw_frame();
	}

	vkDeviceWaitIdle(_device);
}
void destroy_window_and_free_gpu()
{
	vkDestroyDescriptorSetLayout(_device, _descriptor_set_layout, NULL);
	vkDestroyDescriptorPool(_device, _descriptor_pool, NULL);

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

	vkDestroySemaphore(_device, _image_available, NULL);
	vkDestroySemaphore(_device, _render_finished, NULL);

	free(_vertex_shader_code);
	free(_fragment_shader_code);
	free(_compute_shader_code);

	vkFreeCommandBuffers(_device, _long_live_buffers_pool, _command_buffers.count, _command_buffers.data);
	free(_command_buffers.data);
	vkDestroyCommandPool(_device, _long_live_buffers_pool, NULL);

	for (uint32_t i = 0; i < _swap_chain_framebuffers.count; i += 1)
	{
		vkDestroyFramebuffer(_device, _swap_chain_framebuffers.data[i], NULL);
	}

	free(_swap_chain_framebuffers.data);
	
	vkDestroyImageView(_device, _depth_image_view, NULL);
	vkDestroyImage(_device, _depth_image, NULL);
	vkDestroyShaderModule(_device, _vertex_shader, NULL);
	vkDestroyShaderModule(_device, _fragment_shader, NULL);
	vkDestroyPipeline(_device, _graphics_pipeline, NULL);
	vkDestroyPipelineLayout(_device, _graphics_pipeline_layout, NULL);
	vkDestroyDescriptorSetLayout(_device, _descriptor_set_layout, NULL);
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
	free((void *)_instance_extensions_to_enable.names);

	vkDestroySurfaceKHR(_instance, _surface, NULL);

	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		_instance, "vkDestroyDebugReportCallbackEXT"
	);

	if (func != NULL) {
		func(_instance, _debug_callback_object, NULL);
	}

	vkDestroyInstance(_instance, NULL);
}