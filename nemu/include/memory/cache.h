#ifndef __CACHE_H__
#define __CACHE_H__

#include "common.h"

void init_cache(void);
void cache_stats(uint64_t *, uint64_t *, uint64_t *, uint64_t *);
uint32_t cache_read(hwaddr_t addr, size_t len);
void cache_write(hwaddr_t addr, size_t len, uint32_t data);

#endif
