#ifndef RITOS_DESKTOP_HPP
#define RITOS_DESKTOP_HPP

#include "window.hpp"

namespace ritos {

class Desktop : public rit::Object {
private:
	rit::Color m_bg_color;
	rit::Color m_bar_color;
	char m_bg_pattern;
	Window* m_windows[10];
	int m_window_count;

	// Mouse and drag state tracking
	int m_mouse_x;
	int m_mouse_y;
	uint8_t m_mouse_buttons;
	Window* m_dragged_window;
	int m_drag_offset_x;
	int m_drag_offset_y;
	int m_focus_index; // 0: None, 1: Shutdown, 2: Reboot
	bool m_start_menu_open;

	void handle_mouse_down(int x, int y);
	void handle_right_mouse_down(int x, int y);
	void handle_mouse_move(int x, int y);
	void handle_mouse_up();

public:
	Desktop();
	~Desktop() override;

	void add_window(Window* win);
	void draw();
	void update_mouse(int x, int y, uint8_t buttons);
	void draw_cursor();
	
	// Keyboard Navigation APIs
	void cycle_focus();
	void trigger_focused_action();

	// Keyboard routing helper
	Window* get_active_window();

	// Setters for Settings Window
	void set_bg_color(rit::Color color) { m_bg_color = color; }
	void set_bg_pattern(char pattern) { m_bg_pattern = pattern; }

	const char* get_class_name() const override {
		return "Desktop";
	}
};

} // namespace ritos

#endif
