#ifndef RITOS_APPS_HPP
#define RITOS_APPS_HPP

#include "window.hpp"

namespace ritos {

class SystemMonitorWindow : public Window {
public:
	SystemMonitorWindow(int x, int y, int w, int h);
	void draw() override;
};

class CalculatorWindow : public Window {
private:
	int m_accumulator;
	int m_current_val;
	char m_op;
	bool m_clear_on_next;
	char m_display[16];

	void update_display();
	void press(char btn);

public:
	CalculatorWindow(int x, int y);
	void draw() override;
	void handle_click(int mx, int my) override;
	void handle_key(char key) override;
};

class TextEditorWindow : public Window {
private:
	char m_buffer[512];
	int m_buf_len;
	char m_filename[32];

public:
	TextEditorWindow(int x, int y, int w, int h);
	void draw() override;
	void handle_key(char key) override;
	void handle_click(int mx, int my) override;

	void open_file(const char* filename);
	void save_file();
	const char* get_filename() const { return m_filename; }
};

class ClockWindow : public Window {
public:
	ClockWindow(int x, int y);
	void draw() override;
};

class CalendarWindow : public Window {
public:
	CalendarWindow(int x, int y);
	void draw() override;
};

class SettingsWindow : public Window {
private:
	int m_page;   /* 0 = Personalization, 1 = System, 2 = About */

public:
	SettingsWindow(int x, int y);
	void draw() override;
	void handle_click(int mx, int my) override;
};

} // namespace ritos

#endif
