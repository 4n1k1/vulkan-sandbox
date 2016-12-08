#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "z_game.h"
#include "system_bridge.h"

#ifdef NDEBUG
const bool _enable_validation_layers = FALSE;
#else
const bool _enable_validation_layers = TRUE;
#endif

static uint _WINDOW_WIDTH = 800;
static uint _WINDOW_HEIGHT = 600;

static RequiredGLFWExtensions _required_glfw_extensions;
static RequiredPhysicalDeviceExtensions _required_physical_device_extensions;
static ExtensionsToEnable _extensions_to_enable;
static RequiredValidationLayers _required_validation_layers;
static VkInstance _instance;
static VkDebugReportCallbackEXT _debug_callback_object;
static GLFWwindow* _window = NULL;
static VkSurfaceKHR _surface;
static VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
static PhysicalDeviceQueueFamilies _physical_device_queue_families;
static PhysicalDeviceSwapChainSupport _physical_device_swap_chain_support;

static inline bool _is_complete(const PhysicalDeviceQueueFamilies *indicies) {
	return indicies->graphics_family_idx >= 0 && indicies->present_family_idx >= 0;
}
static inline bool _use_same_family(const PhysicalDeviceQueueFamilies *indicies) {
	return indicies->graphics_family_idx == indicies->present_family_idx;
}

static void _on_window_resize(GLFWwindow* window, int width, int height)
{

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

	_extensions_to_enable.extension_names = malloc(
		sizeof(char **) * extensions_num
	);

	uint extension_to_enable_idx = 0;

	for (uint i = 0; i < _required_glfw_extensions.exts_num; i += 1)
	{
		_extensions_to_enable.extension_names[extension_to_enable_idx] = _required_glfw_extensions.extension_names[i];
		extension_to_enable_idx += 1;
	}

	_extensions_to_enable.extension_names[extension_to_enable_idx] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

	_extensions_to_enable.exts_num = extensions_num;
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

static bool _instance_supports_required_layers()
{
	uint supported_layers_num;
	vkEnumerateInstanceLayerProperties(&supported_layers_num, NULL);

	VkLayerProperties *supported_layers = malloc(sizeof(VkLayerProperties) * supported_layers_num);

	vkEnumerateInstanceLayerProperties(&supported_layers_num, supported_layers);

	bool result = TRUE;

	for (uint i = 0; i < _required_validation_layers.layers_num; i += 1) {
		bool layer_found = FALSE;

		for (uint j = 0; j < supported_layers_num; j += 1) {
			if (strcmp(supported_layers[j].layerName, _required_validation_layers.layer_names[i]) == 0) {
				layer_found = TRUE;
				break;
			}
		}

		if (!layer_found)
		{
			result = FALSE;
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

	bool result = TRUE;

	for (uint i = 0; i < _required_glfw_extensions.exts_num; i += 1) {
		bool ext_found = FALSE;

		for (uint j = 0; j < supported_exts_num; j += 1) {
			if (strcmp(supported_exts[j].extensionName, _required_glfw_extensions.extension_names[i]) == 0) {
				ext_found = TRUE;
				break;
			}
		}

		if (!ext_found)
		{
			result = FALSE;
			break;
		}

	}

	free(supported_exts);

	return result;
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

	bool swap_chain_supported = FALSE;

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

static bool _init_window()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // enable resizing

	_window = glfwCreateWindow(_WINDOW_WIDTH, _WINDOW_HEIGHT, "zGame", NULL, NULL);

	if (!_window)
	{
		return FALSE;
	}

	glfwSetWindowSizeCallback(_window, _on_window_resize);

	return TRUE;
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

	create_info.enabledExtensionCount = _extensions_to_enable.exts_num;
	create_info.ppEnabledExtensionNames = _extensions_to_enable.extension_names;

	return VK_SUCCESS == vkCreateInstance(&create_info, NULL, &_instance);
}
static bool _setup_debug_callback()
{
	if (!_enable_validation_layers) return TRUE;

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
		return FALSE;
	}

	func(_instance, &create_info, NULL, &_debug_callback_object);

	return TRUE;
}
static bool _pick_physical_device() {
	uint devices_num = 0;

	vkEnumeratePhysicalDevices(_instance, &devices_num, NULL);

	VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * devices_num);
	vkEnumeratePhysicalDevices(_instance, &devices_num, devices);

	bool result = TRUE;

	if (devices_num == 0) {
		printf("Failed to find GPUs with Vulkan support.");

		result = FALSE;
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
			result = FALSE;
		}
	}

	free(devices);

	return result;
};

bool setup_window_and_gpu()
{
	if (!glfwInit()) // to know which extensions GLFW requires
	{
		printf("Failed to initialize GLFW.");

		return FALSE;
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

			return FALSE;
		}
	}

	if (!_instance_supports_required_extensions())
	{
		printf("Some extensions are not supported.\n");

		return FALSE;
	}

	if (!_create_instance())
	{
		printf("Failed to create instance.\n");

		return FALSE;
	}

	if (!_setup_debug_callback())
	{
		printf("Failed to set up debug callback.\n");

		return FALSE;
	}

	if (!_init_window())
	{
		printf("Failed to init window.");

		return FALSE;
	}

	if (VK_SUCCESS != glfwCreateWindowSurface(_instance, _window, NULL, &(_surface)))
	{
		printf("Failed to create surface.\n");

		return FALSE;
	}

	if (!_pick_physical_device())
	{
		printf("Failed to find suitable GPU.");

		return FALSE;
	}

	return TRUE;
}

static void _clear_debug_callback()
{
	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		_instance, "vkDestroyDebugReportCallbackEXT"
	);

	if (func != NULL) {
		func(_instance, _debug_callback_object, NULL);
	}
}

void destroy_window_and_free_gpu()
{
	free(_physical_device_swap_chain_support.formats);
	free(_physical_device_swap_chain_support.present_modes);
	free((void *)_extensions_to_enable.extension_names);

	vkDestroySurfaceKHR(_instance, _surface, NULL);

	_clear_debug_callback();

	vkDestroyInstance(_instance, NULL);
}