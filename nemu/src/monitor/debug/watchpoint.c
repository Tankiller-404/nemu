#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
		wp_pool[i].expression[0] = '\0';
		wp_pool[i].value = 0;
	}
	wp_pool[NR_WP - 1].next = NULL;
	head = NULL;
	free_ = wp_pool;
}

WP *new_wp(const char *expression, uint32_t value) {
	WP *wp;

	if(free_ == NULL) {
		printf("No free watchpoints.\n");
		return NULL;
	}
	if(strlen(expression) >= sizeof(wp_pool[0].expression)) {
		printf("Watchpoint expression is too long.\n");
		return NULL;
	}

	wp = free_;
	free_ = free_->next;
	strcpy(wp->expression, expression);
	wp->value = value;
	wp->next = head;
	head = wp;
	return wp;
}

bool free_wp(int no) {
	WP **link = &head;
	WP *wp;

	while(*link != NULL && (*link)->NO != no) {
		link = &(*link)->next;
	}
	if(*link == NULL) {
		return false;
	}

	wp = *link;
	*link = wp->next;
	wp->expression[0] = '\0';
	wp->next = free_;
	free_ = wp;
	return true;
}

void print_watchpoints() {
	WP *wp;

	if(head == NULL) {
		printf("No watchpoints.\n");
		return;
	}

	printf("Num\tValue\t\tExpression\n");
	for(wp = head; wp != NULL; wp = wp->next) {
		printf("%d\t0x%08x\t%s\n", wp->NO, wp->value, wp->expression);
	}
}

bool check_watchpoints(swaddr_t eip) {
	WP *wp;
	bool triggered = false;

	for(wp = head; wp != NULL; wp = wp->next) {
		bool success;
		uint32_t new_value = expr(wp->expression, &success);

		if(!success) {
			printf("Could not evaluate watchpoint %d: %s\n", wp->NO, wp->expression);
			continue;
		}
		if(new_value != wp->value) {
			printf("Hint watchpoint %d at address 0x%08x\n", wp->NO, eip);
			printf("Old value = 0x%08x\nNew value = 0x%08x\n",
					wp->value, new_value);
			wp->value = new_value;
			triggered = true;
		}
	}
	return triggered;
}
