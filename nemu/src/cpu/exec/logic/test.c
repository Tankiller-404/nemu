#include "cpu/exec/helper.h"

make_helper(test_r2rm_l) {
	int len = decode_r2rm_l(eip + 1);
	uint32_t result = op_dest->val & op_src->val;

	update_eflags_pf_zf_sf(result);
	cpu.eflags.CF = 0;
	cpu.eflags.OF = 0;

	print_asm("testl %s,%s", op_src->str, op_dest->str);
	return len + 1;
}
