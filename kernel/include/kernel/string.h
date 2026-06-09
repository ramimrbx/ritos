#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void* memset(void* bufptr, int value, size_t size);
void* memcpy(void* dstptr, const void* srcptr, size_t size);
void* memmove(void* dstptr, const void* srcptr, size_t size);
int memcmp(const void* aptr, const void* bptr, size_t size);
size_t strlen(const char* str);

#ifdef __cplusplus
}
#endif

#endif
