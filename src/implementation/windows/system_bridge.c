#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "z_game.h"
#include "system_bridge.h"

#ifdef NDEBUG
const bool _enable_validation_layers = false;
#else
const bool _enable_validation_layers = true;
#endif

static uint _WINDOW_WIDTH = 800;
static uint _WINDOW_HEIGHT = 600;

static RequiredGLFWExtensions _required_glfw_extensions;
static RequiredPhysicalDeviceExtensions _required_physical_device_extensions;
static ExtensionsToEnable _instance_extensions_to_enable;
static RequiredValidationLayers _required_validation_layers;
static VkInstance _instance;
static VkDebugReportCallbackEXT _debug_callback_object;
static GLFWwindow *_window = NULL;
static VkSurfaceKHR _surface;
static VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
static PhysicalDeviceQueueFamilies _physical_device_queue_families;
static PhysicalDeviceSwapChainSupport _physical_device_swap_chain_support;
static VkDevice _device;
static VkQueue _graphics_queue;
static VkQueue _present_queue;
static VkSwapchainKHR _swap_chain;
static SwapChainImages _swap_chain_images;
static SwapChainImageViews _swap_chain_image_views;
static VkFormat _color_buffer_format;
static VkFormat _depth_buffer_format;
static VkExtent2D _extent;
static VkRenderPass _render_pass;
static VkDescriptorSetLayout _descriptor_set_layout;
static VkShaderModule _vertex_shader;
static VkShaderModule _fragment_shader;
static VkPipeline _graphics_pipeline;

static inline bool _is_complete(const PhysicalDeviceQueueFamilies *indicies) {
	return indicies->graphics_family_idx >= 0 && indicies->present_family_idx >= 0;
}
static inline bool _use_same_family(const PhysicalDeviceQueueFamilies *indicies) {
	return indicies->graphics_family_idx == indicies->present_family_idx;
}

static void _on_window_resize(GLFWwindow* window, int width, int height)
{

}
static VkBool32 __stdcall _debug_callback_function(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData
)
{
	printf("Validation layer: %s.\n", msg);

	return VK_FALSE;
}

static void _set_required_validation_layers()
{
	_required_validation_layers.layer_names[0] = "VK_LAYER_LUNARG_standard_validation";
	_required_validation_layers.layers_num = 1;
}
static void _set_required_glfw_extensions()
{
	_required_glfw_extensions.extension_names = glfwGetRequiredInstanceExtensions(
		&(_required_glfw_extensions.exts_num)
	);
}
static void _set_required_physical_device_extensions()
{
	_required_physical_device_extensions.extension_names[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	_required_physical_device_extensions.exts_num = 1;
}
static void _set_extensions_to_enable()
{
	uint extensions_num = _required_glfw_extensions.exts_num + 1;

	_instance_extensions_to_enable.extension_names = malloc(
		sizeof(char **) * extensions_num
	);

	uint extension_to_enable_idx = 0;

	for (uint i = 0; i < _required_glfw_extensions.exts_num; i += 1)
	{
		_instance_extensions_to_enable.extension_names[extension_to_enable_idx] = _required_glfw_extensions.extension_names[i];
		extension_to_enable_idx += 1;
	}

	_instance_extensions_to_enable.extension_names[extension_to_enable_idx] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

	_instance_extensions_to_enable.exts_num = extensions_num;
}
static void _set_queue_family_properties(const VkPhysicalDevice *physical_device)
{
	uint32_t queue_families_num = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(*physical_device, &queue_families_num, NULL);

	VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_families_num);
	vkGetPhysicalDeviceQueueFamilyProperties(*physical_device, &queue_families_num, queue_families);

	for (uint i = 0; i < queue_families_num; i += 1)
	{
		if ((queue_families + i)->queueCount > 0 && (queue_families + i)->queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			_physical_device_queue_families.graphics_family_idx = i;
		}

		VkBool32 presentSupport;

		vkGetPhysicalDeviceSurfaceSupportKHR(*physical_device, i, _surface, &presentSupport);

		if ((queue_families + i)->queueCount > 0 && presentSupport)
		{
			_physical_device_queue_families.present_family_idx = i;
		}

		if (_is_complete(&_physical_device_queue_families))
		{
			break;
		}
	}

	free(queue_families);
}
static void _set_swap_chain_support(const VkPhysicalDevice *physical_device) {
	PhysicalDeviceSwapChainSupport *alias = &_physical_device_swap_chain_support;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physical_device, _surface, &(alias->capabilities));

	vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, _surface, &(alias->formats_num), NULL);

	if (alias->formats_num != 0) {
		alias->formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * alias->formats_num);

		vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, _surface, &(alias->formats_num), alias->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device, _surface, &(alias->present_modes_num), NULL);

	if (alias->present_modes_num != 0) {
		alias->present_modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * alias->present_modes_num);

		vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device, _surface, &(alias->present_modes_num), alias->present_modes);
	}
}

static bool _instance_supports_required_layers()
{
	uint supported_layers_num;
	vkEnumerateInstanceLayerProperties(&supported_layers_num, NULL);

	VkLayerProperties *supported_layers = malloc(sizeof(VkLayerProperties) * supported_layers_num);

	vkEnumerateInstanceLayerProperties(&supported_layers_num, supported_layers);

	bool result = true;

	for (uint i = 0; i < _required_validation_layers.layers_num; i += 1) {
		bool layer_found = false;

		for (uint j = 0; j < supported_layers_num; j += 1) {
			if (strcmp(supported_layers[j].layerName, _required_validation_layers.layer_names[i]) == 0) {
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
	uint supported_exts_num = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_num, NULL);

	VkExtensionProperties *supported_exts = malloc(sizeof(VkExtensionProperties) * supported_exts_num);
	vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_num, supported_exts);

	bool result = true;

	for (uint i = 0; i < _required_glfw_extensions.exts_num; i += 1) {
		bool ext_found = false;

		for (uint j = 0; j < supported_exts_num; j += 1) {
			if (strcmp(supported_exts[j].extensionName, _required_glfw_extensions.extension_names[i]) == 0) {
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

	uint matches = 0;

	for (uint i = 0; i < _required_physical_device_extensions.exts_num; i += 1)
	{
		for (uint j = 0; j < supported_exts_num; j += 1)
		{
			VkExtensionProperties *supported_extension = supported_extensions + j;

			if (strcmp(supported_extension->extensionName, _required_physical_device_extensions.extension_names[i]) == 0)
			{
				matches += 1;
			}
		}
	}

	bool result = (matches == _required_physical_device_extensions.exts_num);

	free(supported_extensions);

	return result;
}
static bool _physical_device_suitable(const VkPhysicalDevice* physical_device) {
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(*physical_device, &device_properties);

	_set_queue_family_properties(physical_device);

	bool extensions_supported = _physical_device_supports_required_extensions(physical_device);

	bool swap_chain_supported = false;

	if (extensions_supported) {
		_set_swap_chain_support(physical_device);

		swap_chain_supported = (
			_physical_device_swap_chain_support.formats_num != 0 &&
			_physical_device_swap_chain_support.present_modes_num != 0
			);
	}

	return (
		device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		_is_complete(&_physical_device_queue_families) &&
		_use_same_family(&_physical_device_queue_families) &&
		extensions_supported &&
		swap_chain_supported
	);
}
static bool _create_shader_module(VkShaderModule *shader_module, const char bin_shader_file_path[]) {
	FILE *file_descriptor = fopen(bin_shader_file_path, "rb");

	if (!file_descriptor)
	{
		return false;
	}

	fseek(file_descriptor, 0L, SEEK_END);
	uint code_size = ftell(file_descriptor);

	rewind(file_descriptor);

	char *shader_code = malloc(code_size);

	fread(shader_code, code_size, 1, file_descriptor);

	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code_size,
		.pCode = (const uint32_t *)shader_code,
	};

	return vkCreateShaderModule(_device, &create_info, NULL, shader_module) == VK_SUCCESS;
}

static void _pick_swap_chain_surface_format(VkSurfaceFormatKHR *picked_surface_format) {
	VkSurfaceFormatKHR *supported_formats = _physical_device_swap_chain_support.formats;
	uint supported_formats_num = _physical_device_swap_chain_support.formats_num;

	if (supported_formats_num == 1 && supported_formats[0].format == VK_FORMAT_UNDEFINED) {
		picked_surface_format->format = VK_FORMAT_B8G8R8A8_UNORM;
		picked_surface_format->colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

		return;
	}

	for (uint i = 0; i < supported_formats_num; i += 1)
	{
		if (supported_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && supported_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			picked_surface_format->format = supported_formats[i].format;
			picked_surface_format->colorSpace = supported_formats[i].colorSpace;

			return;
		}
	}
}
static void _pick_swap_chain_present_mode(VkPresentModeKHR *present_mode) {
	for (uint i = 0; i < _physical_device_swap_chain_support.present_modes_num; i += 1) {
		if (_physical_device_swap_chain_support.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			*present_mode = _physical_device_swap_chain_support.present_modes[i];
		}
	}

	*present_mode = VK_PRESENT_MODE_FIFO_KHR;
}

static bool _init_window()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // enable resizing

	_window = glfwCreateWindow(_WINDOW_WIDTH, _WINDOW_HEIGHT, "zGame", NULL, NULL);

	if (!_window)
	{
		return false;
	}

	glfwSetWindowSizeCallback(_window, _on_window_resize);

	return true;
}
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
		create_info.enabledLayerCount = _required_validation_layers.layers_num;
		create_info.ppEnabledLayerNames = _required_validation_layers.layer_names;
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	create_info.enabledExtensionCount = _instance_extensions_to_enable.exts_num;
	create_info.ppEnabledExtensionNames = _instance_extensions_to_enable.extension_names;

	return VK_SUCCESS == vkCreateInstance(&create_info, NULL, &_instance);
}
static bool _setup_debug_callback()
{
	if (!_enable_validation_layers) return true;

	VkDebugReportCallbackCreateInfoEXT create_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
		.pfnCallback = (PFN_vkDebugReportCallbackEXT)(_debug_callback_function)
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
static bool _pick_physical_device() {
	uint devices_num = 0;

	vkEnumeratePhysicalDevices(_instance, &devices_num, NULL);

	VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * devices_num);
	vkEnumeratePhysicalDevices(_instance, &devices_num, devices);

	bool result = true;

	if (devices_num == 0) {
		printf("Failed to find GPUs with Vulkan support.");

		result = false;
	}
	else
	{
		for (uint i = 0; i < devices_num; i += 1) {
			if (_physical_device_suitable(devices + i)) {
				_physical_device = *(devices + i);

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
	float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo queue_create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = _physical_device_queue_families.graphics_family_idx,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
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

	if (_enable_validation_layers) {
		create_info.enabledLayerCount = _required_validation_layers.layers_num;
		create_info.ppEnabledLayerNames = _required_validation_layers.layer_names;
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	create_info.enabledExtensionCount = _required_physical_device_extensions.exts_num;
	create_info.ppEnabledExtensionNames = _required_physical_device_extensions.extension_names;

	if (
		vkCreateDevice(_physical_device, &create_info, NULL, &_device) != VK_SUCCESS
	) {
		return false;
	}

	vkGetDeviceQueue(_device, _physical_device_queue_families.graphics_family_idx, 0, &_graphics_queue);
	vkGetDeviceQueue(_device, _physical_device_queue_families.present_family_idx, 0, &_present_queue);

	return true;
}
static bool _create_swap_chain()
{
	VkSurfaceFormatKHR picked_surface_format;
	_pick_swap_chain_surface_format(&picked_surface_format);

	VkPresentModeKHR picked_present_mode;
	_pick_swap_chain_present_mode(&picked_present_mode);

	_extent.width = _WINDOW_WIDTH;
	_extent.height = _WINDOW_HEIGHT;

	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = _surface,
		.minImageCount = _physical_device_swap_chain_support.capabilities.minImageCount,
		.imageFormat = picked_surface_format.format,
		.imageColorSpace = picked_surface_format.colorSpace,
		.imageExtent = _extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = _physical_device_swap_chain_support.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = picked_present_mode,
		.clipped = VK_TRUE
	};

	if (vkCreateSwapchainKHR(_device, &createInfo, NULL, &(_swap_chain)) != VK_SUCCESS)
	{
		return false;
	}

	vkGetSwapchainImagesKHR(_device, _swap_chain, &(_swap_chain_images.images_num), NULL);

	_swap_chain_images.images = malloc(sizeof(VkImage) * _swap_chain_images.images_num);

	vkGetSwapchainImagesKHR(_device, _swap_chain, &(_swap_chain_images.images_num), _swap_chain_images.images);

	_color_buffer_format = picked_surface_format.format;

	return true;
}
static bool _create_swap_chain_image_views()
{
	_swap_chain_image_views.image_views_num = _swap_chain_images.images_num;
	_swap_chain_image_views.image_views = malloc(
		sizeof(VkImageView) * _swap_chain_image_views.image_views_num
	);

	for (uint i = 0; i < _swap_chain_images.images_num; i++)
	{
		VkImageViewCreateInfo view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = _swap_chain_images.images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = _color_buffer_format,
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel = 0,
			.subresourceRange.levelCount = 1,
			.subresourceRange.baseArrayLayer = 0,
			.subresourceRange.layerCount = 1,
		};

		if (vkCreateImageView(_device, &view_info, NULL, _swap_chain_image_views.image_views + i) != VK_SUCCESS)
		{
			return false;
		}
	}

	return true;
}
static bool _pick_depth_buffer_format()
{
	VkFormat suitable_formats[3] = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};

	VkFormatProperties props;

	for (uint i = 0; i < 3; i += 1)
	{
		vkGetPhysicalDeviceFormatProperties(_physical_device, suitable_formats[i], &props);

		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			_depth_buffer_format = suitable_formats[i];

			return true;
		}
	}

	return false;
}
static bool _create_render_pass()
{
	VkAttachmentDescription color_attachment = {
		.format = _color_buffer_format,
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
		.format = _depth_buffer_format,
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
static bool _create_descriptor_set_layout()
{
	VkDescriptorSetLayoutBinding ubo_layout_binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	VkDescriptorSetLayoutBinding sampler_layout_binding = {
		.binding = 1,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutBinding bindings[2] = { ubo_layout_binding, sampler_layout_binding };
	VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 2,
		.pBindings = bindings,
	};

	return vkCreateDescriptorSetLayout(_device, &layout_info, NULL, &_descriptor_set_layout) == VK_SUCCESS;
}
static bool _create_graphics_pipeline()
{
	if (!_create_shader_module(&_vertex_shader, "../../src/shaders/vert.spv"))
	{
		return false;
	}
	if (!_create_shader_module(&_fragment_shader, "../../src/shaders/frag.spv"))
	{
		return false;
	}

	return true;
}

bool setup_window_and_gpu()
{
	if (!glfwInit()) // to know which extensions GLFW requires
	{
		printf("Failed to initialize GLFW.");

		return false;
	}

	_set_required_validation_layers();
	_set_required_glfw_extensions();
	_set_required_physical_device_extensions();
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
		printf("Failed to init window.");

		return false;
	}

	if (VK_SUCCESS != glfwCreateWindowSurface(_instance, _window, NULL, &(_surface)))
	{
		printf("Failed to create surface.\n");

		return false;
	}

	if (!_pick_physical_device())
	{
		printf("Failed to find suitable GPU.");

		return false;
	}

	if (!_create_logical_device())
	{
		printf("Failed to create logical device.");

		return false;
	}

	if (_create_swap_chain())
	{
		if (!_create_swap_chain_image_views())
		{
			printf("Failed to create image views.");

			return false;
		}
	}
	else
	{
		printf("Failed to create swap chain.");

		return false;
	}

	if (!_create_render_pass())
	{
		printf("Failed to create render pass.");

		return false;
	}

	if (!_create_descriptor_set_layout())
	{
		printf("Failed to create descriptor set layout.");

		return false;
	}

	if (!_create_graphics_pipeline())
	{
		printf("Failed to create graphics pipeline.");

		return false;
	}

	return true;
}

void destroy_window_and_free_gpu()
{
	vkDestroyShaderModule(_device, _vertex_shader, NULL);
	vkDestroyShaderModule(_device, _fragment_shader, NULL);
	vkDestroyPipeline(_device, _graphics_pipeline, NULL);
	vkDestroyDescriptorSetLayout(_device, _descriptor_set_layout, NULL);
	vkDestroyRenderPass(_device, _render_pass, NULL);

	for (uint i = 0; i < _swap_chain_image_views.image_views_num; i += 1)
	{
		vkDestroyImageView(_device, _swap_chain_image_views.image_views[i], NULL);
	}
	free(_swap_chain_image_views.image_views);
	free(_swap_chain_images.images);

	vkDestroySwapchainKHR(_device, _swap_chain, NULL);
	vkDestroyDevice(_device, NULL);

	free(_physical_device_swap_chain_support.formats);
	free(_physical_device_swap_chain_support.present_modes);
	free((void *)_instance_extensions_to_enable.extension_names);

	vkDestroySurfaceKHR(_instance, _surface, NULL);

	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		_instance, "vkDestroyDebugReportCallbackEXT"
	);

	if (func != NULL) {
		func(_instance, _debug_callback_object, NULL);
	}

	vkDestroyInstance(_instance, NULL);
}