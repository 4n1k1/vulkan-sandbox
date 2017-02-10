#include <stdbool.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct RequiredValidationLayers
{
	char *names[1];
	size_t count;

} RequiredValidationLayers;

typedef struct RequiredGLFWExtensions
{
	const char **names;
	size_t count;

} RequiredGLFWExtensions;

typedef struct ExtensionsToEnable
{
	char const **names;
	size_t count;

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
	size_t count;

} SurfaceFormats;

typedef struct PresentModes
{
	VkPresentModeKHR *data;
	size_t count;

} PresentModes;

typedef struct RequiredPhysicalDeviceExtensions
{
	char *names[1];
	size_t count;

} RequiredPhysicalDeviceExtensions;

typedef struct SwapChainImages
{
	VkImage *data;
	size_t count;

} SwapChainImages;

typedef struct SwapChainImageViews
{
	VkImageView *data;
	size_t count;

} SwapChainImageViews;

typedef struct SwapChainFramebuffers
{
	VkFramebuffer *data;
	size_t count;

} SwapChainFramebuffers;

typedef struct CommandBuffers
{
	VkCommandBuffer *data;
	size_t count;

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

	Color color;

} Vertex;

typedef struct Vertices
{
	Vertex *data;
	size_t count;

} Vertices;

typedef struct Indices
{
	size_t *data;

	size_t count;

} Indices;

typedef struct Position
{
	float x;
	float y;
	float z;

} Position;

typedef struct Particle
{
	Position position;

	size_t color_idx;

} Particle;

typedef struct Matrix
{
	float data[4][4];

} Matrix;

typedef struct MVP
{
	Matrix model;
	Matrix view;
	Matrix projection;

} MVP;

typedef struct UniformData
{
	MVP mvp;
	Color colors[1];

} UniformData;

bool setup_window_and_gpu();
void destroy_window_and_free_gpu();

void create_particles();
void destroy_particles();

void render();