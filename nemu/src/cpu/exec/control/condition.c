#include "cpu/exec/helper.h"

static bool condition(uint8_t code) {
	switch(code & 0xf) {
		case 0x0: return cpu.eflags.OF;
		case 0x1: return !cpu.eflags.OF;
		case 0x2: return cpu.eflags.CF;
		case 0x3: return !cpu.eflags.CF;
		case 0x4: return cpu.eflags.ZF;
		case 0x5: return !cpu.eflags.ZF;
		case 0x6: return cpu.eflags.CF || cpu.eflags.ZF;
		case 0x7: return !cpu.eflags.CF && !cpu.eflags.ZF;
		case 0x8: return cpu.eflags.SF;
		case 0x9: return !cpu.eflags.SF;
		case 0xa: return cpu.eflags.PF;
		case 0xb: return !cpu.eflags.PF;
		case 0xc: return cpu.eflags.SF != cpu.eflags.OF;
		case 0xd: return cpu.eflags.SF == cpu.eflags.OF;
		case 0xe: return cpu.eflags.ZF || cpu.eflags.SF != cpu.eflags.OF;
		case 0xf: return !cpu.eflags.ZF && cpu.eflags.SF == cpu.eflags.OF;
	}
	return false;
}

make_helper(jcc_si_b) {
	int8_t displacement = instr_fetch(eip + 1, 1);
	if(condition(ops_decoded.opcode)) cpu.eip += displacement;
	print_asm("jcc %d", displacement);
	return 2;
}

make_helper(jcc_si_l) {
	int32_t displacement = instr_fetch(eip + 1, 4);
	if(condition(ops_decoded.opcode)) cpu.eip += displacement;
	print_asm("jcc %d", displacement);
	return 5;
}

make_helper(jecxz_si_b) {
	int8_t displacement = instr_fetch(eip + 1, 1);
	if(cpu.ecx == 0) cpu.eip += displacement;
	print_asm("jecxz %d", displacement);
	return 2;
}

make_helper(setcc_rm_b) {
	int len = decode_rm_b(eip + 1);
	write_operand_b(op_src, condition(ops_decoded.opcode));
	print_asm("setcc %s", op_src->str);
	return len + 1;
}

make_helper(leave) {
	cpu.esp = cpu.ebp;
	cpu.ebp = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	print_asm("leave");
	return 1;
}

make_helper(sahf) {
	uint8_t flags = reg_b(R_AH);
	cpu.eflags.SF = (flags >> 7) & 1;
	cpu.eflags.ZF = (flags >> 6) & 1;
	cpu.eflags.AF = (flags >> 4) & 1;
	cpu.eflags.PF = (flags >> 2) & 1;
	cpu.eflags.CF = flags & 1;
	print_asm("sahf");
	return 1;
}

make_helper(cld) {
	cpu.eflags.DF = 0;
	print_asm("cld");
	return 1;
}

make_helper(std) {
	cpu.eflags.DF = 1;
	print_asm("std");
	return 1;
}
