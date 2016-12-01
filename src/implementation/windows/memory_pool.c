#include <malloc.h>
#include <stdint.h>

#define addr_unit uint32_t

#define MEMORY_SIZE 536870912
#define FREE_CHUNKS_LIMIT 32
#define OCCUPIED_CHUNKS_LIMIT 1024

struct memory_pool
{
	addr_unit *memory;

	/*
		Occupied memory chunks.
	*/
	struct occupied_memory_chunk *o_chunks;

	/*
		Free memory chunks insied occupied memory.
		These are basically holes in occupied memory which
		must be used when possible and are to be
		eliminated (memory defragmentation) when possible.
	*/
	struct free_memory_chunk *f_chunks;

	uint32_t f_chunks_num;
	uint32_t o_chunks_num;

	uint32_t occupied_memory_offset;
};

struct occupied_memory_chunk
{
	addr_unit *addr;
	uint32_t size;
	struct occupied_memory_chunk *next;
};

struct free_memory_chunk
{
	addr_unit *addr;
	uint32_t size;
	struct free_memory_chunk *next;
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

	_mempool.f_chunks_num = 0;
	_mempool.o_chunks_num = 0;

	_mempool.f_chunks = NULL;
	_mempool.o_chunks = NULL;
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

	if (_mempool.f_chunks_num != 0)
	{
		for (uint32_t i = 0; i < _mempool.f_chunks_num; i += 1)
		{
			if (_mempool.f_chunks[i].size < addr_units_num)
			{
				addr_unit *target_addr = _mempool.o_chunks[_mempool.o_chunks_num].addr;

				_mempool.o_chunks[_mempool.o_chunks_num].addr = _mempool.f_chunks[i].addr;
				_mempool.o_chunks[_mempool.o_chunks_num].size = _mempool.f_chunks[i].size;

				_mempool.o_chunks_num += 1;

				_mempool.f_chunks[i].addr += addr_units_num;
				_mempool.f_chunks[i].size -= addr_units_num;

				return (void *)(target_addr);
			}

			if (_mempool.f_chunks[i].size == addr_units_num)
			{
				addr_unit *target_addr = _mempool.o_chunks[_mempool.o_chunks_num].addr;

				_mempool.o_chunks[_mempool.o_chunks_num].addr = _mempool.f_chunks[i].addr;
				_mempool.o_chunks[_mempool.o_chunks_num].size = _mempool.f_chunks[i].size;

				_mempool.o_chunks_num += 1;
				_mempool.f_chunks_num -= 1;

				return (void *)(target_addr);
			}
		}
	}

	addr_unit *target_addr = _mempool.o_chunks[_mempool.o_chunks_num].addr;

	_mempool.o_chunks[_mempool.o_chunks_num].addr = _mempool.memory + _mempool.occupied_memory_offset;
	_mempool.o_chunks[_mempool.o_chunks_num].size = addr_units_num;

	_mempool.o_chunks_num += 1;
	_mempool.occupied_memory_offset += addr_units_num;

	return (void*)(target_addr);
}

void free_memory(void *addr) {
	addr_unit *target_addr = (addr_unit *)addr;

	for (unsigned int i = 0; i < _mempool.o_chunks_num; i++)
	{
		if (_mempool.o_chunks[i].addr == target_addr)
		{
			struct occupied_memory_chunk *target_o_chunk = _mempool.o_chunks + i;

			/*
				If we release chunk of memory which is the latest memory chunk
				in _mempool.memory array.
			*/
			if (target_o_chunk->addr + target_o_chunk->size == _mempool.memory + _mempool.occupied_memory_offset)
			{
				_mempool.occupied_memory_offset -= target_o_chunk->size;
				_mempool.o_chunks_num -= 1;

				for (unsigned int j = 0; j < _mempool.f_chunks_num; j++)
				{
					/*
						If free chunk was before target_o_chunk.
					*/
					if (_mempool.f_chunks[j].addr + _mempool.f_chunks[j].size == _mempool.memory + _mempool.occupied_memory_offset)
					{
						_mempool.f_chunks_num -= 1;
					}
				}
			}
			else
			{

			}
		}
	}
}