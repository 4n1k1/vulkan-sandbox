#include "z_game.h"

#include <stdbool.h>
#include <vulkan/vulkan.h>

typedef struct RequiredValidationLayers
{
	char *layer_names[1];
	uint layers_num;

} RequiredValidationLayers;

typedef struct RequiredGLFWExtensions
{
	const char **extension_names;
	uint exts_num;

} RequiredGLFWExtensions;

typedef struct ExtensionsToEnable
{
	char const **extension_names;
	uint exts_num;

} ExtensionsToEnable;

typedef struct PhysicalDeviceQueueFamilies
{
	int graphics_family_idx;
	int present_family_idx;

} PhysicalDeviceQueueFamilies;

typedef struct PhysicalDeviceSwapChainSupport
{
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *formats;
	uint formats_num;

	VkPresentModeKHR *present_modes;
	uint present_modes_num;

} PhysicalDeviceSwapChainSupport;

typedef struct RequiredPhysicalDeviceExtensions
{
	char *extension_names[1];
	uint exts_num;

} RequiredPhysicalDeviceExtensions;

typedef struct SwapChainImages
{
	VkImage *images;
	uint images_num;

} SwapChainImages;

bool setup_window_and_gpu();
void destroy_window_and_free_gpu();
