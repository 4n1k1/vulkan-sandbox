#include <vulkan/vulkan.h>
#include <stdio.h>

#ifdef NDEBUG
const int enableValidationLayers = 0;
#else
const int enableValidationLayers = 1;
#endif

struct _required_validation_layers
{
	const char** data;
	uint32_t size;
};

struct _required_instance_extensions
{
	const char** data;
	uint32_t size;
};

static VkInstance _instance;

int setup_window_and_gpu()
{
	struct _required_validation_layers* rvls;

	_set_required_layers(rvls);

	if (enableValidationLayers)
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
	return 1;
}

static void _set_required_layers(struct _required_validation_layers* rvls)
{
	rvls->data[0] = "VK_LAYER_LUNARG_standard_validation";
	rvls->size = 1;
}

static void _set_required_extensions(struct _required_instance_extensions* ries)
{
	ries->data = glfwGetRequiredInstanceExtensions(&(ries->size));

	ries->data[ries->size] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
	ries->size += 1;
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

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = rvls->size;
		createInfo.ppEnabledLayerNames = rvls->data;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	createInfo.enabledExtensionCount = ries->size;
	createInfo.ppEnabledExtensionNames = ries->data;

	if (VK_SUCCESS != vkCreateInstance(&createInfo, NULL, &_instance))
	{
		printf("Failed to create instance.\n");

		return 0;
	}
}

static int _check_validation_layers_support(struct _required_validation_layers* rls)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);

	VkLayerProperties layerProperties*
	std::vector<VkLayerProperties> supportedLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());

	for (const char* layerName : requiredLayers) {
		int layerFound = 0;

		for (const auto& layerProperties : supportedLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
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