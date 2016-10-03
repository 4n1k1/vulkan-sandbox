#include "HelloTriangleApplication.hpp"

#include "Logger.hpp"

Logger *logger;

int main() try {
	logger = new Logger();

	logger->SetLogFileName("zGame.log");

	LOG_DEBUG("========================================");

	HelloTriangleApplication app;

	app.run();

	return EXIT_SUCCESS;
}
catch (const std::runtime_error& e) {
	LOG_DEBUG(e.what());

	return EXIT_FAILURE;
}