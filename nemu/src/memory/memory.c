#include "nemu.h"
#include "memory/cache.h"
#include <stdlib.h>

typedef struct { bool valid; uint32_t vpn, frame; } TLBEntry;
static TLBEntry tlb[64];

void init_tlb(void) { memset(tlb, 0, sizeof(tlb)); }

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
 Assert(len >= 1 && len <= 4 && addr < HW_MEM_SIZE && len <= HW_MEM_SIZE - addr, "invalid physical read %x/%zu", addr, len);
 return cache_read(addr, len);
}
void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
 Assert(len >= 1 && len <= 4 && addr < HW_MEM_SIZE && len <= HW_MEM_SIZE - addr, "invalid physical write %x/%zu", addr, len);
 cache_write(addr, len, data);
}

/* Nonfatal page-table inspection also serves the monitor. No TLB changes. */
bool page_query(lnaddr_t addr, hwaddr_t *pa, uint32_t *pde, uint32_t *pte) {
 *pde = *pte = 0;
 if(!(cpu.cr0.protect_enable && cpu.cr0.paging)) { *pa = addr; return true; }
 uint32_t dir = (cpu.cr3.val & ~0xfffu) + (addr >> 22) * 4;
 if(dir > HW_MEM_SIZE - 4) return false;
 *pde = hwaddr_read(dir, 4);
 if(!(*pde & 1) || (*pde & 0x80)) return false;
 uint32_t tab = (*pde & ~0xfffu) + ((addr >> 12) & 1023) * 4;
 if(tab > HW_MEM_SIZE - 4) return false;
 *pte = hwaddr_read(tab, 4);
 if(!(*pte & 1)) return false;
 *pa = (*pte & ~0xfffu) | (addr & 4095);
 return *pa < HW_MEM_SIZE;
}

hwaddr_t page_translate(lnaddr_t addr) {
 if(!(cpu.cr0.protect_enable && cpu.cr0.paging)) return addr;
 uint32_t vpn = addr >> 12;
 int i, slot = -1;
 for(i = 0; i < 64; i++) {
  if(tlb[i].valid && tlb[i].vpn == vpn) return tlb[i].frame | (addr & 4095);
  if(!tlb[i].valid) slot = i;
 }
 hwaddr_t pa; uint32_t pde, pte;
 bool ok = page_query(addr, &pa, &pde, &pte);
 Assert(ok, "unmapped linear address %08x CR3=%08x PDE=%08x PTE=%08x", addr, cpu.cr3.val, pde, pte);
 if(slot < 0) slot = rand() % 64;
 tlb[slot].valid = true; tlb[slot].vpn = vpn; tlb[slot].frame = pa & ~0xfffu;
 return pa;
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
 assert(len >= 1 && len <= 4);
 size_t first = 4096 - (addr & 4095);
 if(first >= len) return hwaddr_read(page_translate(addr), len);
 uint32_t low = hwaddr_read(page_translate(addr), first);
 return low | (hwaddr_read(page_translate(addr + first), len - first) << (first * 8));
}
void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
 assert(len >= 1 && len <= 4);
 size_t first = 4096 - (addr & 4095);
 if(first >= len) { hwaddr_write(page_translate(addr), len, data); return; }
 hwaddr_write(page_translate(addr), first, data);
 hwaddr_write(page_translate(addr + first), len - first, data >> (first * 8));
}

void load_segment(uint8_t reg, uint16_t selector) {
 assert(reg < 6);
 if(!cpu.cr0.protect_enable) { cpu.sreg[reg].selector = selector; return; }
 Assert(!(selector & 4), "LDT is unsupported");
 uint32_t index = selector & ~7u;
 Assert(index + 7 <= cpu.gdtr.limit, "segment selector %x exceeds GDT", selector);
 uint32_t lo = lnaddr_read(cpu.gdtr.base + index, 4);
 uint32_t hi = lnaddr_read(cpu.gdtr.base + index + 4, 4);
 Assert(hi & 0x8000, "segment %x is not present", selector);
 cpu.sreg[reg].selector = selector;
 cpu.sreg[reg].base = (lo >> 16) | ((hi & 0xff) << 16) | (hi & 0xff000000);
 uint32_t limit = (lo & 0xffff) | (hi & 0xf0000);
 cpu.sreg[reg].limit = (hi & 0x800000) ? (limit << 12) | 4095 : limit;
}
lnaddr_t seg_translate(swaddr_t addr, size_t len, uint8_t reg) {
 assert(reg < 6);
 if(!cpu.cr0.protect_enable) return addr;
 Assert(len && addr <= cpu.sreg[reg].limit && len - 1 <= cpu.sreg[reg].limit - addr, "segment limit exceeded: %x", addr);
 return cpu.sreg[reg].base + addr;
}
uint32_t swaddr_read(swaddr_t addr, size_t len, uint8_t reg) {
 return lnaddr_read(seg_translate(addr, len, reg), len);
}
void swaddr_write(swaddr_t addr, size_t len, uint32_t data, uint8_t reg) {
 lnaddr_write(seg_translate(addr, len, reg), len, data);
}
