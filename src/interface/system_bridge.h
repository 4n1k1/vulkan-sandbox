#ifndef ZGAME_SYSTEM_BRIDGE
#define ZGAME_SYSTEM_BRIDGE

#include <stdbool.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "math3d.h"

#define VK_FLAGS_NONE 0
#define CHECK_VK_RESULT(action) \
{                               \
	if (VK_SUCCESS != (action)) \
	{                           \
		return false;           \
	}                           \
}

#define CHECK_VK_RESULT_M(action, message) \
{                                          \
	if (VK_SUCCESS != (action))            \
	{                                      \
		printf("%s\n", message);           \
		return false;                      \
	}                                      \
}

#define CHECK_RESULT(action) \
{                            \
	if (!(action))           \
	{                        \
		return false;        \
	}                        \
}

#define CHECK_RESULT_M(action, message) \
{                                       \
	if (!(action))                      \
	{                                   \
		printf("%s\n", message);        \
		return false;                   \
	}                                   \
}

#define CHECK_GLFW_RESULT_M(action, message) \
{                                            \
	if (GLFW_TRUE != (action))               \
	{                                        \
		printf("%s\n", message);             \
		return false;                        \
	}                                        \
}

typedef struct Extensions
{
	const char **names;
	uint32_t count;

} Extensions;

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
	Vector4 position;
	Color color;

} Vertex;

typedef struct Vertices
{
	Vertex *data;
	uint32_t count;

} Vertices;

typedef struct Indices
{
	uint32_t *data;

	uint32_t count;

} Indices;

typedef struct Particle
{
	Vector4 position;
	Color color;

} Particle;

typedef struct UniformData
{
	Matrix4x4 model;
	Matrix4x4 view;
	Matrix4x4 projection;

	uint32_t particle_count;
	float particle_radius;

} UniformData;

bool setup_window_and_gpu();
void destroy_window_and_free_gpu();

void create_particles();
void destroy_particles();

void render();

#endif
