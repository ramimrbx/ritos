#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
size_t get_heap_usage(void);

#ifdef __cplusplus
}
#endif

#endif
