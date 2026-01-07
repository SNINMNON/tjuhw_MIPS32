#include "common.h"
#define UART_DATA  0x1FD003F8
#define UART_STAT  0x1FD003FC

uint32_t dram_read(uint32_t, size_t);
void dram_write(uint32_t, size_t, uint32_t);

/* Memory accessing interfaces */

uint32_t mem_read(uint32_t addr, size_t len) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif

	if (addr == UART_STAT && len == 1) {
		return 0x03; // bit0=TX ready, bit1=RX ready
	}
	if (addr == UART_DATA && len == 1) {
        return 'T';  // 让 READSERIAL 直接通过
    }

	return dram_read(addr, len) & (~0u >> ((4 - len) << 3));
}

void mem_write(uint32_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif

	if (addr == UART_DATA && len == 1) {
		putchar((char)(data & 0xFF));
		fflush(stdout);
		return;
	}

	dram_write(addr, len, data);
}

