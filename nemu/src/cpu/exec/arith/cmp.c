#include "cpu/exec/helper.h"

make_helper(cmp_si2rm_l) {
	int len = decode_si2rm_l(eip + 1);
	uint32_t result = op_dest->val - op_src->val;

	update_eflags_pf_zf_sf(result);
	cpu.eflags.CF = op_dest->val < op_src->val;
	cpu.eflags.OF = ((op_dest->val ^ op_src->val) &
		(op_dest->val ^ result)) >> 31;

	print_asm("cmpl %s,%s", op_src->str, op_dest->str);
	return len + 1;
}
