#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef NDEBUG
const int enable_validation_layers = 0;
#else
const int enable_validation_layers = 1;
#endif

struct _required_validation_layers
{
	const char **layer_names;
	uint32_t size;
};

struct _required_instance_extensions
{
	char** extension_names;
	uint32_t size;
};

static VkInstance _instance;

int setup_window_and_gpu()
{
	struct _required_validation_layers* rvls;

	_set_required_layers(rvls);

	if (enable_validation_layers)
	{
		if (!_check_validation_layers_support(rvls))
		{
			printf("Validation layers requested, but not available.\n");

			return 0;
		}
	}

	struct _required_instance_extensions* ries;

	_set_required_extensions(ries);

	if (!_check_extensions_support(ries)) {
		printf("Some extensions are not supported.\n");

		return 0;
	}

	return _create_instance(rvls, ries);
}

int destroy_window_and_free_gpu()
{
	free()
	return 1;
}

static void _set_required_layers(struct _required_validation_layers* rvls)
{
	rvls->layer_names[0] = "VK_LAYER_LUNARG_standard_validation";
	rvls->size = 1;
}

static void _set_required_extensions(struct _required_instance_extensions* ries)
{
	uint16_t glfw_required_exts_num;

	const char **glfw_required_exts = glfwGetRequiredInstanceExtensions(
		&glfw_required_exts_num
	);

	ries->extension_names = malloc(VK_MAX_EXTENSION_NAME_SIZE * glfw_required_exts_num + 1);

	for (uint16_t i = 0; i < glfw_required_exts_num; i += 1)
	{
		ries->extension_names[i] = glfw_required_exts[i];
	}

	ries->extension_names[glfw_required_exts_num] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
	ries->size = glfw_required_exts_num + 1;
}

static int _create_instance(
	struct _required_validation_layers* rvls,
	struct _required_instance_extensions* ries
)
{
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

	if (enable_validation_layers)
	{
		createInfo.enabledLayerCount = rvls->size;
		createInfo.ppEnabledLayerNames = rvls->layer_names;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	createInfo.enabledExtensionCount = ries->size;
	createInfo.ppEnabledExtensionNames = ries->extension_names;

	if (VK_SUCCESS != vkCreateInstance(&createInfo, NULL, &_instance))
	{
		printf("Failed to create instance.\n");

		return 0;
	}
}

static int _check_validation_layers_support(struct _required_validation_layers* rls)
{
	uint32_t supported_layer_num;
	vkEnumerateInstanceLayerProperties(&supported_layer_num, NULL);

	VkLayerProperties *supported_layers = malloc(sizeof(VkLayerProperties) * supported_layer_num);

	vkEnumerateInstanceLayerProperties(&supported_layer_num, supported_layers);

	for (uint16_t i = 0; i < rls->size; i += 1) {
		int layer_found = 0;

		for (uint16_t j = 0; j < supported_layer_num; j += 1) {
			if (strcmp(supported_layers[j].layerName, layerName, layerProperties.layerName) == 0) {
				layerFound = 1;
				break;
			}
		}

		return layerFound;
	}

	return 1;
}

static int _check_extensions_support()
{

}