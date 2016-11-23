#include <stdio.h>
#include <stdlib.h>

#include "system_bridge.h"

int main(int argc, char** argv) {
	if (!setup_window_and_gpu())
	{
		printf("Error while window/gpu setup.\n");

		return EXIT_FAILURE;
	}

	if (!destroy_window_and_free_gpu())
	{
		printf("Error releasing resources.\n");

		return EXIT_FAILURE;

	}

	return EXIT_SUCCESS;
}
