#ifndef RIT_RBX_MODULE_H
#define RIT_RBX_MODULE_H

#include "ritos_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RBX_MODULE_DESKTOP,
	RBX_MODULE_TASKBAR,
	RBX_MODULE_STARTMENU,
	RBX_MODULE_STATUSBAR,
	RBX_MODULE_APP_WINDOW
} rbx_module_type_t;

struct rbx_module {
	rbx_module_type_t type;
	char name[32];
	void* instance;

	// Callbacks
	void (*draw)(void* instance);
	void (*handle_click)(void* instance, int mx, int my);
	void (*handle_key)(void* instance, char key);
	// Called when the window is closed; must release the instance's memory.
	void (*destroy)(void* instance);

	// App Window APIs
	int (*get_x)(void* instance);
	int (*get_y)(void* instance);
	int (*get_width)(void* instance);
	int (*get_height)(void* instance);
	void (*set_position)(void* instance, int x, int y);
	int (*is_visible)(void* instance);
	void (*set_visible)(void* instance, int visible);
	int (*is_minimized)(void* instance);
	void (*set_minimized)(void* instance, int minimized);
	int (*is_active)(void* instance);
	void (*set_active)(void* instance, int active);
	const char* (*get_title)(void* instance);
	int (*is_maximized)(void* instance);
	void (*set_maximized)(void* instance, int maximized);
};

// Interface exposed by Desktop to taskbar, startmenu, and statusbar
struct Desktop_Interface {
	int (*get_window_count)(void);
	const char* (*get_window_title)(int index);
	int (*is_window_active)(int index);
	int (*is_window_visible)(int index);
	int (*is_window_minimized)(int index);
	
	void (*toggle_window)(int index);
	void (*launch_app)(const char* title);
	
	int (*get_mouse_x)(void);
	int (*get_mouse_y)(void);
	int (*is_start_menu_open)(void);
	void (*set_start_menu_open)(int open);

	/* Pixel-precise mouse position (0..1023, 0..767) */
	int (*get_mouse_px_x)(void);
	int (*get_mouse_px_y)(void);
};

typedef struct rbx_module* (*rbx_init_t)(const struct RitOS_API* api, const struct Desktop_Interface* desktop);

#ifdef __cplusplus
}
#endif

#endif
