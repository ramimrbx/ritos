#ifndef RITOS_WINDOW_HPP
#define RITOS_WINDOW_HPP

#include <rit/string.hpp>
#include <rit/system.hpp>

struct Desktop_Interface;

namespace ritos {

/* Window chrome geometry shared with the desktop's hit testing */
constexpr int kTitlebarPx = 32;   /* titlebar height in pixels */
constexpr int kCaptionW   = 46;   /* caption button width      */
constexpr int kTaskbarPx  = 48;   /* shell taskbar height      */

class Window : public rit::Object {
protected:
	rit::String m_title;
	int m_x;
	int m_y;
	int m_width;
	int m_height;
	bool m_active;
	rit::Color m_title_color;
	rit::Color m_border_color;
	rit::Color m_body_color;
	rit::String m_content;
	bool m_visible;
	bool m_minimized;
	bool m_maximized;
	int m_orig_x;
	int m_orig_y;
	int m_orig_width;
	int m_orig_height;

public:
	Window(const char* title, int x, int y, int w, int h);
	~Window() override;

	virtual void draw();
	void set_content(const char* content);
	virtual void set_active(bool active);
	void move(int dx, int dy);

	bool is_maximized() const { return m_maximized; }
	virtual void set_maximized(bool maximized) {
		m_maximized = maximized;
		if (maximized) {
			m_orig_x = m_x;
			m_orig_y = m_y;
			m_orig_width = m_width;
			m_orig_height = m_height;
			set_position(0, 0);
			/* Fill the whole screen above the taskbar (cell granularity) */
			int sw = rit::System::fb_is_avail() ? rit::System::fb_get_width()  : 1024;
			int sh = rit::System::fb_is_avail() ? rit::System::fb_get_height() : 768;
			m_width  = sw / 8;
			m_height = (sh - kTaskbarPx) / 16;
		} else {
			set_position(m_orig_x, m_orig_y);
			m_width = m_orig_width;
			m_height = m_orig_height;
		}
	}

	/* Lets the chrome show caption-button hover; each app module calls
	 * this once in rbx_module_init with the interface it was handed. */
	static void set_desktop_interface(const Desktop_Interface* di);

	/* Pixel-space helpers for app content drawing */
	int px() const { return m_x * 8; }
	int py() const { return m_y * 16; }
	int pw() const { return m_width * 8; }
	int ph() const { return m_height * 16; }
	int content_y() const { return m_y * 16 + kTitlebarPx; }
	int content_h() const { return m_height * 16 - kTitlebarPx; }

	// Virtual Event Handlers
	virtual void handle_click(int mx, int my) { (void)mx; (void)my; }
	virtual void handle_right_click(int mx, int my) { (void)mx; (void)my; }
	virtual void handle_key(char key) { (void)key; }

	// Accessors and Hit Test
	const char* get_title() const { return m_title.c_str(); }
	int get_x() const { return m_x; }
	int get_y() const { return m_y; }
	int get_width() const { return m_width; }
	int get_height() const { return m_height; }
	bool is_visible() const { return m_visible; }
	virtual void set_visible(bool visible) { m_visible = visible; }
	bool is_minimized() const { return m_minimized; }
	virtual void set_minimized(bool minimized) { m_minimized = minimized; }
	bool is_active() const { return m_active; }
	virtual void set_position(int x, int y) { m_x = x; m_y = y; }
	bool is_on_header(int mx, int my) const { 
		return m_visible && !m_minimized && my == m_y && mx >= m_x && mx < m_x + m_width; 
	}

	const char* get_class_name() const override {
		return "Window";
	}
};

} // namespace ritos

#endif
