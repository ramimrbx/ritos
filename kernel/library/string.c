#include <kernel/string.h>

/* Word-at-a-time memset/memcpy: framebuffer flushes move megabytes per
 * frame into (slow, uncached) video memory, where 32-bit transactions are
 * 4x fewer bus writes than the naive byte loop. */
void* memset(void* bufptr, int value, size_t size) {
	unsigned char* buf = (unsigned char*) bufptr;
	unsigned char v = (unsigned char) value;
	while (size > 0 && ((uintptr_t)buf & 3)) { *buf++ = v; size--; }
	uint32_t word = (uint32_t)v * 0x01010101u;
	uint32_t* buf32 = (uint32_t*) buf;
	while (size >= 4) { *buf32++ = word; size -= 4; }
	buf = (unsigned char*) buf32;
	while (size > 0) { *buf++ = v; size--; }
	return bufptr;
}

void* memcpy(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	/* Align the destination (the VRAM side), then move words */
	while (size > 0 && ((uintptr_t)dst & 3)) { *dst++ = *src++; size--; }
	uint32_t* d32 = (uint32_t*) dst;
	const uint32_t* s32 = (const uint32_t*) src;
	while (size >= 16) {
		d32[0] = s32[0]; d32[1] = s32[1]; d32[2] = s32[2]; d32[3] = s32[3];
		d32 += 4; s32 += 4; size -= 16;
	}
	while (size >= 4) { *d32++ = *s32++; size -= 4; }
	dst = (unsigned char*) d32;
	src = (const unsigned char*) s32;
	while (size > 0) { *dst++ = *src++; size--; }
	return dstptr;
}

void* memmove(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	if (dst < src) {
		for (size_t i = 0; i < size; i++) {
			dst[i] = src[i];
		}
	} else {
		for (size_t i = size; i > 0; i--) {
			dst[i-1] = src[i-1];
		}
	}
	return dstptr;
}

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i]) {
			return -1;
		} else if (a[i] > b[i]) {
			return 1;
		}
	}
	return 0;
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len] != '\0') {
		len++;
	}
	return len;
}
