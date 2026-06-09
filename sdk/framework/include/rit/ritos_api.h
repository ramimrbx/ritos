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
};

#ifdef __cplusplus
}
#endif

#endif
