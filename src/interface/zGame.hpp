#pragma once

#include "Logger.hpp"

#define STRINGIFY(s) PRESTRINGIFY(s)
#define PRESTRINGIFY(s) #s

extern Logger *logger;

#ifndef NDEBUG
#define LOG_DEBUG(message) \
	logger->log_debug(__FILE__ ":" STRINGIFY(__LINE__), message)
#else
#define LOG_DEBUG(message)
#endif
