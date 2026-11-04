/* Independent host-side tests: link production memory/cache code against a
 * byte-array DRAM oracle, never duplicate the cache/page algorithms here. */
#include "nemu.h"
#include "memory/cache.h"
#include <stdlib.h>
#include <stdio.h>
#include "cpu/helper.h"
Operands ops_decoded;
char assembly[80];
const char *regsl[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
const char *regsw[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
const char *regsb[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
int mov_r2cr(swaddr_t);
int mov_cr2r(swaddr_t);
CPU_state cpu;
FILE *log_fp;
static uint8_t ram[HW_MEM_SIZE], oracle[HW_MEM_SIZE];
static unsigned reads, writes;
uint32_t dram_read(hwaddr_t addr, size_t len) {
 uint32_t result = 0; assert(addr + len <= sizeof(ram));
 memcpy(&result, ram + addr, len); reads++; return result;
}
void dram_write(hwaddr_t addr, size_t len, uint32_t data) {
 assert(addr + len <= sizeof(ram)); memcpy(ram + addr, &data, len); writes++;
}
static void put(hwaddr_t addr, uint32_t value) { hwaddr_write(addr, 4, value); }
static void cache_test(void) {
 init_cache();
 cache_write(63, 4, 0x12345678);
 assert(writes == 0); /* cold write allocates L2, never immediately DRAM */
 assert(cache_read(63, 4) == 0x12345678);
 unsigned old = reads;
 assert(cache_read(63, 4) == 0x12345678 && reads == old);
 /* 40 blocks mapped to one L2 set guarantee dirty eviction. */
 unsigned i;
 for(i = 0; i < 40; i++) cache_write(0x100 + i * 0x40000, 4, i + 123);
 assert(writes > 0);
 for(i = 0; i < 40; i++) assert(cache_read(0x100 + i * 0x40000, 4) == i + 123);
 /* Seeded randomized reads/writes against an independent byte oracle. */
 init_cache(); memset(ram, 0, sizeof(ram)); memset(oracle, 0, sizeof(oracle));
 srand(90210);
 for(i = 0; i < 120000; i++) {
  uint32_t addr = rand() % (8 * 1024 * 1024 - 4);
  size_t len = 1u << (rand() % 3); uint32_t val = rand();
  if(i & 1) { cache_write(addr, len, val); memcpy(oracle + addr, &val, len); }
  else { uint32_t expected = 0; memcpy(&expected, oracle + addr, len); assert(cache_read(addr, len) == expected); }
 }
 for(i = 0; i < 8 * 1024 * 1024; i += 4) { uint32_t expected; memcpy(&expected, oracle + i, 4); assert(cache_read(i, 4) == expected); }
 puts("PASS L1/L2 cold writes, hits, dirty eviction, split access, randomized byte oracle");
}
static void paging_test(void) {
 init_cache(); init_tlb(); memset(&cpu, 0, sizeof(cpu));
 cpu.cr0.protect_enable = cpu.cr0.paging = 1; cpu.cr3.val = 0x1000;
 put(0x1000 + 4, 0x2007); /* virtual 4 MiB -> page table */
 put(0x2000, 0x3007); put(0x2004, 0x9007);
 lnaddr_write(0x400fff, 4, 0xdeadbeef);
 assert(hwaddr_read(0x3fff, 1) == 0xef);
 assert(hwaddr_read(0x9000, 3) == 0xdeadbe);
 assert(lnaddr_read(0x400fff, 4) == 0xdeadbeef);
 hwaddr_t pa; uint32_t pde, pte;
 assert(!page_query(0x800000, &pa, &pde, &pte));
 /* Cached translation persists until CR3 flush, including same-value CR3. */
 assert(page_translate(0x400000) == 0x3000);
 put(0x2000, 0x5007);
 assert(page_translate(0x400000) == 0x3000);
 /* Execute production MOV r,CR3 helper, instead of manually flushing. */
 cpu.sreg[R_CS].limit = 0xffffffff;
 put(0x10000, 0x0000d822); /* 22 d8: MOV EAX,CR3 (helper excludes 0F) */
 cpu.eax = cpu.cr3.val;
 put(0x1000, 0xc007); put(0xc040, 0x10007);
 assert(mov_r2cr(0x10000) == 2);
 assert(page_translate(0x400000) == 0x5000);
 cpu.cr3.val = 0x6000; put(0x6004, 0x7007); put(0x7000, 0xb007);
 put(0x6000, 0xc007);
 cpu.eax = cpu.cr3.val; assert(mov_r2cr(0x10000) == 2);
 assert(page_translate(0x400000) == 0xb000);
 assert(mov_cr2r(0x10000) == 2 && cpu.eax == 0x6000);
 cpu.cr0.val = 0; assert(page_translate(0x400000) == 0x400000);
 puts("PASS noncontiguous cross-page read/write, absent PDE, TLB and address-space switch");
}
static void segment_test(void) {
 init_cache(); init_tlb(); memset(&cpu, 0, sizeof(cpu));
 cpu.gdtr.base = 0x18000; cpu.gdtr.limit = 31;
 /* nonzero byte-granularity data base=0x20000, limit=0xffff */
 put(0x18008, 0xffff); put(0x1800c, 0x409202);
 cpu.cr0.protect_enable = 1; load_segment(R_DS, 8);
 assert(cpu.sreg[R_DS].base == 0x20000 && cpu.sreg[R_DS].limit == 0xffff);
 swaddr_write(0x123, 4, 0xaabbccdd, R_DS);
 assert(hwaddr_read(0x20123, 4) == 0xaabbccdd);
 assert(swaddr_read(0x123, 4, R_DS) == 0xaabbccdd);
 /* Hidden descriptor remains until reload. */
 put(0x1800c, 0x409203);
 assert(seg_translate(0x123, 4, R_DS) == 0x20123);
 load_segment(R_DS, 8); assert(seg_translate(0x123, 4, R_DS) == 0x30123);
 puts("PASS nonzero segment base, limit decode, hidden descriptor reload");
}
int main(void) { cache_test(); paging_test(); segment_test(); puts("PA3 memory tests PASS"); return 0; }
