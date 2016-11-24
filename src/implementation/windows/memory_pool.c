#include <malloc.h>
#include <stdint.h>

#define WORD_SIZE 4
#define MEMORY_POOL_SIZE 536870912

struct memory_pool
{
	void *memory;

	uint32_t occupied_memory_offset;
	uint32_t *allocated_sizes;
};

static struct memory_pool _mempool;

void setup_memory_pool()
{
	_mempool.memory = malloc(MEMORY_POOL_SIZE);
	_mempool.occupied_memory_offset = 0;
}

void clear_memory_pool()
{
	free(_mempool.memory);
}

void* allocate_memory(unsigned int bytes)
{
	if (bytes < WORD_SIZE) bytes = WORD_SIZE;

	

}

void free_memory()