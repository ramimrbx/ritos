#include <kernel/fat_filesystem.h>
#include <kernel/storage.h>
#include <kernel/string.h>

/*
 * FAT16/FAT32 driver (root directory, 8.3 names) plus detection of other
 * filesystems for reporting. Works on whole-disk volumes and on the first
 * partition of an MBR-partitioned disk. Single-volume, not reentrant -
 * fine for a polling kernel.
 */

#define FAT_TYPE_NONE 0
#define FAT_TYPE_12   12
#define FAT_TYPE_16   16
#define FAT_TYPE_32   32

#define DIR_ENTRY_BYTES   32
#define ATTR_READ_ONLY    0x01
#define ATTR_VOLUME_LABEL 0x08
#define ATTR_DIRECTORY    0x10
#define ATTR_ARCHIVE      0x20
#define ATTR_LONG_NAME    0x0F

static struct {
	int      probed;
	int      ready;
	int      fat_type;
	uint32_t volume_start;       /* LBA of the volume's boot sector */
	uint32_t bytes_per_sector;
	uint32_t sectors_per_cluster;
	uint32_t reserved_sectors;
	uint32_t fat_count;
	uint32_t fat_sectors;        /* size of one FAT */
	uint32_t root_entry_count;   /* FAT12/16 */
	uint32_t root_cluster;       /* FAT32 */
	uint32_t fat_start;          /* relative to volume_start */
	uint32_t root_start;         /* FAT12/16: first root dir sector */
	uint32_t root_sectors;       /* FAT12/16 */
	uint32_t data_start;
	uint32_t cluster_count;
	char     detected_name[32];  /* what we found, e.g. "FAT16", "ext2" */
} g_volume;

static uint8_t g_sector[STORAGE_SECTOR_BYTES];
static uint8_t g_fat_sector[STORAGE_SECTOR_BYTES];
static uint32_t g_fat_sector_loaded = 0xFFFFFFFF;

static uint16_t read_u16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t read_u32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write_u16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void write_u32(uint8_t* p, uint32_t v) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void copy_name(char* dst, const char* src, int max) {
	int i = 0;
	while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
	dst[i] = '\0';
}

/* Try to parse 'sector_data' as a FAT boot sector at LBA 'start'. */
static int parse_fat_boot_sector(const uint8_t* b, uint32_t start) {
	if (b[510] != 0x55 || b[511] != 0xAA) return 0;
	if (b[0] != 0xEB && b[0] != 0xE9) return 0;
	uint32_t bytes_per_sector = read_u16(b + 11);
	uint32_t sectors_per_cluster = b[13];
	if (bytes_per_sector != 512) return 0;
	if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1))) return 0;

	uint32_t reserved   = read_u16(b + 14);
	uint32_t fat_count  = b[16];
	uint32_t root_count = read_u16(b + 17);
	uint32_t total16    = read_u16(b + 19);
	uint32_t fat16_size = read_u16(b + 22);
	uint32_t total32    = read_u32(b + 32);
	uint32_t fat32_size = read_u32(b + 36);
	if (reserved == 0 || fat_count == 0) return 0;

	uint32_t fat_sectors = fat16_size ? fat16_size : fat32_size;
	uint32_t total = total16 ? total16 : total32;
	if (fat_sectors == 0 || total == 0) return 0;

	uint32_t root_sectors = (root_count * DIR_ENTRY_BYTES + 511) / 512;
	uint32_t fat_start    = reserved;
	uint32_t root_start   = fat_start + fat_count * fat_sectors;
	uint32_t data_start   = root_start + root_sectors;
	if (data_start >= total) return 0;
	uint32_t clusters = (total - data_start) / sectors_per_cluster;

	g_volume.volume_start        = start;
	g_volume.bytes_per_sector    = bytes_per_sector;
	g_volume.sectors_per_cluster = sectors_per_cluster;
	g_volume.reserved_sectors    = reserved;
	g_volume.fat_count           = fat_count;
	g_volume.fat_sectors         = fat_sectors;
	g_volume.root_entry_count    = root_count;
	g_volume.root_cluster        = read_u32(b + 44);
	g_volume.fat_start           = fat_start;
	g_volume.root_start          = root_start;
	g_volume.root_sectors        = root_sectors;
	g_volume.data_start          = data_start;
	g_volume.cluster_count       = clusters;

	if (clusters < 4085) {
		g_volume.fat_type = FAT_TYPE_12;
		copy_name(g_volume.detected_name, "FAT12", 32);
	} else if (clusters < 65525) {
		g_volume.fat_type = FAT_TYPE_16;
		copy_name(g_volume.detected_name, "FAT16", 32);
	} else {
		g_volume.fat_type = FAT_TYPE_32;
		copy_name(g_volume.detected_name, "FAT32", 32);
	}
	/* FAT12 detection works, but read/write here supports 16/32 only */
	g_volume.ready = (g_volume.fat_type != FAT_TYPE_12);
	return 1;
}

/* Identify a non-FAT filesystem starting at 'start'; fills detected_name. */
static int identify_other_filesystem(uint32_t start) {
	if (!storage_read_sectors(start, 1, g_sector)) return 0;
	if (g_sector[3]=='N' && g_sector[4]=='T' && g_sector[5]=='F' && g_sector[6]=='S') {
		copy_name(g_volume.detected_name, "NTFS (read not supported)", 32);
		return 1;
	}
	if (g_sector[3]=='E' && g_sector[4]=='X' && g_sector[5]=='F' &&
	    g_sector[6]=='A' && g_sector[7]=='T') {
		copy_name(g_volume.detected_name, "exFAT (read not supported)", 32);
		return 1;
	}
	/* ext2/3/4 superblock: 1024 bytes in, magic 0xEF53 at offset 56 */
	if (storage_read_sectors(start + 2, 1, g_sector) &&
	    read_u16(g_sector + 56) == 0xEF53) {
		copy_name(g_volume.detected_name, "ext2/3/4 (read not supported)", 32);
		return 1;
	}
	return 0;
}

static void fat_probe(void) {
	if (g_volume.probed) return;
	g_volume.probed = 1;
	g_volume.ready = 0;
	g_volume.fat_type = FAT_TYPE_NONE;
	copy_name(g_volume.detected_name, "no filesystem recognized", 32);

	if (!storage_present()) {
		copy_name(g_volume.detected_name, "no disk", 32);
		return;
	}
	if (!storage_read_sectors(0, 1, g_sector)) return;

	/* Whole-disk (superfloppy) FAT volume? */
	if (parse_fat_boot_sector(g_sector, 0)) return;
	if (identify_other_filesystem(0)) return;

	/* MBR partition table: try the first non-empty partition */
	if (g_sector[510] == 0x55 && g_sector[511] == 0xAA) {
		for (int p = 0; p < 4; p++) {
			const uint8_t* e = g_sector + 446 + p * 16;
			uint8_t type = e[4];
			uint32_t start = read_u32(e + 8);
			if (type == 0 || start == 0) continue;
			uint8_t partition_sector[STORAGE_SECTOR_BYTES];
			if (!storage_read_sectors(start, 1, partition_sector)) continue;
			if (parse_fat_boot_sector(partition_sector, start)) return;
			if (identify_other_filesystem(start)) return;
		}
	}
}

int fat_volume_ready(void) {
	fat_probe();
	return g_volume.ready;
}

/* ── FAT table access (one-sector cache) ──────────────────────────────── */

static int fat_load_sector(uint32_t fat_relative_sector) {
	uint32_t absolute = g_volume.volume_start + g_volume.fat_start + fat_relative_sector;
	if (g_fat_sector_loaded == absolute) return 1;
	if (!storage_read_sectors(absolute, 1, g_fat_sector)) return 0;
	g_fat_sector_loaded = absolute;
	return 1;
}

static int fat_store_sector(uint32_t fat_relative_sector) {
	/* Write the cached sector back to every FAT copy */
	for (uint32_t f = 0; f < g_volume.fat_count; f++) {
		uint32_t absolute = g_volume.volume_start + g_volume.fat_start +
		                    f * g_volume.fat_sectors + fat_relative_sector;
		if (!storage_write_sectors(absolute, 1, g_fat_sector)) return 0;
	}
	return 1;
}

static uint32_t fat_end_of_chain(void) {
	return (g_volume.fat_type == FAT_TYPE_32) ? 0x0FFFFFF8 : 0xFFF8;
}

static int fat_get_entry(uint32_t cluster, uint32_t* out_value) {
	uint32_t offset = (g_volume.fat_type == FAT_TYPE_32) ? cluster * 4 : cluster * 2;
	if (!fat_load_sector(offset / 512)) return 0;
	if (g_volume.fat_type == FAT_TYPE_32)
		*out_value = read_u32(g_fat_sector + (offset % 512)) & 0x0FFFFFFF;
	else
		*out_value = read_u16(g_fat_sector + (offset % 512));
	return 1;
}

static int fat_set_entry(uint32_t cluster, uint32_t value) {
	uint32_t offset = (g_volume.fat_type == FAT_TYPE_32) ? cluster * 4 : cluster * 2;
	if (!fat_load_sector(offset / 512)) return 0;
	if (g_volume.fat_type == FAT_TYPE_32) {
		uint32_t old = read_u32(g_fat_sector + (offset % 512)) & 0xF0000000;
		write_u32(g_fat_sector + (offset % 512), old | (value & 0x0FFFFFFF));
	} else {
		write_u16(g_fat_sector + (offset % 512), (uint16_t)value);
	}
	int ok = fat_store_sector(offset / 512);
	return ok;
}

static int fat_is_end(uint32_t value) {
	return (g_volume.fat_type == FAT_TYPE_32) ? (value >= 0x0FFFFFF8) : (value >= 0xFFF8);
}

static uint32_t cluster_first_sector(uint32_t cluster) {
	return g_volume.volume_start + g_volume.data_start +
	       (cluster - 2) * g_volume.sectors_per_cluster;
}

static int fat_allocate_cluster(uint32_t* out_cluster) {
	for (uint32_t c = 2; c < g_volume.cluster_count + 2; c++) {
		uint32_t value;
		if (!fat_get_entry(c, &value)) return 0;
		if (value == 0) { *out_cluster = c; return 1; }
	}
	return 0; /* volume full */
}

/* ── Root directory iteration ─────────────────────────────────────────── */

/* Iterate root directory sectors. 'state' starts at 0; returns the absolute
 * sector to process, or 0 when the directory ends. */
static uint32_t root_next_sector(uint32_t* state, uint32_t* fat32_cluster) {
	if (g_volume.fat_type == FAT_TYPE_32) {
		uint32_t within = *state % g_volume.sectors_per_cluster;
		if (*state == 0) *fat32_cluster = g_volume.root_cluster;
		else if (within == 0) {
			uint32_t next;
			if (!fat_get_entry(*fat32_cluster, &next) || fat_is_end(next) || next < 2) return 0;
			*fat32_cluster = next;
		}
		uint32_t sector = cluster_first_sector(*fat32_cluster) + within;
		(*state)++;
		return sector;
	}
	if (*state >= g_volume.root_sectors) return 0;
	uint32_t sector = g_volume.volume_start + g_volume.root_start + *state;
	(*state)++;
	return sector;
}

/* 8.3 directory-entry name -> "name.ext" (lowercase) */
static void entry_to_name(const uint8_t* entry, char* out) {
	int n = 0;
	for (int i = 0; i < 8 && entry[i] != ' '; i++) {
		char c = (char)entry[i];
		out[n++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
	}
	if (entry[8] != ' ') {
		out[n++] = '.';
		for (int i = 8; i < 11 && entry[i] != ' '; i++) {
			char c = (char)entry[i];
			out[n++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
		}
	}
	out[n] = '\0';
}

/* "name.ext" -> 11-byte padded uppercase 8.3 field. 1 on success. */
static int name_to_entry(const char* name, uint8_t* out) {
	for (int i = 0; i < 11; i++) out[i] = ' ';
	int n = 0;
	for (; name[n] && name[n] != '.'; n++) {
		if (n >= 8) return 0;
		char c = name[n];
		out[n] = (uint8_t)((c >= 'a' && c <= 'z') ? c - 32 : c);
	}
	if (name[n] == '.') {
		n++;
		for (int e = 0; name[n + e]; e++) {
			if (e >= 3) return 0;
			char c = name[n + e];
			out[8 + e] = (uint8_t)((c >= 'a' && c <= 'z') ? c - 32 : c);
		}
	}
	return out[0] != ' ';
}

static int entry_is_file(const uint8_t* e) {
	if (e[0] == 0x00 || e[0] == 0xE5) return 0;          /* end / deleted */
	uint8_t attr = e[11];
	if ((attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) return 0;
	if (attr & (ATTR_VOLUME_LABEL | ATTR_DIRECTORY)) return 0;
	return 1;
}

int fat_list_root_directory(fat_list_callback callback, void* user) {
	if (!fat_volume_ready()) return -1;
	int count = 0;
	uint32_t state = 0, chain = 0;
	uint32_t sector;
	while ((sector = root_next_sector(&state, &chain)) != 0) {
		if (!storage_read_sectors(sector, 1, g_sector)) return count;
		for (int i = 0; i < 512; i += DIR_ENTRY_BYTES) {
			const uint8_t* e = g_sector + i;
			if (e[0] == 0x00) return count; /* end of directory */
			if (!entry_is_file(e)) continue;
			char name[16];
			entry_to_name(e, name);
			callback(name, read_u32(e + 28), user);
			count++;
		}
	}
	return count;
}

/* Find a file's directory entry. Fills sector + offset + a copy of the
 * entry. Returns 1 if found. */
static int find_entry(const uint8_t* name83, uint32_t* out_sector, int* out_offset, uint8_t* out_entry) {
	uint32_t state = 0, chain = 0;
	uint32_t sector;
	while ((sector = root_next_sector(&state, &chain)) != 0) {
		if (!storage_read_sectors(sector, 1, g_sector)) return 0;
		for (int i = 0; i < 512; i += DIR_ENTRY_BYTES) {
			uint8_t* e = g_sector + i;
			if (e[0] == 0x00) return 0;
			if (!entry_is_file(e)) continue;
			int match = 1;
			for (int c = 0; c < 11; c++) if (e[c] != name83[c]) { match = 0; break; }
			if (match) {
				*out_sector = sector;
				*out_offset = i;
				for (int c = 0; c < DIR_ENTRY_BYTES; c++) out_entry[c] = e[c];
				return 1;
			}
		}
	}
	return 0;
}

int fat_read_file(const char* name, char* buffer, uint32_t buffer_max, uint32_t* out_size) {
	if (!fat_volume_ready()) return 0;
	uint8_t name83[11];
	if (!name_to_entry(name, name83)) return 0;
	uint32_t dir_sector; int dir_offset; uint8_t entry[DIR_ENTRY_BYTES];
	if (!find_entry(name83, &dir_sector, &dir_offset, entry)) return 0;

	uint32_t size = read_u32(entry + 28);
	uint32_t cluster = read_u16(entry + 26) |
	                   ((g_volume.fat_type == FAT_TYPE_32) ? ((uint32_t)read_u16(entry + 20) << 16) : 0);
	if (size > buffer_max) size = buffer_max;
	*out_size = size;

	uint32_t copied = 0;
	while (copied < size && cluster >= 2 && !fat_is_end(cluster)) {
		uint32_t base = cluster_first_sector(cluster);
		for (uint32_t s = 0; s < g_volume.sectors_per_cluster && copied < size; s++) {
			if (!storage_read_sectors(base + s, 1, g_sector)) return 0;
			uint32_t chunk = size - copied;
			if (chunk > 512) chunk = 512;
			memcpy(buffer + copied, g_sector, chunk);
			copied += chunk;
		}
		uint32_t next;
		if (!fat_get_entry(cluster, &next)) return 0;
		cluster = next;
	}
	return 1;
}

/* Release a cluster chain back to the FAT */
static void free_chain(uint32_t cluster) {
	while (cluster >= 2 && !fat_is_end(cluster)) {
		uint32_t next;
		if (!fat_get_entry(cluster, &next)) return;
		fat_set_entry(cluster, 0);
		cluster = next;
	}
}

/* Find an existing entry slot or a free one. Returns 1 with sector/offset. */
static int find_free_entry(uint32_t* out_sector, int* out_offset) {
	uint32_t state = 0, chain = 0;
	uint32_t sector;
	while ((sector = root_next_sector(&state, &chain)) != 0) {
		if (!storage_read_sectors(sector, 1, g_sector)) return 0;
		for (int i = 0; i < 512; i += DIR_ENTRY_BYTES) {
			if (g_sector[i] == 0x00 || g_sector[i] == 0xE5) {
				*out_sector = sector;
				*out_offset = i;
				return 1;
			}
		}
	}
	return 0; /* root directory full */
}

int fat_write_file(const char* name, const char* data, uint32_t size) {
	if (!fat_volume_ready()) return 0;
	uint8_t name83[11];
	if (!name_to_entry(name, name83)) return 0;

	/* Overwriting? Free the old chain and reuse the entry slot. */
	uint32_t dir_sector; int dir_offset; uint8_t entry[DIR_ENTRY_BYTES];
	if (find_entry(name83, &dir_sector, &dir_offset, entry)) {
		uint32_t old = read_u16(entry + 26) |
		               ((g_volume.fat_type == FAT_TYPE_32) ? ((uint32_t)read_u16(entry + 20) << 16) : 0);
		free_chain(old);
	} else if (!find_free_entry(&dir_sector, &dir_offset)) {
		return 0;
	}

	/* Allocate and fill the cluster chain */
	uint32_t cluster_bytes = g_volume.sectors_per_cluster * 512;
	uint32_t first_cluster = 0, previous = 0;
	uint32_t written = 0;
	while (written < size) {
		uint32_t cluster;
		if (!fat_allocate_cluster(&cluster)) { if (first_cluster) free_chain(first_cluster); return 0; }
		if (!fat_set_entry(cluster, fat_end_of_chain())) return 0;
		if (previous) { if (!fat_set_entry(previous, cluster)) return 0; }
		else first_cluster = cluster;
		previous = cluster;

		uint32_t base = cluster_first_sector(cluster);
		for (uint32_t s = 0; s < g_volume.sectors_per_cluster; s++) {
			uint32_t offset = written + s * 512;
			for (int b = 0; b < 512; b++)
				g_sector[b] = (offset + (uint32_t)b < size) ? (uint8_t)data[offset + b] : 0;
			if (!storage_write_sectors(base + s, 1, g_sector)) return 0;
		}
		written += cluster_bytes;
	}

	/* Write the directory entry */
	if (!storage_read_sectors(dir_sector, 1, g_sector)) return 0;
	uint8_t* e = g_sector + dir_offset;
	for (int i = 0; i < DIR_ENTRY_BYTES; i++) e[i] = 0;
	for (int i = 0; i < 11; i++) e[i] = name83[i];
	e[11] = ATTR_ARCHIVE;
	write_u16(e + 26, (uint16_t)(first_cluster & 0xFFFF));
	if (g_volume.fat_type == FAT_TYPE_32) write_u16(e + 20, (uint16_t)(first_cluster >> 16));
	write_u32(e + 28, size);
	return storage_write_sectors(dir_sector, 1, g_sector);
}

int fat_delete_file(const char* name) {
	if (!fat_volume_ready()) return 0;
	uint8_t name83[11];
	if (!name_to_entry(name, name83)) return 0;
	uint32_t dir_sector; int dir_offset; uint8_t entry[DIR_ENTRY_BYTES];
	if (!find_entry(name83, &dir_sector, &dir_offset, entry)) return 0;

	uint32_t cluster = read_u16(entry + 26) |
	                   ((g_volume.fat_type == FAT_TYPE_32) ? ((uint32_t)read_u16(entry + 20) << 16) : 0);
	free_chain(cluster);

	if (!storage_read_sectors(dir_sector, 1, g_sector)) return 0;
	g_sector[dir_offset] = 0xE5; /* deleted marker */
	return storage_write_sectors(dir_sector, 1, g_sector);
}

int fat_rename_file(const char* old_name, const char* new_name) {
	if (!fat_volume_ready()) return 0;
	uint8_t old83[11], new83[11];
	if (!name_to_entry(old_name, old83)) return 0;
	if (!name_to_entry(new_name, new83)) return 0;

	/* Refuse to clobber an existing file of the new name */
	uint32_t s; int o; uint8_t e[DIR_ENTRY_BYTES];
	if (find_entry(new83, &s, &o, e)) return 0;

	uint32_t dir_sector; int dir_offset; uint8_t entry[DIR_ENTRY_BYTES];
	if (!find_entry(old83, &dir_sector, &dir_offset, entry)) return 0;

	if (!storage_read_sectors(dir_sector, 1, g_sector)) return 0;
	for (int i = 0; i < 11; i++) g_sector[dir_offset + i] = new83[i];
	return storage_write_sectors(dir_sector, 1, g_sector);
}

/* ── Description for the `disk` command ───────────────────────────────── */

static void append(char* out, int max, int* at, const char* text) {
	while (*text && *at < max - 1) out[(*at)++] = *text++;
	out[*at] = '\0';
}

static void append_number(char* out, int max, int* at, uint32_t value) {
	char digits[12]; int n = 0;
	if (value == 0) digits[n++] = '0';
	while (value) { digits[n++] = (char)('0' + value % 10); value /= 10; }
	while (n) { char c[2] = { digits[--n], 0 }; append(out, max, at, c); }
}

int fat_volume_description(char* out, int out_max) {
	fat_probe();
	int at = 0;
	out[0] = '\0';
	if (!storage_present()) {
		append(out, out_max, &at, "No storage device detected.");
		return 0;
	}
	append(out, out_max, &at, "Disk: ");
	append(out, out_max, &at, storage_model());
	append(out, out_max, &at, "\nCapacity: ");
	append_number(out, out_max, &at, storage_sector_count() / 2048);
	append(out, out_max, &at, " MB (");
	append_number(out, out_max, &at, storage_sector_count());
	append(out, out_max, &at, " sectors)\nFilesystem: ");
	append(out, out_max, &at, g_volume.detected_name);
	if (g_volume.ready) {
		append(out, out_max, &at, "\nStatus: mounted, read/write");
	} else {
		append(out, out_max, &at, "\nStatus: not operable");
	}
	return g_volume.ready;
}
