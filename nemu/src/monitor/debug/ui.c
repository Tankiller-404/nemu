#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

static char *skip_spaces(char *text) {
	if(text == NULL) {
		return NULL;
	}
	while(*text == ' ' || *text == '\t') {
		text ++;
	}
	return *text == '\0' ? NULL : text;
}

static bool parse_number(char *text, uint32_t *value) {
	char *end;
	unsigned long parsed;

	text = skip_spaces(text);
	if(text == NULL) {
		return false;
	}
	errno = 0;
	parsed = strtoul(text, &end, 10);
	if(errno != 0 || end == text || parsed > UINT_MAX || skip_spaces(end) != NULL) {
		return false;
	}
	*value = (uint32_t)parsed;
	return true;
}

/* We use the readline library to provide line editing and command history. */
char* rl_gets() {
	static char *line_read = NULL;

	if(line_read) {
		free(line_read);
		line_read = NULL;
	}
	line_read = readline("(nemu) ");
	if(line_read && *line_read) {
		add_history(line_read);
	}
	return line_read;
}

static int cmd_c(char *args) {
	(void)args;
	cpu_exec((uint32_t)-1);
	return 0;
}

static int cmd_q(char *args) {
	(void)args;
	return -1;
}

static int cmd_si(char *args) {
	uint32_t count = 1;

	if(args != NULL && !parse_number(args, &count)) {
		printf("Usage: si [N]\n");
		return 0;
	}
	if(count == 0) {
		printf("N must be greater than zero.\n");
		return 0;
	}
	cpu_exec(count);
	return 0;
}

static void print_registers() {
	int i;
	for(i = 0; i < 8; i ++) {
		printf("%-3s\t0x%08x\t%u\n", regsl[i], reg_l(i), reg_l(i));
	}
	printf("eip\t0x%08x\t%u\n", cpu.eip, cpu.eip);
}

static int cmd_info(char *args) {
	char subcommand[16];
	char extra[2];

	args = skip_spaces(args);
	if(args == NULL || sscanf(args, "%15s %1s", subcommand, extra) != 1) {
		printf("Usage: info r|w\n");
		return 0;
	}
	if(strcmp(subcommand, "r") == 0) {
		print_registers();
	}
	else if(strcmp(subcommand, "w") == 0) {
		print_watchpoints();
	}
	else {
		printf("Unknown info subcommand '%s'.\n", subcommand);
	}
	return 0;
}

static int cmd_x(char *args) {
	char *end;
	char *expression;
	unsigned long count;
	uint32_t address;
	uint32_t i;
	bool success;

	args = skip_spaces(args);
	if(args == NULL) {
		printf("Usage: x N EXPR\n");
		return 0;
	}
	errno = 0;
	count = strtoul(args, &end, 10);
	expression = skip_spaces(end);
	if(errno != 0 || end == args || count > UINT_MAX || expression == NULL) {
		printf("Usage: x N EXPR\n");
		return 0;
	}
	address = expr(expression, &success);
	if(!success) {
		return 0;
	}
	for(i = 0; i < (uint32_t)count; i ++) {
		uint32_t current = address + i * 4;
		printf("0x%08x: 0x%08x\n", current, swaddr_read(current, 4));
	}
	return 0;
}

static int cmd_p(char *args) {
	bool success;
	uint32_t value;

	args = skip_spaces(args);
	if(args == NULL) {
		printf("Usage: p EXPR\n");
		return 0;
	}
	value = expr(args, &success);
	if(success) {
		printf("0x%08x (%u)\n", value, value);
	}
	return 0;
}

static int cmd_w(char *args) {
	bool success;
	uint32_t value;
	WP *wp;

	args = skip_spaces(args);
	if(args == NULL) {
		printf("Usage: w EXPR\n");
		return 0;
	}
	value = expr(args, &success);
	if(!success) {
		return 0;
	}
	wp = new_wp(args, value);
	if(wp != NULL) {
		printf("Watchpoint %d: %s = 0x%08x\n", wp->NO, wp->expression, wp->value);
	}
	return 0;
}

static int cmd_d(char *args) {
	uint32_t no;

	if(!parse_number(args, &no)) {
		printf("Usage: d N\n");
		return 0;
	}
	if(no > INT_MAX || !free_wp((int)no)) {
		printf("Watchpoint %u does not exist.\n", no);
	}
	else {
		printf("Deleted watchpoint %u.\n", no);
	}
	return 0;
}

static int cmd_help(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{"help", "Display information about supported commands", cmd_help},
	{"c", "Continue the execution of the program", cmd_c},
	{"q", "Exit NEMU", cmd_q},
	{"si", "Execute N instructions (default: 1)", cmd_si},
	{"info", "Display registers (r) or watchpoints (w)", cmd_info},
	{"x", "Examine N 4-byte words starting at EXPR", cmd_x},
	{"p", "Evaluate EXPR", cmd_p},
	{"w", "Set a watchpoint for EXPR", cmd_w},
	{"d", "Delete watchpoint N", cmd_d}
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	char command[32];
	char extra[2];
	int i;

	args = skip_spaces(args);
	if(args == NULL) {
		for(i = 0; i < (int)NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
		return 0;
	}
	if(sscanf(args, "%31s %1s", command, extra) != 1) {
		printf("Usage: help [COMMAND]\n");
		return 0;
	}
	for(i = 0; i < (int)NR_CMD; i ++) {
		if(strcmp(command, cmd_table[i].name) == 0) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
			return 0;
		}
	}
	printf("Unknown command '%s'\n", command);
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end;
		char *cmd;
		char *args;
		int i;

		if(str == NULL) {
			return;
		}
		str_end = str + strlen(str);
		cmd = strtok(str, " \t");
		if(cmd == NULL) {
			continue;
		}

		args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}
		else {
			args = skip_spaces(args);
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		for(i = 0; i < (int)NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) {
					return;
				}
				break;
			}
		}
		if(i == (int)NR_CMD) {
			printf("Unknown command '%s'\n", cmd);
		}
	}
}
