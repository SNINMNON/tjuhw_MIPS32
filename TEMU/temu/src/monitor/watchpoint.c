#include "watchpoint.h"
#include "expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP* head, * free_;

void init_wp_pool() {
	for (int i = 0; i < NR_WP - 1; i++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].NO = NR_WP - 1;
	wp_pool[NR_WP - 1].next = NULL;
  
	head = NULL;
	free_ = wp_pool;
  }

void new_wp(char * expression) {
	if (free_ == NULL) {
		printf("No free watchpoints available (MAX=%d)\n", NR_WP);
		return;
	}
	WP* wp = free_;
	free_ = free_->next;
	wp->next = head;
	head = wp;

	bool success = true;
	strncpy(wp->expr, expression, sizeof(wp->expr) - 1);
	wp->expr[sizeof(wp->expr) - 1] = '\0';
	wp->last_value = expr(expression, &success);
	if (!success) {
		printf("Failed to evaluate expression for new watchpoint: %s\n", expression);
	}
	return;
}

void free_wp(int NO) {
	if (NO < 0 || NO >= NR_WP) {
		printf("Invalid watchpoint number (MAX=%d)\n", NR_WP);
		return;
	}
	WP* wp = &wp_pool[NO];
	if (head == NULL || wp == NULL) {
		printf("No watchpoints to free\n");
		return;
	}

	if (head == wp) {
		head = head->next;
	} else {
		WP* prev = head;
		while (prev->next != NULL && prev->next != wp) {
			prev = prev->next;
		}
		if (prev->next == wp) {
			prev->next = wp->next;
		} else {
			printf("Watchpoint not found\n");
			return;
		}
	}

	wp->next = free_;
	free_ = wp;
	return;
}

bool update_watchpoints() {
	WP* wp = head;
	bool triggered = false;
	while (wp != NULL) {
		bool success = true;
		uint32_t new_value = expr(wp->expr, &success);
		if (success) {
			if (new_value != wp->last_value) {
				printf("Watchpoint %d triggered: %s\n", wp->NO, wp->expr);
				printf("Old value = 0x%08x, New value = 0x%08x\n", wp->last_value, new_value);
				wp->last_value = new_value;
				triggered = true;
			}
		} else {
			printf("Failed to evaluate expression for watchpoint %d: %s\n", wp->NO, wp->expr);
		}
		wp = wp->next;
	}
	return triggered;
}

void print_wp() {
	WP* wp = head;
	if (wp == NULL) {
		printf("No watchpoints set.\n");
		return;
	}
	printf("NO.\tExpression\tLast Value\n");
	while (wp != NULL) {
		printf("%d\t%s\t0x%08x\n", wp->NO, wp->expr, wp->last_value);
		wp = wp->next;
	}
}