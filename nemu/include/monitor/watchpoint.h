#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	char expression[128];
	uint32_t value;
} WP;

void init_wp_pool(void);
WP *new_wp(const char *expression, uint32_t value);
bool free_wp(int no);
void print_watchpoints(void);
bool check_watchpoints(swaddr_t eip);

#endif
