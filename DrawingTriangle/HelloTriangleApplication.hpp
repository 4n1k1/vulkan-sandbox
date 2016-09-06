#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> /* includes <vulkan/vulkan.h> */

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <set>
#include <algorithm>

#include "VDeleter.hpp"

#ifndef WIN32
#define __stdcall
#endif

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

const std::vector<const char*> requiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> requiredDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0;
	}

	bool useSameFamily() {
		return graphicsFamily == presentFamily;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication {

	GLFWwindow* _window;
	VDeleter<VkInstance> _instance{ vkDestroyInstance };
	VDeleter<VkDebugReportCallbackEXT> _callback{ _instance, DestroyDebugReportCallbackEXT };
	VDeleter<VkSurfaceKHR> _surface{ _instance, vkDestroySurfaceKHR };
	std::vector<const char*> _validExtensions;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VDeleter<VkDevice> _device{ vkDestroyDevice };
	QueueFamilyIndices _queueFamilyIndices;
	VkQueue _graphicsQueue;
	VkQueue _presentQueue;
	VDeleter<VkSwapchainKHR> _swapChain{ _device, vkDestroySwapchainKHR };
	std::vector<VkImage> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;

	void _initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // disable resizing

		this->_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void _initVulkan() {
		this->_createInstance();
		this->_setupDebugCallback();
		this->_createSurface();
		this->_pickPhysicalDevice();
		this->_createLogicalDevice();
		this->_createSwapChain();
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

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = (uint32_t)requiredLayers.size();
			createInfo.ppEnabledLayerNames = requiredLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		createInfo.enabledExtensionCount = requiredDeviceExtensions.size();
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
		}
		else {
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
		if (!enableValidationLayers) return;

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
		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

	void _mainLoop() {
		while (!glfwWindowShouldClose(this->_window)) {
			glfwPollEvents();
		}
	}

	void _createInstance() {
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		if (enableValidationLayers && !this->_checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		if (!this->_checkExtensionSupport(glfwExtensionCount, glfwExtensions)) {
			throw std::runtime_error("glfw extensions are not supported!");
		}

		this->_validExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		if (enableValidationLayers) {
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

	bool _checkExtensionSupport(unsigned int glfwExtensionCount, const char** glfwExtensions) {
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.data());

		std::cout << "Supported extensions:" << std::endl;

		for (const auto& extension : supportedExtensions) {
			std::cout << extension.extensionName << std::endl;
		}

		std::cout << "Requred extensions:" << std::endl;

		for (unsigned int i = 0; i < glfwExtensionCount; i++) {
			bool extensionFound = false;

			std::cout << glfwExtensions[i] << std::endl;

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

public:

	void run() {
		this->_initWindow();
		this->_initVulkan();
		this->_mainLoop();
	}
};