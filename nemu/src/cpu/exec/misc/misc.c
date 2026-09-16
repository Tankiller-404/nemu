#include "cpu/exec/helper.h"
#include "cpu/decode/modrm.h"

make_helper(nop) {
	print_asm("nop");
	return 1;
}

make_helper(int3) {
	void do_int3();
	do_int3();
	print_asm("int3");

	return 1;
}

make_helper(int_i_b) {
	uint8_t vector = instr_fetch(eip + 1, 1);
	uint32_t syscall = cpu.eax;

	Assert(vector == 0x80, "unsupported interrupt vector 0x%02x", vector);
	if (syscall == 4 && (cpu.ebx == 1 || cpu.ebx == 2)) {
		uint32_t i;
		for (i = 0; i < cpu.edx; i ++) {
			putchar(swaddr_read(cpu.ecx + i, 1));
		}
		fflush(stdout);
		cpu.eax = cpu.edx;
	} else {
		cpu.eax = (uint32_t)-1;
	}

	print_asm("int $0x%02x", vector);
	return 2;
}

make_helper(lea) {
	ModR_M m;
	m.val = instr_fetch(eip + 1, 1);
	int len = load_addr(eip + 1, &m, op_src);
	reg_l(m.reg) = op_src->addr;

	print_asm("leal %s,%%%s", op_src->str, regsl[m.reg]);
	return 1 + len;
}
