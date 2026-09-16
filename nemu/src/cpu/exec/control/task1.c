#include "cpu/exec/helper.h"

make_helper(call_rel_l) {
	int32_t displacement = (int32_t)instr_fetch(eip + 1, 4);

	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, eip + 5);
	cpu.eip += displacement;

	print_asm("call %x", eip + 5 + displacement);
	return 5;
}

make_helper(call_rm_l) {
	int len = decode_rm_l(eip + 1);
	uint32_t return_address = eip + len + 1;
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, return_address);
	cpu.eip = op_src->val - (len + 1);
	print_asm("call *%s", op_src->str);
	return len + 1;
}

make_helper(je_si_b) {
	int8_t displacement = (int8_t)instr_fetch(eip + 1, 1);
	if(cpu.eflags.ZF) {
		cpu.eip += displacement;
	}

	print_asm("je %x", eip + 2 + displacement);
	return 2;
}

make_helper(ret) {
	uint32_t target = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	cpu.eip = target - 1;

	print_asm("ret");
	return 1;
}
