#ifndef MODERN_GUI_VBE_H
#define MODERN_GUI_VBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Multiboot 1 Information Structure graphics-related fields */
struct multiboot_info {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t syms[4];
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;
	
	/* Video info */
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;

	/* Framebuffer info (Multiboot 1 extensions) */
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t  framebuffer_bpp;
	uint8_t  framebuffer_type; /* 0 = indexed, 1 = RGB, 2 = EGA text */
};

/* VBE Mode Info Block */
struct vbe_mode_info_block {
	uint16_t attributes;
	uint8_t  window_a, window_b;
	uint16_t granularity;
	uint16_t window_size;
	uint16_t segment_a, segment_b;
	uint32_t win_func_ptr;
	uint16_t pitch;
	uint16_t width;
	uint16_t height;
	uint8_t  w_char, y_char, planes, bpp, banks;
	uint8_t  memory_model, bank_size, image_pages;
	uint8_t  reserved0;
	
	/* Color fields */
	uint8_t  red_mask, red_position;
	uint8_t  green_mask, green_position;
	uint8_t  blue_mask, blue_position;
	uint8_t  rsv_mask, rsv_position;
	uint8_t  direct_color_attributes;
	
	uint32_t physbase; /* Linear Framebuffer address */
	uint32_t reserved1;
	uint16_t reserved2;
} __attribute__((packed));

/* Setup function prototypes */
void vbe_init(struct multiboot_info* mbi, uint32_t magic);
uint32_t* vbe_get_framebuffer(void);
int vbe_get_width(void);
int vbe_get_height(void);
int vbe_get_bpp(void);

#ifdef __cplusplus
}
#endif

#endif /* MODERN_GUI_VBE_H */
