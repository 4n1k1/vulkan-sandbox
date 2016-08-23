#include "HelloTriangleApplication.hpp"

int main() try {
	HelloTriangleApplication app;

	app.run();

	return EXIT_SUCCESS;
}
catch (const std::runtime_error& e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}