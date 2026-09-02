#include "nemu.h"

#include <regex.h>
#include <stdlib.h>
#include <sys/types.h>

enum {
	NOTYPE = 256,
	TK_DEC,
	TK_HEX,
	TK_REG,
	TK_EQ,
	TK_NEQ,
	TK_AND,
	TK_DEREF,
	TK_NEG
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {
	{"[ \t]+", NOTYPE},
	{"0[xX][0-9a-fA-F]+", TK_HEX},
	{"[0-9]+", TK_DEC},
	{"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG},
	{"==", TK_EQ},
	{"!=", TK_NEQ},
	{"&&", TK_AND},
	{"\\+", '+'},
	{"-", '-'},
	{"\\*", '*'},
	{"/", '/'},
	{"\\(", '('},
	{"\\)", ')'}
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))
#define NR_TOKEN 128

static regex_t re[NR_REGEX];

void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < (int)NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, sizeof(error_msg));
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

static Token tokens[NR_TOKEN];
static int nr_token;

static bool is_value_token(int type) {
	return type == TK_DEC || type == TK_HEX || type == TK_REG || type == ')';
}

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;
	while(e[position] != '\0') {
		for(i = 0; i < (int)NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;
				int type = rules[i].token_type;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
						i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				if(type != NOTYPE) {
					if(nr_token >= NR_TOKEN) {
						printf("Expression has too many tokens (maximum %d).\n", NR_TOKEN);
						return false;
					}
					if(substr_len >= (int)sizeof(tokens[nr_token].str)) {
						printf("Token is too long at position %d.\n", position - substr_len);
						return false;
					}

					tokens[nr_token].type = type;
					memcpy(tokens[nr_token].str, substr_start, substr_len);
					tokens[nr_token].str[substr_len] = '\0';
					nr_token ++;
				}
				break;
			}
		}

		if(i == (int)NR_REGEX) {
			printf("No match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	for(i = 0; i < nr_token; i ++) {
		if(tokens[i].type == '*' && (i == 0 || !is_value_token(tokens[i - 1].type))) {
			tokens[i].type = TK_DEREF;
		}
		else if(tokens[i].type == '-' && (i == 0 || !is_value_token(tokens[i - 1].type))) {
			tokens[i].type = TK_NEG;
		}
	}

	return true;
}

static bool parentheses_enclose(int p, int q, bool *valid) {
	int depth = 0;
	int i;
	bool encloses = tokens[p].type == '(' && tokens[q].type == ')';

	*valid = true;
	for(i = p; i <= q; i ++) {
		if(tokens[i].type == '(') {
			depth ++;
		}
		else if(tokens[i].type == ')') {
			depth --;
			if(depth < 0) {
				*valid = false;
				return false;
			}
		}
		if(encloses && depth == 0 && i < q) {
			encloses = false;
		}
	}

	if(depth != 0) {
		*valid = false;
		return false;
	}
	return encloses;
}

static int binary_precedence(int type) {
	switch(type) {
		case TK_AND: return 1;
		case TK_EQ:
		case TK_NEQ: return 2;
		case '+':
		case '-': return 3;
		case '*':
		case '/': return 4;
		default: return 0;
	}
}

static uint32_t register_value(const char *name, bool *success) {
	int i;
	const char *reg_name = name + 1;

	for(i = 0; i < 8; i ++) {
		if(strcmp(reg_name, regsl[i]) == 0) {
			return reg_l(i);
		}
		if(strcmp(reg_name, regsw[i]) == 0) {
			return reg_w(i);
		}
		if(strcmp(reg_name, regsb[i]) == 0) {
			return reg_b(i);
		}
	}
	if(strcmp(reg_name, "eip") == 0) {
		return cpu.eip;
	}

	printf("Unknown register '%s'.\n", name);
	*success = false;
	return 0;
}

static uint32_t eval(int p, int q, bool *success) {
	bool valid;
	int depth;
	int op;
	int best_precedence;
	int i;
	uint32_t lhs;
	uint32_t rhs;

	if(p > q) {
		*success = false;
		return 0;
	}

	if(p == q) {
		switch(tokens[p].type) {
			case TK_DEC:
				return (uint32_t)strtoul(tokens[p].str, NULL, 10);
			case TK_HEX:
				return (uint32_t)strtoul(tokens[p].str, NULL, 16);
			case TK_REG:
				return register_value(tokens[p].str, success);
			default:
				*success = false;
				return 0;
		}
	}

	if(parentheses_enclose(p, q, &valid)) {
		return eval(p + 1, q - 1, success);
	}
	if(!valid) {
		*success = false;
		return 0;
	}

	depth = 0;
	op = -1;
	best_precedence = 100;
	for(i = p; i <= q; i ++) {
		int precedence;
		if(tokens[i].type == '(') {
			depth ++;
			continue;
		}
		if(tokens[i].type == ')') {
			depth --;
			continue;
		}
		if(depth != 0) {
			continue;
		}

		precedence = binary_precedence(tokens[i].type);
		if(precedence != 0 && precedence <= best_precedence) {
			best_precedence = precedence;
			op = i;
		}
	}

	if(op >= 0) {
		lhs = eval(p, op - 1, success);
		if(!*success) {
			return 0;
		}
		rhs = eval(op + 1, q, success);
		if(!*success) {
			return 0;
		}

		switch(tokens[op].type) {
			case '+': return lhs + rhs;
			case '-': return lhs - rhs;
			case '*': return lhs * rhs;
			case '/':
				if(rhs == 0) {
					printf("Division by zero.\n");
					*success = false;
					return 0;
				}
				return lhs / rhs;
			case TK_EQ: return lhs == rhs;
			case TK_NEQ: return lhs != rhs;
			case TK_AND: return lhs && rhs;
			default:
				*success = false;
				return 0;
		}
	}

	if(tokens[p].type == TK_NEG || tokens[p].type == TK_DEREF) {
		rhs = eval(p + 1, q, success);
		if(!*success) {
			return 0;
		}
		if(tokens[p].type == TK_NEG) {
			return 0 - rhs;
		}
		return swaddr_read(rhs, 4);
	}

	*success = false;
	return 0;
}

uint32_t expr(char *e, bool *success) {
	uint32_t value;

	*success = false;
	if(e == NULL || !make_token(e) || nr_token == 0) {
		return 0;
	}

	*success = true;
	value = eval(0, nr_token - 1, success);
	if(!*success) {
		printf("Invalid expression: %s\n", e);
		return 0;
	}
	return value;
}
