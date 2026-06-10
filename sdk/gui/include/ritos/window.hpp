#ifndef RITOS_WINDOW_HPP
#define RITOS_WINDOW_HPP

#include "../../framework/include/rit/string.hpp"
#include "../../framework/include/rit/system.hpp"

namespace ritos {

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
			set_position(0, 2);   /* below status bar */
			m_width  = 128;       /* 1024 / 8 */
			m_height = 44;        /* rows 2..45 */
		} else {
			set_position(m_orig_x, m_orig_y);
			m_width = m_orig_width;
			m_height = m_orig_height;
		}
	}

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
