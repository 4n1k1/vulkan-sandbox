#include <stdio.h>
#include <stdlib.h>

#include "system_bridge.h"

int main(int argc, char** argv) {
	if (!setup_window_and_gpu())
	{
		return EXIT_FAILURE;
	}

	destroy_window_and_free_gpu();

	return EXIT_SUCCESS;
}
