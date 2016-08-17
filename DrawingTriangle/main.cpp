#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> /* includes <vulkan/vulkan.h> */

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <cstring>

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
	} else {
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

template <typename T>
class VDeleter {

	T _object{ VK_NULL_HANDLE };
	std::function<void(T)> _deleter;

	void _cleanup() {
		if (_object != VK_NULL_HANDLE) {
			_deleter(_object);
		}
		_object = VK_NULL_HANDLE;
	}

public:
	VDeleter() : VDeleter([](T _) {}) {}

	VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef) {
		this->_deleter = [=](T obj) { deletef(obj, nullptr); };
	}

	VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
		this->_deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
	}

	VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) {
		this->_deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
	}

	~VDeleter() {
		this->_cleanup();
	}

	T* operator &() {
		this->_cleanup();
		return &(this->_object);
	}

	operator T() const {
		return this->_object;
	}
};

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> requiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {

	GLFWwindow* _window;
	VDeleter<VkInstance> _instance{ vkDestroyInstance };
	VDeleter<VkDebugReportCallbackEXT> _callback{ _instance, DestroyDebugReportCallbackEXT };
	std::vector<const char*> _validExtensions;

	void _initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // disable resizing

		this->_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void _initVulkan() {
		this->_createInstance();
		this->_setupDebugCallback();
	}

	void _setupDebugCallback() {
		if (!enableValidationLayers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)(this->_debugCallback);

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

		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;
		createInfo.enabledLayerCount = 0;

		VkResult result = vkCreateInstance(&createInfo, nullptr, &(this->_instance));

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}

		if (enableValidationLayers && !this->_checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		if (!this->_checkExtensionSupport(glfwExtensionCount, glfwExtensions)) {
			throw std::runtime_error("glfw extensions are not supported!");
		}

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = requiredLayers.size();
			createInfo.ppEnabledLayerNames = requiredLayers.data();

			this->_validExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		} else {
			createInfo.enabledLayerCount = 0;
		}

		createInfo.enabledExtensionCount = this->_validExtensions.size();
		createInfo.ppEnabledExtensionNames = this->_validExtensions.data();
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

		for (int i = 0; i < glfwExtensionCount; i++) {
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

	static VkBool32 _debugCallback(
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

public:

	void run() {
		this->_initWindow();
		this->_initVulkan();
		this->_mainLoop();
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	} catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}