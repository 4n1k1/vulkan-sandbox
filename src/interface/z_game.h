#ifndef Z_GAME
#define Z_GAME

/*
#include "Logger.hpp"
*/

#define uint unsigned int

#ifndef WIN32
#define __stdcall
#endif

/*
#define STRINGIFY(s) PRESTRINGIFY(s)
#define PRESTRINGIFY(s) #s

extern Logger *logger;

#ifndef NDEBUG
#define LOG_DEBUG(message) \
	logger->log_debug(__FILE__ ":" STRINGIFY(__LINE__), message)
#else
#define LOG_DEBUG(message)
#endif
*/

#endif  // Z_GAME
