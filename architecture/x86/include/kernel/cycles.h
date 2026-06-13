#ifndef KERNEL_CYCLES_H
#define KERNEL_CYCLES_H

/* Per-port CPU cycle counter (x86: TSC). Used only for relative load
 * measurement — ratios of busy/total cycles — so the absolute frequency
 * never needs to be known. */

#include <stdint.h>

static inline uint64_t cpu_cycles(void) {
	uint32_t lo, hi;
	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

#endif
