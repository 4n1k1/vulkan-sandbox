#include <malloc.h>
#include <stdint.h>

#define addr_unit uint32_t

#define MEMORY_SIZE 536870912
#define FREE_CHUNKS_LIMIT 32
#define OCCUPIED_CHUNKS_LIMIT 1024

struct memory_pool
{
	addr_unit *memory;

	struct occupied_memory_chunk *o_chunks;
	struct free_memory_chunk *f_chunks;

	uint32_t free_chunks_num;
	uint32_t occupied_chunks_num;
	uint32_t occupied_memory_offset;
};

struct occupied_memory_chunk
{
	addr_unit *addr;
	uint32_t size;
};

struct free_memory_chunk
{
	addr_unit *addr;
	uint32_t size;
};

static struct memory_pool _mempool;

void setup_memory_pool()
{
	_mempool.memory = (addr_unit *)malloc(MEMORY_SIZE);

	_mempool.f_chunks = (struct free_memory_chunk *)malloc(
		sizeof(struct free_memory_chunk) * FREE_CHUNKS_LIMIT
	);

	_mempool.o_chunks = (struct occupied_memory_chunk *)malloc(
		sizeof(struct occupied_memory_chunk) * OCCUPIED_CHUNKS_LIMIT
	);

	_mempool.occupied_memory_offset = 0;
	_mempool.free_chunks_num = 0;
	_mempool.occupied_chunks_num = 0;
}

void clear_memory_pool()
{
	free(_mempool.o_chunks);
	free(_mempool.f_chunks);
	free(_mempool.memory);
}

void* allocate_memory(unsigned int bytes)
{
	unsigned int addr_units_num = bytes / sizeof(addr_unit);

	if (bytes % sizeof(addr_unit) > 0)
	{
		addr_units_num += 1;
	}

	if (_mempool.free_chunks_num == 0)
	{
		return (void*)(_mempool.memory + _mempool.occupied_memory_offset);
	}
	else
	{
		for (uint32_t i = 0; i < _mempool.free_chunks_num; i += 1)
		{

		}
	}
}

void free_memory(void *addr) {

}