#ifndef RITOS_API_H
#define RITOS_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RitOS_API {
	uint32_t version;

	// Print / Screen APIs
	void (*print)(const char* text);
	void (*println)(const char* text);
	void (*clear_screen)(void);
	void (*set_color)(uint8_t fg, uint8_t bg);
	void (*draw_char)(char c, uint8_t fg, uint8_t bg, int x, int y);
	
	// Mouse Driver API
	void (*init_mouse)(void);
	int (*poll_mouse)(int* x, int* y, uint8_t* buttons);

	// Power Management API
	void (*shutdown)(void);
	void (*reboot)(void);

	// Keyboard Driver API
	int (*poll_keyboard)(char* out_char);

	// System Information API
	size_t (*get_heap_usage)(void);
	void (*get_time)(int* hour, int* min, int* sec);
	void (*get_date)(int* year, int* month, int* day);

	// VFS APIs
	int (*vfs_exists)(const char* name);
	int (*vfs_create_file)(const char* name, const char* content, int length);
	const char* (*vfs_read_file)(const char* name, int* out_length);
	int (*vfs_write_file)(const char* name, const char* content, int length);
	int (*vfs_delete_file)(const char* name);
	int (*vfs_rename_file)(const char* old_name, const char* new_name);
	int (*vfs_get_file_list)(const char* names[], int max_files);

	// Double buffering
	void (*enable_double_buffer)(int enable);
	void (*flush)(void);

	// Dynamic loader
	void* (*load_rbx)(const char* filepath);

	// Memory Allocation
	void* (*kmalloc)(size_t size);
	void (*kfree)(void* ptr);

	// App Launching
	void (*launch_app)(const char* title);
	void (*register_launch_handler)(void (*handler)(const char* title));

	// Screen Buffer Access
	uint16_t* (*get_screen_buffer)(void);

	// ── Framebuffer pixel API ───────────────────────────────────────────────
	void (*fb_fill_rect)(int x, int y, int w, int h, uint32_t argb);
	void (*fb_fill_rect_blend)(int x, int y, int w, int h, uint32_t argb);
	void (*fb_blit_argb)(const uint32_t* pixels, int x, int y, int w, int h);
	void (*fb_draw_string_px)(const char* text, uint32_t fg, uint32_t bg,
	                          int x, int y, int transparent_bg);
	void (*fb_fill_grad_v)(int x, int y, int w, int h,
	                       uint32_t top, uint32_t bot);
	void (*fb_fill_grad_h)(int x, int y, int w, int h,
	                       uint32_t left, uint32_t right);
	void (*fb_fill_rounded_rect)(int x, int y, int w, int h, int r,
	                             uint32_t argb);
	void (*fb_fill_circle)(int cx, int cy, int r, uint32_t argb);
	void (*fb_draw_hline_px)(int x, int y, int w, uint32_t argb);
	void (*fb_flush_px)(void);
	int  (*fb_get_width)(void);
	int  (*fb_get_height)(void);
	int  (*fb_is_available)(void);
};

#ifdef __cplusplus
}
#endif

#endif
