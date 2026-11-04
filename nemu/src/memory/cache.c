#include "common.h"
#include "memory/cache.h"
#include <stdlib.h>

uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);
#define LINE 64
#define L1_SETS 128
#define L1_WAYS 8
#define L2_SETS 4096
#define L2_WAYS 16
typedef struct { bool valid, dirty; uint32_t tag; uint8_t data[LINE]; } CacheLine;
static CacheLine l1[L1_SETS][L1_WAYS], l2[L2_SETS][L2_WAYS];
static uint64_t l1_hits, l1_misses, l2_hits, l2_misses;

static CacheLine *lookup(CacheLine *set, int ways, uint32_t tag) {
 int i;
 for(i = 0; i < ways; i++) if(set[i].valid && set[i].tag == tag) return set + i;
 return NULL;
}
static CacheLine *victim(CacheLine *set, int ways) {
 int i;
 for(i = 0; i < ways; i++) if(!set[i].valid) return set + i;
 return set + rand() % ways;
}
static CacheLine *get_l2(hwaddr_t addr) {
 uint32_t index = (addr >> 6) & (L2_SETS - 1), tag = addr >> 18;
 CacheLine *line = lookup(l2[index], L2_WAYS, tag);
 if(line) { l2_hits++; return line; }
 l2_misses++;
 line = victim(l2[index], L2_WAYS);
 int i;
 if(line->valid && line->dirty) {
  hwaddr_t old = (line->tag << 18) | (index << 6);
  for(i = 0; i < LINE; i += 4) { uint32_t word; memcpy(&word, line->data + i, 4); dram_write(old + i, 4, word); }
 }
 hwaddr_t block = addr & ~63u;
 for(i = 0; i < LINE; i += 4) { uint32_t word = dram_read(block + i, 4); memcpy(line->data + i, &word, 4); }
 line->valid = true; line->dirty = false; line->tag = tag;
 return line;
}
void init_cache(void) {
 memset(l1, 0, sizeof(l1)); memset(l2, 0, sizeof(l2));
 l1_hits = l1_misses = l2_hits = l2_misses = 0;
}
void cache_stats(uint64_t *h1, uint64_t *m1, uint64_t *h2, uint64_t *m2) {
 *h1 = l1_hits; *m1 = l1_misses; *h2 = l2_hits; *m2 = l2_misses;
}
uint32_t cache_read(hwaddr_t addr, size_t len) {
 uint32_t result = 0; size_t copied = 0;
 while(copied < len) {
  hwaddr_t cur = addr + copied; uint32_t offset = cur & 63;
  size_t chunk = LINE - offset; if(chunk > len - copied) chunk = len - copied;
  CacheLine *line = lookup(l1[(cur >> 6) & 127], L1_WAYS, cur >> 13);
  if(!line) {
   l1_misses++;
   CacheLine *lower = get_l2(cur);
   line = victim(l1[(cur >> 6) & 127], L1_WAYS);
   memcpy(line->data, lower->data, LINE); line->valid = true; line->tag = cur >> 13;
  } else l1_hits++;
  memcpy((uint8_t *)&result + copied, line->data + offset, chunk); copied += chunk;
 }
 return result;
}
void cache_write(hwaddr_t addr, size_t len, uint32_t data) {
 size_t copied = 0;
 while(copied < len) {
  hwaddr_t cur = addr + copied; uint32_t offset = cur & 63;
  size_t chunk = LINE - offset; if(chunk > len - copied) chunk = len - copied;
  CacheLine *line = lookup(l1[(cur >> 6) & 127], L1_WAYS, cur >> 13);
  if(line) { l1_hits++; memcpy(line->data + offset, (uint8_t *)&data + copied, chunk); }
  else l1_misses++;
  /* L1 write-through targets L2; only dirty L2 eviction reaches DRAM. */
  line = get_l2(cur); line->dirty = true;
  memcpy(line->data + offset, (uint8_t *)&data + copied, chunk); copied += chunk;
 }
}
