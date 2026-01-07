#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	char expr[256];
	uint32_t last_value;
  } WP;

void init_wp_pool();
void new_wp(char* expression);
void free_wp(int NO);
bool update_watchpoints();

#endif
