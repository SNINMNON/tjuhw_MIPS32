#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdint.h>
#include <stdbool.h>

void init_trace(const char* path);
void trace_close(void);

/* 生成指令Golden Trace */
void trace_log(uint32_t pc, int reg_no, uint32_t value);

#endif
