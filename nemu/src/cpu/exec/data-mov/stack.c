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

make_helper(push_i_l) {
	uint32_t value = instr_fetch(eip + 1, 4);
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, value);
	print_asm("pushl $0x%x", value);
	return 5;
}

make_helper(push_si_b) {
	int32_t value = (int8_t)instr_fetch(eip + 1, 1);
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, value);
	print_asm("pushl $0x%x", value);
	return 2;
}

make_helper(push_rm_l) {
	int len = decode_rm_l(eip + 1);
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, op_src->val);
	print_asm("pushl %s", op_src->str);
	return len + 1;
}
