#include "cpu/exec/template-start.h"

#define instr add

static void do_execute() {
	DATA_TYPE lhs = op_dest->val;
	DATA_TYPE rhs = op_src->val;
	DATA_TYPE result = lhs + rhs;
	OPERAND_W(op_dest, result);

	update_eflags_pf_zf_sf((DATA_TYPE_S)result);
	cpu.eflags.CF = result < lhs;
	cpu.eflags.OF = MSB((~(lhs ^ rhs)) & (lhs ^ result));
	print_asm_template2();
}

make_instr_helper(i2a)
make_instr_helper(i2rm)
#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm)
#endif
make_instr_helper(r2rm)
make_instr_helper(rm2r)

#include "cpu/exec/template-end.h"
