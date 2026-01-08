#include "trace.h"
#include <stdio.h>

static FILE* fp = NULL;

void init_trace(const char* path) {
    fp = fopen(path, "w");
    if (!fp) {
        perror("init_trace fopen");
    }
    fprintf(fp, "%-8s\t%-4s\t%-8s\n", "PC", "REG", "VALUE");
}

void trace_close(void) {
    if (fp) fclose(fp);
    fp = NULL;
}

void trace_log(uint32_t pc, int reg_no, uint32_t value) {
    if (!fp) return;
    if (reg_no <= 0) return;
    if (reg_no >= 32) return;

    fprintf(fp, "%08x\t%02d  \t%08x\n", pc, reg_no, value);
}
