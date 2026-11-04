#include "cpu/exec/helper.h"
#include "cpu/decode/modrm.h"

make_helper(lgdt) {
 ModR_M m; Operand mem;
 m.val = instr_fetch(eip + 1, 1);
 Assert(m.mod != 3, "LGDT requires memory");
 int len = load_addr(eip + 1, &m, &mem);
 cpu.gdtr.limit = swaddr_read(mem.addr, 2, mem.sreg);
 cpu.gdtr.base = swaddr_read(mem.addr + 2, 4, mem.sreg);
 if(ops_decoded.is_operand_size_16) cpu.gdtr.base &= 0xffffff;
 print_asm("lgdt"); return len + 1;
}
make_helper(mov_cr2r) {
 ModR_M m; m.val = instr_fetch(eip + 1, 1);
 Assert(m.mod == 3 && (m.reg == 0 || m.reg == 3), "unsupported control register");
 reg_l(m.R_M) = m.reg == 0 ? cpu.cr0.val : cpu.cr3.val;
 print_asm("mov cr,r"); return 2;
}
make_helper(mov_r2cr) {
 ModR_M m; m.val = instr_fetch(eip + 1, 1);
 Assert(m.mod == 3 && (m.reg == 0 || m.reg == 3), "unsupported control register");
 if(m.reg == 0) cpu.cr0.val = reg_l(m.R_M); else cpu.cr3.val = reg_l(m.R_M);
 init_tlb(); print_asm("mov r,cr"); return 2;
}
make_helper(mov_rm2sreg) {
 ModR_M m; Operand mem;
 m.val = instr_fetch(eip + 1, 1);
 Assert(m.reg < 6 && m.reg != R_CS, "invalid MOV segment destination");
 int len = 1; uint16_t selector;
 if(m.mod == 3) selector = reg_w(m.R_M);
 else { len = load_addr(eip + 1, &m, &mem); selector = swaddr_read(mem.addr, 2, mem.sreg); }
 load_segment(m.reg, selector); print_asm("mov rm,sreg"); return len + 1;
}
make_helper(ljmp) {
 int bytes = ops_decoded.is_operand_size_16 ? 2 : 4;
 uint32_t target = instr_fetch(eip + 1, bytes);
 uint16_t selector = instr_fetch(eip + 1 + bytes, 2);
 load_segment(R_CS, selector);
 int len = 1 + bytes + 2;
 cpu.eip = target - len;
 print_asm("ljmp %x:%x", selector, target); return len;
}
