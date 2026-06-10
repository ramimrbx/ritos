#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

extern const RitOS_API* g_api;

namespace ritos {

class TerminalWindow : public Window {
private:
	static const int MAX_LINES = 16;
	char m_history[MAX_LINES][80];
	int m_history_count;
	char m_input_buf[80];
	int m_input_len;
	const Desktop_Interface* m_desktop;

	void add_line(const char* text) {
		if (m_history_count < MAX_LINES) {
			int i = 0;
			while (text[i] && i < 79) {
				m_history[m_history_count][i] = text[i];
				i++;
			}
			m_history[m_history_count][i] = '\0';
			m_history_count++;
		} else {
			// Scroll up
			for (int s = 0; s < MAX_LINES - 1; s++) {
				int i = 0;
				while (m_history[s + 1][i]) {
					m_history[s][i] = m_history[s + 1][i];
					i++;
				}
				m_history[s][i] = '\0';
			}
			int i = 0;
			while (text[i] && i < 79) {
				m_history[MAX_LINES - 1][i] = text[i];
				i++;
			}
			m_history[MAX_LINES - 1][i] = '\0';
		}
	}

	void execute_command(const char* cmd) {
		// Trim command
		char clean_cmd[80];
		int len = 0;
		while (cmd[len] && cmd[len] != ' ' && len < 79) {
			clean_cmd[len] = cmd[len];
			len++;
		}
		clean_cmd[len] = '\0';

		const char* args = cmd + len;
		while (*args == ' ') args++;

		// Echo command to history
		char echo_line[90];
		int el = 0;
		echo_line[el++] = '>';
		echo_line[el++] = ' ';
		int cl = 0;
		while (cmd[cl] && el < 89) echo_line[el++] = cmd[cl++];
		echo_line[el] = '\0';
		add_line(echo_line);

		if (len == 0) return;

		auto str_equals = [](const char* s1, const char* s2) {
			int i = 0;
			while (s1[i] && s2[i]) {
				if (s1[i] != s2[i]) return false;
				i++;
			}
			return s1[i] == s2[i];
		};

		if (str_equals(clean_cmd, "help")) {
			add_line("Commands:");
			add_line("  help - list all commands");
			add_line("  ls - list VFS files");
			add_line("  cat <file> - show file content");
			add_line("  create <file> <text> - write file");
			add_line("  rm <file> - delete file");
			add_line("  calc, clock, calen, sysmon");
			add_line("  editor, files, config, imgview");
			add_line("  wallpaper <name> - set wallpaper");
			add_line("    names: picture grid stars solid");
			add_line("           checker rain wave brick network");
			add_line("  theme <blue|dark|green|red> - color");
			add_line("  bg <grid|solid|stars> - background");
			add_line("  sysinfo - system information");
			add_line("  reboot, shutdown, clear");
		} else if (str_equals(clean_cmd, "ls")) {
			const char* file_list[16];
			int count = g_api->vfs_get_file_list(file_list, 16);
			if (count <= 0) {
				add_line("No files found.");
			} else {
				for (int i = 0; i < count; i++) {
					add_line(file_list[i]);
				}
			}
		} else if (str_equals(clean_cmd, "cat")) {
			if (*args == '\0') {
				add_line("Usage: cat <filename>");
			} else {
				char full_path[64];
				int fp_len = 0;
				if (args[0] != '/') {
					const char* sys_path = "/sys/";
					while (sys_path[fp_len]) { full_path[fp_len] = sys_path[fp_len]; fp_len++; }
				}
				int al = 0;
				while (args[al] && fp_len < 63) { full_path[fp_len++] = args[al++]; }
				full_path[fp_len] = '\0';

				if (g_api->vfs_exists(full_path)) {
					int flen = 0;
					const char* content = g_api->vfs_read_file(full_path, &flen);
					if (content) {
						char line_buf[80];
						int lbl = 0;
						for (int i = 0; i < flen; i++) {
							if (content[i] == '\n' || lbl >= 78) {
								line_buf[lbl] = '\0';
								add_line(line_buf);
								lbl = 0;
							} else {
								line_buf[lbl++] = content[i];
							}
						}
						if (lbl > 0) {
							line_buf[lbl] = '\0';
							add_line(line_buf);
						}
					}
				} else {
					add_line("File not found.");
				}
			}
		} else if (str_equals(clean_cmd, "create")) {
			const char* p = args;
			char filename[32];
			int fn_idx = 0;
			while (*p && *p != ' ' && fn_idx < 31) {
				filename[fn_idx++] = *p++;
			}
			filename[fn_idx] = '\0';

			while (*p == ' ') p++;

			if (fn_idx == 0 || *p == '\0') {
				add_line("Usage: create <filename> <content>");
			} else {
				char full_path[64];
				int fp_len = 0;
				if (filename[0] != '/') {
					const char* sys_path = "/sys/";
					while (sys_path[fp_len]) { full_path[fp_len] = sys_path[fp_len]; fp_len++; }
				}
				int al = 0;
				while (filename[al] && fp_len < 63) { full_path[fp_len++] = filename[al++]; }
				full_path[fp_len] = '\0';

				int content_len = 0;
				while (p[content_len]) content_len++;

				g_api->vfs_write_file(full_path, p, content_len);
				add_line("File created successfully.");
			}
		} else if (str_equals(clean_cmd, "rm")) {
			if (*args == '\0') {
				add_line("Usage: rm <filename>");
			} else {
				char full_path[64];
				int fp_len = 0;
				if (args[0] != '/') {
					const char* sys_path = "/sys/";
					while (sys_path[fp_len]) { full_path[fp_len] = sys_path[fp_len]; fp_len++; }
				}
				int al = 0;
				while (args[al] && fp_len < 63) { full_path[fp_len++] = args[al++]; }
				full_path[fp_len] = '\0';

				if (g_api->vfs_exists(full_path)) {
					g_api->vfs_delete_file(full_path);
					add_line("File deleted.");
				} else {
					add_line("File not found.");
				}
			}
		} else if (str_equals(clean_cmd, "calc")) {
			m_desktop->launch_app("Calculator");
		} else if (str_equals(clean_cmd, "clock")) {
			m_desktop->launch_app("Clock");
		} else if (str_equals(clean_cmd, "calen")) {
			m_desktop->launch_app("Calendar");
		} else if (str_equals(clean_cmd, "sysmon")) {
			m_desktop->launch_app("System Monitor");
		} else if (str_equals(clean_cmd, "editor")) {
			m_desktop->launch_app("Text Editor");
		} else if (str_equals(clean_cmd, "files")) {
			m_desktop->launch_app("File Explorer");
		} else if (str_equals(clean_cmd, "config")) {
			m_desktop->launch_app("Settings");
		} else if (str_equals(clean_cmd, "imgview")) {
			m_desktop->launch_app("Image Viewer");
		} else if (str_equals(clean_cmd, "wallpaper")) {
			char wp_pattern = 'G';
			char wp_color = 'B';
			if (g_api->vfs_exists("/sys/settings.cfg")) {
				int cfg_len = 0;
				const char* old_cfg = g_api->vfs_read_file("/sys/settings.cfg", &cfg_len);
				if (old_cfg && cfg_len >= 2) {
					wp_pattern = old_cfg[0];
					wp_color = old_cfg[1];
				}
			}
			bool wp_valid = true;
			if (str_equals(args, "picture"))      wp_pattern = 'P';
			else if (str_equals(args, "grid"))    wp_pattern = 'G';
			else if (str_equals(args, "stars"))   wp_pattern = '*';
			else if (str_equals(args, "solid"))   wp_pattern = 'S';
			else if (str_equals(args, "checker")) wp_pattern = 'C';
			else if (str_equals(args, "rain"))    wp_pattern = 'R';
			else if (str_equals(args, "wave"))    wp_pattern = 'W';
			else if (str_equals(args, "brick"))   wp_pattern = 'B';
			else if (str_equals(args, "network")) wp_pattern = 'N';
			else { wp_valid = false; }

			if (!wp_valid) {
				add_line("Unknown wallpaper. Valid: picture grid");
				add_line("  stars solid checker rain wave");
				add_line("  brick network");
			} else {
				char new_cfg[3] = { wp_pattern, wp_color, '\0' };
				g_api->vfs_write_file("/sys/settings.cfg", new_cfg, 2);
				add_line("Wallpaper updated.");
			}
		} else if (str_equals(clean_cmd, "sysinfo")) {
			add_line("--- System Information ---");
			add_line("  OS: RitOS v1.1.0");
			add_line("  Arch: x86 32-bit");
			add_line("  Display: 80x25 VGA text");
			{
				size_t heap = g_api->get_heap_usage();
				// Format heap usage manually
				char info[80];
				int idx = 0;
				const char* prefix = "  Heap used: ";
				while (prefix[idx]) { info[idx] = prefix[idx]; idx++; }
				// Convert bytes to decimal
				if (heap == 0) {
					info[idx++] = '0';
				} else {
					char tmp[16];
					int ti = 0;
					size_t v = heap;
					while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; }
					for (int j = ti - 1; j >= 0 && idx < 78; j--) info[idx++] = tmp[j];
				}
				const char* suffix = " bytes";
				int si = 0;
				while (suffix[si] && idx < 78) { info[idx++] = suffix[si++]; }
				info[idx] = '\0';
				add_line(info);
			}
			{
				int h = 0, m = 0, s = 0;
				g_api->get_time(&h, &m, &s);
				char tstr[64];
				int idx = 0;
				const char* prefix = "  Uptime: ";
				while (prefix[idx]) { tstr[idx] = prefix[idx]; idx++; }
				// Hours
				tstr[idx++] = '0' + h / 10;
				tstr[idx++] = '0' + h % 10;
				tstr[idx++] = ':';
				tstr[idx++] = '0' + m / 10;
				tstr[idx++] = '0' + m % 10;
				tstr[idx++] = ':';
				tstr[idx++] = '0' + s / 10;
				tstr[idx++] = '0' + s % 10;
				tstr[idx] = '\0';
				add_line(tstr);
			}
			add_line("  VFS: Active");
			add_line("  Mouse: PS/2 driver");
		} else if (str_equals(clean_cmd, "theme")) {
			char pattern = 'G';
			char color_char = 'B';
			if (g_api->vfs_exists("/sys/settings.cfg")) {
				int cfg_len = 0;
				const char* old_cfg = g_api->vfs_read_file("/sys/settings.cfg", &cfg_len);
				if (old_cfg && cfg_len >= 2) {
					pattern = old_cfg[0];
					color_char = old_cfg[1];
				}
			}
			if (str_equals(args, "blue")) color_char = 'B';
			else if (str_equals(args, "dark")) color_char = 'D';
			else if (str_equals(args, "green")) color_char = 'G';
			else if (str_equals(args, "red")) color_char = 'R';
			else add_line("Unknown theme. Use blue|dark|green|red");

			char new_cfg[3] = { pattern, color_char, '\0' };
			g_api->vfs_write_file("/sys/settings.cfg", new_cfg, 2);
			add_line("Theme updated.");
		} else if (str_equals(clean_cmd, "bg")) {
			char pattern = 'G';
			char color_char = 'B';
			if (g_api->vfs_exists("/sys/settings.cfg")) {
				int cfg_len = 0;
				const char* old_cfg = g_api->vfs_read_file("/sys/settings.cfg", &cfg_len);
				if (old_cfg && cfg_len >= 2) {
					pattern = old_cfg[0];
					color_char = old_cfg[1];
				}
			}
			if (str_equals(args, "picture")) pattern = 'P';
			else if (str_equals(args, "grid")) pattern = 'G';
			else if (str_equals(args, "solid")) pattern = 'S';
			else if (str_equals(args, "stars")) pattern = '*';
			else if (str_equals(args, "checker")) pattern = 'C';
			else if (str_equals(args, "rain")) pattern = 'R';
			else if (str_equals(args, "wave")) pattern = 'W';
			else if (str_equals(args, "brick")) pattern = 'B';
			else if (str_equals(args, "network")) pattern = 'N';
			else add_line("Unknown bg. Use picture|grid|solid|stars|checker|rain|wave|brick|network");

			char new_cfg[3] = { pattern, color_char, '\0' };
			g_api->vfs_write_file("/sys/settings.cfg", new_cfg, 2);
			add_line("Background updated.");
		} else if (str_equals(clean_cmd, "reboot")) {
			g_api->reboot();
		} else if (str_equals(clean_cmd, "shutdown")) {
			g_api->shutdown();
		} else if (str_equals(clean_cmd, "clear")) {
			m_history_count = 0;
		} else {
			add_line("Unknown command. Type 'help'.");
		}
	}

public:
	TerminalWindow(int x, int y, int w, int h, const Desktop_Interface* desktop)
		: Window("Terminal", x, y, w, h), m_history_count(0), m_input_len(0), m_desktop(desktop) {
		m_body_color = rit::Color::Black;
		m_title_color = rit::Color::LightGreen;
		m_border_color = rit::Color::LightGreen;
		m_input_buf[0] = '\0';
		add_line("RitOS Shell v1.1.0");
		add_line("Type 'help' for commands.");
	}

	void draw() override {
		if (!m_visible || m_minimized) return;

		Window::draw();

		int base_x = m_x + 2;
		int base_y = m_y + 1;
		int max_h = m_height - 2;

		int history_slots = max_h - 1;
		int start_slot = 0;
		if (m_history_count > history_slots) {
			start_slot = m_history_count - history_slots;
		}

		int row = 0;
		for (int s = start_slot; s < m_history_count; s++) {
			int col = 0;
			while (m_history[s][col] != '\0' && col < m_width - 4) {
				rit::System::draw_char(m_history[s][col], rit::Color::LightGreen, rit::Color::Black, base_x + col, base_y + row);
				col++;
			}
			row++;
		}

		int col = 0;
		rit::System::draw_char('>', rit::Color::LightGreen, rit::Color::Black, base_x + col++, base_y + row);
		rit::System::draw_char(' ', rit::Color::LightGreen, rit::Color::Black, base_x + col++, base_y + row);

		for (int i = 0; i < m_input_len; i++) {
			rit::System::draw_char(m_input_buf[i], rit::Color::White, rit::Color::Black, base_x + col++, base_y + row);
		}

		rit::System::draw_char('\xDB', rit::Color::LightGreen, rit::Color::Black, base_x + col, base_y + row);
	}

	void handle_key(char key) override {
		if (key == '\n' || key == '\r') {
			m_input_buf[m_input_len] = '\0';
			execute_command(m_input_buf);
			m_input_len = 0;
			m_input_buf[0] = '\0';
		} else if (key == '\b' || key == 8 || key == 127) {
			if (m_input_len > 0) {
				m_input_len--;
				m_input_buf[m_input_len] = '\0';
			}
		} else if (key >= 32 && key <= 126) {
			if (m_input_len < m_width - 8) {
				m_input_buf[m_input_len++] = key;
				m_input_buf[m_input_len] = '\0';
			}
		}
	}
};

} // namespace ritos



extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
	g_api = api;

	static ritos::TerminalWindow* term = nullptr;
	if (!term) {
		term = new ritos::TerminalWindow(25, 6, 50, 15, desktop);
	}

	static rbx_module mod;
	mod.type = RBX_MODULE_APP_WINDOW;
	const char* nm = "Terminal";
	int i = 0;
	while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
	mod.name[i] = '\0';
	mod.instance = term;

	mod.draw = [](void* inst) { static_cast<ritos::TerminalWindow*>(inst)->draw(); };
	mod.handle_click = [](void* inst, int mx, int my) { static_cast<ritos::TerminalWindow*>(inst)->handle_click(mx, my); };
	mod.handle_key = [](void* inst, char key) { static_cast<ritos::TerminalWindow*>(inst)->handle_key(key); };

	mod.get_x = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_x(); };
	mod.get_y = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_y(); };
	mod.get_width = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_width(); };
	mod.get_height = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_height(); };
	mod.set_position = [](void* inst, int x, int y) { static_cast<ritos::Window*>(inst)->set_position(x, y); };
	mod.is_visible = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_visible() ? 1 : 0; };
	mod.set_visible = [](void* inst, int visible) { static_cast<ritos::Window*>(inst)->set_visible(visible != 0); };
	mod.is_minimized = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_minimized() ? 1 : 0; };
	mod.set_minimized = [](void* inst, int minimized) { static_cast<ritos::Window*>(inst)->set_minimized(minimized != 0); };
	mod.is_active = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_active() ? 1 : 0; };
	mod.set_active = [](void* inst, int active) { static_cast<ritos::Window*>(inst)->set_active(active != 0); };
	mod.get_title = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_title(); };

	return &mod;
}
