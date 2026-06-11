#ifndef RIT_RBX_FORMAT_H
#define RIT_RBX_FORMAT_H

/*
 * RBX - RitOS's native executable format.
 *
 * An .rbx file is the executable the OS actually understands (RitOS's
 * equivalent of Windows' .exe). It is fully self-describing: a 64-byte
 * header followed by the raw memory image. The loader never sees ELF or
 * any other host format; tools/elf2rbx.cpp is just the packaging step that
 * turns the linker's output into this format at build time.
 *
 * Layout:
 *   [rbx_header_t : 64 bytes][code+data image : code_size bytes]
 * The loader places the image at load_addr, zeroes bss_size bytes after it,
 * and jumps to entry_point. This header is shared verbatim between the
 * host-side packer and the in-OS loader so the two can never disagree.
 */

#include <stdint.h>

#define RBX_MAGIC        "RBX2"
#define RBX_VERSION      2

/* Target CPU the image was built for (loader refuses foreign binaries) */
#define RBX_ARCH_X86     1
#define RBX_ARCH_X86_64  2
#define RBX_ARCH_ARM32   3
#define RBX_ARCH_ARM64   4

/* How entry_point expects to be called */
#define RBX_TYPE_MODULE  1  /* rbx_module* entry(const RitOS_API*, const Desktop_Interface*) */
#define RBX_TYPE_PROGRAM 2  /* void entry(const RitOS_API*) - shells/standalone programs */

/* Embedded icon: 32x32 ARGB pixels appended after the program image.
 * Shortcuts (.stct) and launchers read it straight from the .rbx, so an
 * app carries its own icon wherever the file goes. */
#define RBX_ICON_DIM    32
#define RBX_ICON_BYTES  (RBX_ICON_DIM * RBX_ICON_DIM * 4)

typedef struct rbx_header {
	char     magic[4];      /* RBX_MAGIC */
	uint16_t version;       /* RBX_VERSION */
	uint16_t arch;          /* RBX_ARCH_* */
	uint16_t type;          /* RBX_TYPE_* */
	uint16_t header_size;   /* sizeof(rbx_header_t) == 64 */
	uint32_t flags;         /* reserved, 0 */
	uint32_t load_addr;     /* where the image must be placed */
	uint32_t entry_point;   /* absolute address to call */
	uint32_t code_size;     /* bytes of image following the header */
	uint32_t bss_size;      /* bytes to zero after the image */
	uint32_t checksum;      /* 32-bit byte sum of the image */
	uint32_t icon_offset;   /* file offset of embedded icon, 0 = no icon */
	uint32_t icon_size;     /* RBX_ICON_BYTES, or 0 if no icon */
	char     name[20];      /* NUL-terminated display name */
} __attribute__((packed)) rbx_header_t;

#ifdef __cplusplus
static_assert(sizeof(rbx_header_t) == 64, "rbx_header_t must be 64 bytes");
#else
_Static_assert(sizeof(rbx_header_t) == 64, "rbx_header_t must be 64 bytes");
#endif

static inline uint32_t rbx_checksum(const uint8_t* data, uint32_t len) {
	uint32_t sum = 0;
	for (uint32_t i = 0; i < len; i++) sum += data[i];
	return sum;
}

#endif
