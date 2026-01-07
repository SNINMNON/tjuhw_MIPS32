#include "reg.h"
#include "temu.h"

#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

enum {
	NOTYPE = 256,

	TK_DEC, // decimal number
	TK_HEX, // hex number
	TK_REG, // register name, starts with $

	TK_EQ,  // ==
	TK_NEQ, // !=
	TK_AND, // &&
	TK_OR,  // ||

	TK_DEREF // unary *
};

static struct rule {
	char* regex;
	int token_type;
} rules[] = {
	{" +", NOTYPE}, // spaces

	{"0[xX][0-9a-fA-F]+", TK_HEX},       // hex number
	{"[0-9]+", TK_DEC},                  // decimal number
	{"\\$[a-zA-Z][0-9a-zA-Z]*", TK_REG}, // register: $...

	{"==", TK_EQ},
	{"!=", TK_NEQ},
	{"&&", TK_AND},
	{"\\|\\|", TK_OR},

	{"\\+", '+'},
	{"-", '-'},
	{"\\*", '*'},
	{"/", '/'},
	{"!", '!'},
	{"\\(", '('},
	{"\\)", ')'},
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))
static regex_t re[NR_REGEX];

void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for (i = 0; i < (int)NR_REGEX; i++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if (ret != 0) {
			regerror(ret, &re[i], error_msg, sizeof(error_msg));
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg,
				rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

static Token tokens[256];
static int nr_token;

static inline bool is_binary_op(int t) {
	return t == TK_OR || t == TK_AND || t == TK_EQ || t == TK_NEQ || t == '+' ||
		t == '-' || t == '*' || t == '/';
}

static inline bool is_operator_or_lparen(int t) {
	// 用于判断 '*' 是否是解引用
	return is_binary_op(t) || t == '!' || t == TK_DEREF || t == '(';
}

static int precedence(int t) {
	// 数字越小优先级越低
	switch (t) {
	case TK_OR:
		return 1;
	case TK_AND:
		return 2;
	case TK_EQ:
	case TK_NEQ:
		return 3;
	case '+':
	case '-':
		return 4;
	case '*':
	case '/':
		return 5;
	default:
		return 100;
	}
}

static bool make_token(char* e) {
	int position = 0;

	nr_token = 0;

	while (e[position] != '\0') {
		int i;
		regmatch_t pmatch;

		for (i = 0; i < (int)NR_REGEX; i++) {
			if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
				pmatch.rm_so == 0) {
				int substr_len = pmatch.rm_eo;
				char* substr_start = e + position;
				position += substr_len;

				int type = rules[i].token_type;
				if (type == NOTYPE) {
					break; // skip spaces
				}

				Assert(nr_token < (int)(sizeof(tokens) / sizeof(tokens[0])), "too many tokens");

				tokens[nr_token].type = type;

				int copy_len = substr_len < (int)sizeof(tokens[nr_token].str) - 1
					? substr_len
					: (int)sizeof(tokens[nr_token].str) - 1;
				memcpy(tokens[nr_token].str, substr_start, copy_len);
				tokens[nr_token].str[copy_len] = '\0';

				nr_token++;
				break;
			}
		}

		if (i == (int)NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true;
}

// 去掉前导 '$' 后比较
static bool reg_name_equal(const char* tok, const char* rf) {
	const char* a = tok;
	const char* b = rf;
	if (a[0] == '$')
		a++;
	if (b[0] == '$')
		b++;
	return strcmp(a, b) == 0;
}

static uint32_t read_reg_value(const char* tok, bool* ok) {
	if (reg_name_equal(tok, "pc")) {
		*ok = true;
		return cpu.pc;
	}
	if (reg_name_equal(tok, "hi")) {
		*ok = true;
		return cpu.hi;
	}
	if (reg_name_equal(tok, "lo")) {
		*ok = true;
		return cpu.lo;
	}

	for (int i = 0; i < 32; i++) {
		if (reg_name_equal(tok, regfile[i])) {
			*ok = true;
			return cpu.gpr[i]._32;
		}
	}

	*ok = false;
	return 0;
}

static bool check_parentheses(int l, int r) {
	if (tokens[l].type != '(' || tokens[r].type != ')')
		return false;

	int depth = 0;
	for (int i = l; i <= r; i++) {
		if (tokens[i].type == '(')
			depth++;
		else if (tokens[i].type == ')')
			depth--;
		if (depth == 0 && i < r)
			return false; // 外层括号提前闭合，说明不是整段包起来
		if (depth < 0)
			return false;
	}
	return depth == 0;
}

static uint32_t eval(int l, int r, bool* success) {
	if (!*success)
		return 0;

	if (l > r) {
		*success = false;
		return 0;
	}

	if (l == r) {
		int t = tokens[l].type;
		if (t == TK_DEC) {
			return (uint32_t)strtoul(tokens[l].str, NULL, 10);
		}
		if (t == TK_HEX) {
			return (uint32_t)strtoul(tokens[l].str, NULL, 16);
		}
		if (t == TK_REG) {
			bool ok = true;
			uint32_t v = read_reg_value(tokens[l].str, &ok);
			if (!ok)
				*success = false;
			return v;
		}
		*success = false;
		return 0;
	}

	if (check_parentheses(l, r)) {
		return eval(l + 1, r - 1, success);
	}

	// 找主运算符：括号外，最低优先级；同级取最右（保证左结合）
	int main_op = -1;
	int min_prec = 100;
	int depth = 0;

	for (int i = l; i <= r; i++) {
		int t = tokens[i].type;
		if (t == '(') {
			depth++;
			continue;
		}
		if (t == ')') {
			depth--;
			continue;
		}
		if (depth != 0)
			continue;

		if (is_binary_op(t)) {
			int p = precedence(t);
			if (p <= min_prec) {
				min_prec = p;
				main_op = i;
			}
		}
	}

	// 没有二元运算符，则必须是一元：!expr 或 *expr
	if (main_op == -1) {
		int t = tokens[l].type;
		if (t == '!') {
			uint32_t v = eval(l + 1, r, success);
			if (!*success)
				return 0;
			return (v == 0) ? 1u : 0u;
		}
		if (t == TK_DEREF) {
			uint32_t addr = eval(l + 1, r, success);
			if (!*success)
				return 0;
			uint32_t paddr = addr & 0x1fffffff; // 映射到物理地址
			return mem_read(paddr, 4);
		}
		*success = false;
		return 0;
	}

	// 对 && || 做短路
	int op = tokens[main_op].type;

	if (op == TK_AND) {
		uint32_t left = eval(l, main_op - 1, success);
		if (!*success)
			return 0;
		if (left == 0)
			return 0;
		uint32_t right = eval(main_op + 1, r, success);
		if (!*success)
			return 0;
		return (right != 0) ? 1u : 0u;
	}

	if (op == TK_OR) {
		uint32_t left = eval(l, main_op - 1, success);
		if (!*success)
			return 0;
		if (left != 0)
			return 1u;
		uint32_t right = eval(main_op + 1, r, success);
		if (!*success)
			return 0;
		return (right != 0) ? 1u : 0u;
	}

	uint32_t val1 = eval(l, main_op - 1, success);
	uint32_t val2 = eval(main_op + 1, r, success);
	if (!*success)
		return 0;

	switch (op) {
	case '+':
		return val1 + val2;
	case '-':
		return val1 - val2;
	case '*':
		return val1 * val2;
	case '/':
		if (val2 == 0) {
			*success = false;
			return 0;
		}
		return val1 / val2;

	case TK_EQ:
		return (val1 == val2) ? 1u : 0u;
	case TK_NEQ:
		return (val1 != val2) ? 1u : 0u;

	default:
		*success = false;
		return 0;
	}
}

uint32_t expr(char* e, bool* success) {
	*success = true;

	if (!make_token(e)) {
		*success = false;
		return 0;
	}

	// 识别一元 *（解引用）：出现在开头，或前一个 token 是运算符/左括号
	for (int i = 0; i < nr_token; i++) {
		if (tokens[i].type == '*') {
			if (i == 0 || is_operator_or_lparen(tokens[i - 1].type)) {
				tokens[i].type = TK_DEREF;
			}
		}
	}

	return eval(0, nr_token - 1, success);
}
