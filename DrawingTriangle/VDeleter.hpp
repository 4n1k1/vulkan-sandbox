#include <vulkan/vulkan.h>
#include <functional>

#ifndef NDEBUG
#include <iostream>
#endif

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
	VDeleter() {
#ifndef NDEBUG
		std::cout << "Object of class: " << typeid(T).name() << " created." << std::endl;
#endif
	}

	VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef): VDeleter() {
		this->_deleter = [=](T obj) { deletef(obj, nullptr); };
	}

	VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef): VDeleter() {
		this->_deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
	}

	VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef): VDeleter() {
		this->_deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
	}

	~VDeleter() {
		this->_cleanup();

#ifndef NDEBUG
		std::cout << "Object of class: " << typeid(T).name() << " destroyed." << std::endl;
#endif
	}

	T* replace() {
		this->_cleanup();
		return &(this->_object);
	}

	T* operator &() {
		this->_cleanup();
		return &(this->_object);
	}

	operator T() const {
		return this->_object;
	}
};