#ifndef KERNEL_STORAGE_H
#define KERNEL_STORAGE_H

/*
 * Portable block-storage interface. Each architecture port implements it
 * for its disk hardware (x86: ATA PIO in architecture/x86/drivers/).
 * Sectors are always 512 bytes.
 */

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_SECTOR_BYTES 512

/* Probe for a disk. Safe to call repeatedly; detection runs once. */
void storage_initialize(void);

/* 1 if a disk was detected */
int storage_present(void);

/* Human-readable model name of the detected disk ("" if none) */
const char* storage_model(void);

/* Total addressable sectors (0 if none) */
uint32_t storage_sector_count(void);

/* Read/write whole sectors. Return 1 on success, 0 on failure. */
int storage_read_sectors(uint32_t sector, uint32_t count, void* buffer);
int storage_write_sectors(uint32_t sector, uint32_t count, const void* buffer);

#ifdef __cplusplus
}
#endif

#endif
