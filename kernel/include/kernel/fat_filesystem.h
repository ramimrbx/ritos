#ifndef KERNEL_FAT_FILESYSTEM_H
#define KERNEL_FAT_FILESYSTEM_H

/*
 * Filesystem layer over the block-storage interface. Detects what lives on
 * the disk (FAT12/16/32, ext2, NTFS, exFAT, partition tables) and can read
 * and write files on FAT16/FAT32 volumes (root directory, 8.3 names).
 */

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fill 'out' with a human-readable description of the disk and what
 * filesystem was detected on it. Returns 1 if an operable FAT volume was
 * found, 0 otherwise (description is still written). */
int fat_volume_description(char* out, int out_max);

/* 1 if a FAT volume is mounted and operable */
int fat_volume_ready(void);

/* Call the callback once per file in the root directory.
 * Returns the number of files, or -1 if no volume. */
typedef void (*fat_list_callback)(const char* name, uint32_t size, void* user);
int fat_list_root_directory(fat_list_callback callback, void* user);

/* Read the named file (e.g. "hello.txt", case-insensitive) into buffer.
 * Returns 1 on success and stores the file size in out_size. */
int fat_read_file(const char* name, char* buffer, uint32_t buffer_max, uint32_t* out_size);

/* Create or overwrite the named file in the root directory.
 * Returns 1 on success. */
int fat_write_file(const char* name, const char* data, uint32_t size);

/* Delete a root-directory file by 8.3 name; 1 on success */
int fat_delete_file(const char* name);

/* Rename a root-directory file (8.3 names, no overwrite); 1 on success */
int fat_rename_file(const char* old_name, const char* new_name);

#ifdef __cplusplus
}
#endif

#endif
