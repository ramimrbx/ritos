#ifndef MODERN_GUI_IPC_HPP
#define MODERN_GUI_IPC_HPP

#include <stdint.h>

namespace modern_gui {

enum WMMessageType {
	WM_REGISTER_WINDOW,    /* App -> WM: Register new window */
	WM_WINDOW_CREATED,     /* WM -> App: Window registered with buffer */
	WM_UNREGISTER_WINDOW,  /* App -> WM: Close window */
	WM_REDRAW_RECT,        /* App -> WM: Mark region as dirty/changed */
	WM_MOUSE_DOWN,         /* WM -> App: Mouse button pressed */
	WM_MOUSE_UP,           /* WM -> App: Mouse button released */
	WM_MOUSE_MOVE,         /* WM -> App: Mouse moved */
	WM_KEY_DOWN,           /* WM -> App: Key pressed */
	WM_FOCUS_CHANGED       /* WM -> App: Window focus state changed */
};

struct WMMessage {
	uint32_t type;         /* WMMessageType */
	uint32_t window_id;    /* Target window identifier */
	
	union {
		struct {
			int width;
			int height;
			uint32_t buffer_phys_addr; /* Mapped shared memory address */
		} reg;

		struct {
			int x;
			int y;
			int w;
			int h;
		} rect;

		struct {
			int x;
			int y;
			uint8_t buttons;
		} mouse;

		struct {
			char key;
			uint32_t flags;
		} key;

		struct {
			int focused;
		} focus;
	} data;
};

/* Mock representation of the Window Manager event loop */
class WindowManagerChannel {
public:
	/* App calls this to send a message to the WM */
	static void send_to_wm(const WMMessage& msg) {
		// In a real OS, this would trigger a kernel software interrupt (e.g. int 0x80)
		// Or write to a message queue or socket bound to the Window Server daemon.
		(void)msg;
	}

	/* App calls this to poll events from the WM queue */
	static bool receive_event(WMMessage* out_msg) {
		// Pulls event from the app's private message queue populated by the kernel/WM.
		(void)out_msg;
		return false;
	}
};

} // namespace modern_gui

#endif /* MODERN_GUI_IPC_HPP */
