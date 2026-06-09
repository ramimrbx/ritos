#ifndef RITOS_APPS_HPP
#define RITOS_APPS_HPP

#include "window.hpp"

namespace ritos {

// Forward declare Desktop
class Desktop;

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

public:
	CalculatorWindow(int x, int y);
	void draw() override;
	void handle_click(int mx, int my) override;
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

class FileExplorerWindow : public Window {
private:
	int m_selected_idx;
	int m_file_count;
	const char* m_file_list[16];
	TextEditorWindow* m_editor;
	bool m_rename_mode;
	char m_rename_buf[32];
	int m_rename_len;
	int m_active_pane;       // 0: Left Pane (Tree), 1: Right Pane (Files list)
	int m_left_selected_idx; // Current folder selection on left pane (0: C:\, 1: Notes)

public:
	FileExplorerWindow(int x, int y, TextEditorWindow* editor);
	void draw() override;
	void handle_click(int mx, int my) override;
	void handle_key(char key) override;
	void refresh_files();
};

class SettingsWindow : public Window {
private:
	Desktop* m_desktop;

public:
	SettingsWindow(int x, int y, Desktop* desktop);
	void draw() override;
	void handle_click(int mx, int my) override;
};

} // namespace ritos

#endif
