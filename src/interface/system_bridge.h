#ifndef ZGAME_SYSTEM_BRIDGE
#define ZGAME_SYSTEM_BRIDGE

#include <stdbool.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "math3d.h"

typedef struct RequiredValidationLayers
{
	char *names[1];
	uint32_t count;

} RequiredValidationLayers;

typedef struct RequiredGLFWExtensions
{
	const char **names;
	uint32_t count;

} RequiredGLFWExtensions;

typedef struct ExtensionsToEnable
{
	char const **names;
	uint32_t count;

} ExtensionsToEnable;

typedef struct OperationQueueFamilies
{
	int graphics_family_idx;
	int compute_family_idx;
	int present_family_idx;

	bool use_same_family;

} OperationQueueFamilies;

typedef struct SurfaceFormats
{
	VkSurfaceFormatKHR *data;
	uint32_t count;

} SurfaceFormats;

typedef struct PresentModes
{
	VkPresentModeKHR *data;
	uint32_t count;

} PresentModes;

typedef struct RequiredPhysicalDeviceExtensions
{
	char *names[1];
	uint32_t count;

} RequiredPhysicalDeviceExtensions;

typedef struct SwapChainImages
{
	VkImage *data;
	uint32_t count;

} SwapChainImages;

typedef struct SwapChainImageViews
{
	VkImageView *data;
	uint32_t count;

} SwapChainImageViews;

typedef struct SwapChainFramebuffers
{
	VkFramebuffer *data;
	uint32_t count;

} SwapChainFramebuffers;

typedef struct CommandBuffers
{
	VkCommandBuffer *data;
	uint32_t count;

} CommandBuffers;

typedef struct Color
{
	float red;
	float green;
	float blue;
	float alpha;

} Color;

typedef struct Vertex
{
	float x;
	float y;
	float z;

} Vertex;

typedef struct Vertices
{
	Vector_3 *data;
	uint32_t count;

} Vertices;

typedef struct Indices
{
	uint32_t *data;

	uint32_t count;

} Indices;

typedef struct Particle
{
	Vector_3 position;

	uint32_t color_idx;

} Particle;

typedef struct MVP
{
	Matrix_4x4 model;
	Matrix_4x4 view;
	Matrix_4x4 projection;

} MVP;

typedef struct UniformData
{
	MVP mvp;
	Color color;

} UniformData;

bool setup_window_and_gpu();
void destroy_window_and_free_gpu();

void create_particles();
void destroy_particles();

void render();

#endif
