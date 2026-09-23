#include "common.h"
#include "memory/cache.h"

#include <stdlib.h>

uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);

#define CACHE_BLOCK_SIZE 64
#define CACHE_SIZE (64 * 1024)
#define CACHE_WAYS 8
#define CACHE_SET_COUNT (CACHE_SIZE / CACHE_BLOCK_SIZE / CACHE_WAYS)

#define CACHE_OFFSET_BITS 6
#define CACHE_SET_BITS 7
#define CACHE_SET_MASK (CACHE_SET_COUNT - 1)

typedef struct {
	bool valid;
	uint32_t tag;
	uint8_t data[CACHE_BLOCK_SIZE];
} CacheLine;

static CacheLine cache[CACHE_SET_COUNT][CACHE_WAYS];

static uint32_t cache_set(hwaddr_t addr) {
	return (addr >> CACHE_OFFSET_BITS) & CACHE_SET_MASK;
}

static uint32_t cache_tag(hwaddr_t addr) {
	return addr >> (CACHE_OFFSET_BITS + CACHE_SET_BITS);
}

static CacheLine *find_line(hwaddr_t addr) {
	uint32_t set = cache_set(addr);
	uint32_t tag = cache_tag(addr);
	int way;

	for(way = 0; way < CACHE_WAYS; way ++) {
		if(cache[set][way].valid && cache[set][way].tag == tag) {
			return &cache[set][way];
		}
	}
	return NULL;
}

static CacheLine *load_line(hwaddr_t addr) {
	uint32_t set = cache_set(addr);
	uint32_t tag = cache_tag(addr);
	hwaddr_t block_addr = addr & ~(CACHE_BLOCK_SIZE - 1);
	int way;
	CacheLine *line = NULL;

	for(way = 0; way < CACHE_WAYS; way ++) {
		if(!cache[set][way].valid) {
			line = &cache[set][way];
			break;
		}
	}
	if(line == NULL) {
		line = &cache[set][rand() % CACHE_WAYS];
	}

	for(way = 0; way < CACHE_BLOCK_SIZE; way += 4) {
		uint32_t data = dram_read(block_addr + way, 4);
		memcpy(line->data + way, &data, sizeof(data));
	}
	line->tag = tag;
	line->valid = true;
	return line;
}

void init_cache(void) {
	int set;
	int way;

	for(set = 0; set < CACHE_SET_COUNT; set ++) {
		for(way = 0; way < CACHE_WAYS; way ++) {
			cache[set][way].valid = false;
		}
	}
}

uint32_t cache_read(hwaddr_t addr, size_t len) {
	uint32_t result = 0;
	size_t copied = 0;

	while(copied < len) {
		hwaddr_t current = addr + copied;
		uint32_t offset = current & (CACHE_BLOCK_SIZE - 1);
		size_t chunk = CACHE_BLOCK_SIZE - offset;
		CacheLine *line = find_line(current);

		if(chunk > len - copied) chunk = len - copied;
		if(line == NULL) line = load_line(current);
		memcpy((uint8_t *)&result + copied, line->data + offset, chunk);
		copied += chunk;
	}
	return result;
}

void cache_write(hwaddr_t addr, size_t len, uint32_t data) {
	size_t copied = 0;

	while(copied < len) {
		hwaddr_t current = addr + copied;
		uint32_t offset = current & (CACHE_BLOCK_SIZE - 1);
		size_t chunk = CACHE_BLOCK_SIZE - offset;
		CacheLine *line = find_line(current);

		if(chunk > len - copied) chunk = len - copied;
		if(line != NULL) {
			memcpy(line->data + offset, (uint8_t *)&data + copied, chunk);
		}
		copied += chunk;
	}

	/* Write through; a miss does not allocate a cache line. */
	dram_write(addr, len, data);
}
