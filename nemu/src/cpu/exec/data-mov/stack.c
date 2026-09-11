#include "cpu/exec/helper.h"

make_helper(push_r_l) {
	int reg = ops_decoded.opcode & 0x7;
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, reg_l(reg));
	print_asm("pushl %%%s", regsl[reg]);
	return 1;
}

make_helper(pop_r_l) {
	int reg = ops_decoded.opcode & 0x7;
	uint32_t value = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	reg_l(reg) = value;
	print_asm("popl %%%s", regsl[reg]);
	return 1;
}
