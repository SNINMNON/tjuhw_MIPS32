#include "monitor.h"
#include "temu.h"
#include "expr.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

void display_reg();

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(temu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args) {
	int steps;

	if (args == NULL) {
		steps = 1;
	} else {
		steps = atoi(args);
	}

	cpu_exec(steps);
	return 0;
}

static int cmd_info(char *args) {
	if (args[0] == 'r') {
		display_reg();
	} else if (args[0] == 'w') {
		//TODO
	} else {
		printf("Usage: info [r/w]\n");
	}
	return 0;
}

static int cmd_x(char *args) {
	if (args == NULL) {
		printf("Usage: x N EXPR\n");
		return 0;
	}
  
	int N = 0;
	int nchar = 0;
	if (sscanf(args, "%d%n", &N, &nchar) != 1) {
		printf("Usage: x N EXPR\n");
		return 0;
	}
  
	char *expression = args + nchar;
	while (*expression == ' ') expression++;
	if (*expression == '\0') {
		printf("Usage: x N EXPR\n");
		return 0;
	}
  
	bool success = true;
	uint32_t vaddr = expr(expression, &success);
	if (!success) {
		printf("Bad expression\n");
		return 0;
	}
  
	uint32_t paddr = vaddr & 0x1fffffff;
	for (int i = 0; i < N; i++) {
	  	printf("0x%08x: 0x%08x\n", vaddr + i * 4, mem_read(paddr + i * 4, 4));
	}
	return 0;
  }
  

static int cmd_p(char *args) {
	bool success = true;
	uint32_t result = expr(args, &success);
	if (success) {
		printf("0x%08x\t\t%d\n", result, result);
	} else {
		printf("Bad expression\n");
	}
	return 0;
}

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit TEMU", cmd_q },
	{ "si", "Step N instructions exactly, default N = 1", cmd_si },
	{ "info", "Display the state of registers", cmd_info },
	{ "x", "Examine memory: x N EXPR", cmd_x },
	{ "p", "Evaluate expression EXPR and print the result", cmd_p },

	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
