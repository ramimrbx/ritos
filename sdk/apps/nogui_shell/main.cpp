#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

// Global API pointer (defined in app_shim.o)
extern const RitOS_API* g_api;

// ---- Minimal string helpers (no stdlib) ----
static int my_strlen(const char* s) {
    int n = 0; while (s[n]) n++; return n;
}

static bool my_streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return false; i++; }
    return a[i] == b[i];
}

static void my_strcpy(char* dst, const char* src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void int_to_str(size_t val, char* buf, int buf_size) {
    if (buf_size <= 0) return;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[32];
    int ti = 0;
    while (val > 0 && ti < 31) { tmp[ti++] = '0' + (int)(val % 10); val /= 10; }
    int out = 0;
    for (int j = ti - 1; j >= 0 && out < buf_size - 1; j--) buf[out++] = tmp[j];
    buf[out] = '\0';
}

// ---- Colors from rit::Color enum ----
static const uint8_t COL_BLACK       = 0;
static const uint8_t COL_LIGHTGREEN  = 10;
static const uint8_t COL_LIGHTGREY   = 7;
static const uint8_t COL_DARKGREY    = 8;
static const uint8_t COL_LIGHTCYAN   = 11;
static const uint8_t COL_WHITE       = 15;
static const uint8_t COL_LIGHTRED    = 12;
static const uint8_t COL_LIGHTBLUE   = 9;

// ---- Scrollback buffer ----
static const int SCROLL_LINES  = 50;
static const int SCROLL_WIDTH  = 76;
static const int CONTENT_ROWS  = 22;  // rows 1..22 (row 0 = header, row 23 = prompt, row 24 = footer)

struct ScrollLine {
    char    text[SCROLL_WIDTH + 1];
    uint8_t color;
};

static ScrollLine g_scroll[SCROLL_LINES];
static int        g_scroll_count = 0;

static void scroll_add(const char* text, uint8_t color) {
    if (g_scroll_count < SCROLL_LINES) {
        my_strcpy(g_scroll[g_scroll_count].text, text, SCROLL_WIDTH + 1);
        g_scroll[g_scroll_count].color = color;
        g_scroll_count++;
    } else {
        // Scroll up
        for (int i = 0; i < SCROLL_LINES - 1; i++) {
            my_strcpy(g_scroll[i].text, g_scroll[i + 1].text, SCROLL_WIDTH + 1);
            g_scroll[i].color = g_scroll[i + 1].color;
        }
        my_strcpy(g_scroll[SCROLL_LINES - 1].text, text, SCROLL_WIDTH + 1);
        g_scroll[SCROLL_LINES - 1].color = color;
    }
}

// ---- Input buffer ----
static char  g_input[SCROLL_WIDTH + 1];
static int   g_input_len = 0;

// ---- Draw the full 80x25 screen ----
static void draw_screen() {
    if (!g_api) return;

    // === Row 0: Header bar ===
    for (int x = 0; x < 80; x++)
        g_api->draw_char(' ', COL_LIGHTGREEN, COL_DARKGREY, x, 0);

    // Logo symbol + title
    const char* logo_sym = "\x0F ";  // sun char
    const char* title    = "RitOS Shell v1.0";
    int cx = 1;
    for (int i = 0; logo_sym[i]; i++)
        g_api->draw_char(logo_sym[i], COL_LIGHTGREEN, COL_DARKGREY, cx++, 0);
    for (int i = 0; title[i]; i++)
        g_api->draw_char(title[i], COL_WHITE, COL_DARKGREY, cx++, 0);

    // Separator
    g_api->draw_char(' ', COL_WHITE, COL_DARKGREY, cx++, 0);
    g_api->draw_char('\xB3', COL_DARKGREY, COL_DARKGREY, cx++, 0);
    g_api->draw_char(' ', COL_WHITE, COL_DARKGREY, cx++, 0);

    // Tab hint
    const char* hint1 = "Tab: history";
    for (int i = 0; hint1[i]; i++)
        g_api->draw_char(hint1[i], COL_LIGHTCYAN, COL_DARKGREY, cx++, 0);

    g_api->draw_char(' ', COL_WHITE, COL_DARKGREY, cx++, 0);
    g_api->draw_char('\xB3', COL_DARKGREY, COL_DARKGREY, cx++, 0);
    g_api->draw_char(' ', COL_WHITE, COL_DARKGREY, cx++, 0);

    // F1 hint
    const char* hint2 = "F1: help";
    for (int i = 0; hint2[i]; i++)
        g_api->draw_char(hint2[i], COL_LIGHTCYAN, COL_DARKGREY, cx++, 0);

    // === Rows 1-22: Scrollback content ===
    // Show last CONTENT_ROWS lines from scrollback
    int start = g_scroll_count - CONTENT_ROWS;
    if (start < 0) start = 0;

    for (int row = 1; row <= CONTENT_ROWS; row++) {
        int s_idx = start + (row - 1);
        for (int x = 0; x < 80; x++)
            g_api->draw_char(' ', COL_LIGHTGREY, COL_BLACK, x, row);

        if (s_idx < g_scroll_count) {
            const char* line = g_scroll[s_idx].text;
            uint8_t col = g_scroll[s_idx].color;
            int i = 0;
            while (line[i] && i < 78) {
                g_api->draw_char(line[i], col, COL_BLACK, 1 + i, row);
                i++;
            }
        }
    }

    // === Row 23: Prompt line ===
    for (int x = 0; x < 80; x++)
        g_api->draw_char(' ', COL_LIGHTGREY, COL_BLACK, x, 23);

    g_api->draw_char('>', COL_LIGHTGREEN, COL_BLACK, 1, 23);
    g_api->draw_char(' ', COL_LIGHTGREEN, COL_BLACK, 2, 23);

    for (int i = 0; i < g_input_len; i++)
        g_api->draw_char(g_input[i], COL_WHITE, COL_BLACK, 3 + i, 23);

    // Cursor block
    g_api->draw_char('\xDB', COL_LIGHTGREEN, COL_BLACK, 3 + g_input_len, 23);

    // === Row 24: Footer bar ===
    for (int x = 0; x < 80; x++)
        g_api->draw_char(' ', COL_LIGHTGREY, COL_DARKGREY, x, 24);

    // Time on left
    int h = 0, m = 0, s = 0;
    g_api->get_time(&h, &m, &s);
    char time_str[16];
    time_str[0] = '0' + h / 10;
    time_str[1] = '0' + h % 10;
    time_str[2] = ':';
    time_str[3] = '0' + m / 10;
    time_str[4] = '0' + m % 10;
    time_str[5] = ':';
    time_str[6] = '0' + s / 10;
    time_str[7] = '0' + s % 10;
    time_str[8] = '\0';
    for (int i = 0; time_str[i]; i++)
        g_api->draw_char(time_str[i], COL_WHITE, COL_DARKGREY, 1 + i, 24);

    // Memory usage on right
    size_t heap = g_api->get_heap_usage();
    char mem_str[32];
    char num_buf[16];
    int_to_str(heap, num_buf, 16);
    // Build "Heap: XXXX B"
    int mi = 0;
    const char* mem_label = "Heap:";
    for (int i = 0; mem_label[i]; i++) mem_str[mi++] = mem_label[i];
    mem_str[mi++] = ' ';
    for (int i = 0; num_buf[i]; i++) mem_str[mi++] = num_buf[i];
    mem_str[mi++] = ' ';
    mem_str[mi++] = 'B';
    mem_str[mi] = '\0';

    int mem_x = 79 - mi;
    for (int i = 0; i < mi; i++)
        g_api->draw_char(mem_str[i], COL_LIGHTGREY, COL_DARKGREY, mem_x + i, 24);
}

// ---- Command execution ----
static void execute_command(const char* cmd) {
    // Parse command and args
    char clean_cmd[80];
    int len = 0;
    while (cmd[len] && cmd[len] != ' ' && len < 79) {
        clean_cmd[len] = cmd[len];
        len++;
    }
    clean_cmd[len] = '\0';

    const char* args = cmd + len;
    while (*args == ' ') args++;

    // Echo command
    char echo_line[80];
    int el = 0;
    echo_line[el++] = '>';
    echo_line[el++] = ' ';
    int cl = 0;
    while (cmd[cl] && el < 78) echo_line[el++] = cmd[cl++];
    echo_line[el] = '\0';
    scroll_add(echo_line, COL_WHITE);

    if (len == 0) return;

    if (my_streq(clean_cmd, "help")) {
        scroll_add("--- RitOS Shell v1.0 Commands ---", COL_LIGHTCYAN);
        scroll_add("  help       - show this help",      COL_LIGHTGREY);
        scroll_add("  ls         - list VFS files",      COL_LIGHTGREY);
        scroll_add("  cat <f>    - show file content",   COL_LIGHTGREY);
        scroll_add("  create <f> <text> - write file",   COL_LIGHTGREY);
        scroll_add("  rm <f>     - delete file",         COL_LIGHTGREY);
        scroll_add("  open <app> - launch application",  COL_LIGHTGREY);
        scroll_add("  wallpaper <name> - set wallpaper", COL_LIGHTGREY);
        scroll_add("    grid stars solid checker",        COL_LIGHTGREY);
        scroll_add("    rain wave brick network",         COL_LIGHTGREY);
        scroll_add("  theme <blue|dark|green|red>",       COL_LIGHTGREY);
        scroll_add("  sysinfo    - system information",  COL_LIGHTGREY);
        scroll_add("  clear      - clear screen",        COL_LIGHTGREY);
        scroll_add("  reboot   / shutdown",              COL_LIGHTGREY);
    } else if (my_streq(clean_cmd, "ls")) {
        const char* file_list[16];
        int count = g_api->vfs_get_file_list(file_list, 16);
        if (count <= 0) {
            scroll_add("  (no files)", COL_LIGHTGREY);
        } else {
            for (int i = 0; i < count; i++) {
                scroll_add(file_list[i], COL_LIGHTGREY);
            }
        }
    } else if (my_streq(clean_cmd, "cat")) {
        if (*args == '\0') {
            scroll_add("Usage: cat <filename>", COL_LIGHTRED);
        } else {
            char full_path[64];
            int fp_len = 0;
            if (args[0] != '/') {
                const char* sp = "/sys/";
                while (sp[fp_len]) { full_path[fp_len] = sp[fp_len]; fp_len++; }
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
                            scroll_add(line_buf, COL_LIGHTGREY);
                            lbl = 0;
                        } else {
                            line_buf[lbl++] = content[i];
                        }
                    }
                    if (lbl > 0) { line_buf[lbl] = '\0'; scroll_add(line_buf, COL_LIGHTGREY); }
                }
            } else {
                scroll_add("File not found.", COL_LIGHTRED);
            }
        }
    } else if (my_streq(clean_cmd, "create")) {
        const char* p = args;
        char filename[32];
        int fn_idx = 0;
        while (*p && *p != ' ' && fn_idx < 31) { filename[fn_idx++] = *p++; }
        filename[fn_idx] = '\0';
        while (*p == ' ') p++;
        if (fn_idx == 0 || *p == '\0') {
            scroll_add("Usage: create <filename> <content>", COL_LIGHTRED);
        } else {
            char full_path[64];
            int fp_len = 0;
            if (filename[0] != '/') {
                const char* sp = "/sys/";
                while (sp[fp_len]) { full_path[fp_len] = sp[fp_len]; fp_len++; }
            }
            int al = 0;
            while (filename[al] && fp_len < 63) { full_path[fp_len++] = filename[al++]; }
            full_path[fp_len] = '\0';
            int content_len = my_strlen(p);
            g_api->vfs_write_file(full_path, p, content_len);
            scroll_add("File created.", COL_LIGHTGREEN);
        }
    } else if (my_streq(clean_cmd, "rm")) {
        if (*args == '\0') {
            scroll_add("Usage: rm <filename>", COL_LIGHTRED);
        } else {
            char full_path[64];
            int fp_len = 0;
            if (args[0] != '/') {
                const char* sp = "/sys/";
                while (sp[fp_len]) { full_path[fp_len] = sp[fp_len]; fp_len++; }
            }
            int al = 0;
            while (args[al] && fp_len < 63) { full_path[fp_len++] = args[al++]; }
            full_path[fp_len] = '\0';
            if (g_api->vfs_exists(full_path)) {
                g_api->vfs_delete_file(full_path);
                scroll_add("File deleted.", COL_LIGHTGREEN);
            } else {
                scroll_add("File not found.", COL_LIGHTRED);
            }
        }
    } else if (my_streq(clean_cmd, "open")) {
        scroll_add("GUI not available in shell mode.", COL_LIGHTRED);
        scroll_add("Use `gui` target to restart with GUI.", COL_LIGHTGREY);
    } else if (my_streq(clean_cmd, "wallpaper")) {
        char wp_pattern = 'G';
        char wp_color   = 'B';
        if (g_api->vfs_exists("/sys/settings.cfg")) {
            int cfg_len = 0;
            const char* old_cfg = g_api->vfs_read_file("/sys/settings.cfg", &cfg_len);
            if (old_cfg && cfg_len >= 2) {
                wp_pattern = old_cfg[0];
                wp_color   = old_cfg[1];
            }
        }
        bool wp_valid = true;
        if (my_streq(args, "grid"))         wp_pattern = 'G';
        else if (my_streq(args, "stars"))   wp_pattern = '*';
        else if (my_streq(args, "solid"))   wp_pattern = 'S';
        else if (my_streq(args, "checker")) wp_pattern = 'C';
        else if (my_streq(args, "rain"))    wp_pattern = 'R';
        else if (my_streq(args, "wave"))    wp_pattern = 'W';
        else if (my_streq(args, "brick"))   wp_pattern = 'B';
        else if (my_streq(args, "network")) wp_pattern = 'N';
        else { wp_valid = false; }

        if (!wp_valid) {
            scroll_add("Unknown wallpaper. Valid names:", COL_LIGHTRED);
            scroll_add("  grid stars solid checker rain wave brick network", COL_LIGHTGREY);
        } else {
            char new_cfg[3] = { wp_pattern, wp_color, '\0' };
            g_api->vfs_write_file("/sys/settings.cfg", new_cfg, 2);
            scroll_add("Wallpaper setting saved.", COL_LIGHTGREEN);
            scroll_add("Restart with GUI to see it.", COL_LIGHTGREY);
        }
    } else if (my_streq(clean_cmd, "theme")) {
        char pattern   = 'G';
        char color_char = 'B';
        if (g_api->vfs_exists("/sys/settings.cfg")) {
            int cfg_len = 0;
            const char* old_cfg = g_api->vfs_read_file("/sys/settings.cfg", &cfg_len);
            if (old_cfg && cfg_len >= 2) {
                pattern    = old_cfg[0];
                color_char = old_cfg[1];
            }
        }
        if (my_streq(args, "blue"))  color_char = 'B';
        else if (my_streq(args, "dark"))  color_char = 'D';
        else if (my_streq(args, "green")) color_char = 'G';
        else if (my_streq(args, "red"))   color_char = 'R';
        else { scroll_add("Use: blue|dark|green|red", COL_LIGHTRED); return; }

        char new_cfg[3] = { pattern, color_char, '\0' };
        g_api->vfs_write_file("/sys/settings.cfg", new_cfg, 2);
        scroll_add("Theme saved.", COL_LIGHTGREEN);
    } else if (my_streq(clean_cmd, "sysinfo")) {
        scroll_add("--- System Information ---",  COL_LIGHTCYAN);
        scroll_add("  OS:      RitOS v1.1.0",     COL_LIGHTGREY);
        scroll_add("  Arch:    x86 32-bit",        COL_LIGHTGREY);
        scroll_add("  Mode:    NOGUI Shell",        COL_LIGHTGREY);
        scroll_add("  Display: 80x25 VGA text",    COL_LIGHTGREY);
        {
            size_t heap = g_api->get_heap_usage();
            char line[80];
            const char* prefix = "  Heap:   ";
            int idx = 0;
            while (prefix[idx]) { line[idx] = prefix[idx]; idx++; }
            char nbuf[16];
            int_to_str(heap, nbuf, 16);
            int ni = 0;
            while (nbuf[ni] && idx < 74) line[idx++] = nbuf[ni++];
            const char* suffix = " bytes";
            int si = 0;
            while (suffix[si] && idx < 78) line[idx++] = suffix[si++];
            line[idx] = '\0';
            scroll_add(line, COL_LIGHTGREY);
        }
        {
            int h = 0, m = 0, s = 0;
            g_api->get_time(&h, &m, &s);
            char line[80];
            const char* prefix = "  Time:   ";
            int idx = 0;
            while (prefix[idx]) { line[idx] = prefix[idx]; idx++; }
            line[idx++] = '0' + h / 10;
            line[idx++] = '0' + h % 10;
            line[idx++] = ':';
            line[idx++] = '0' + m / 10;
            line[idx++] = '0' + m % 10;
            line[idx++] = ':';
            line[idx++] = '0' + s / 10;
            line[idx++] = '0' + s % 10;
            line[idx] = '\0';
            scroll_add(line, COL_LIGHTGREY);
        }
        scroll_add("  VFS:     Active",           COL_LIGHTGREY);
        scroll_add("  Mouse:   PS/2 driver",      COL_LIGHTGREY);
    } else if (my_streq(clean_cmd, "clear")) {
        g_scroll_count = 0;
    } else if (my_streq(clean_cmd, "reboot")) {
        g_api->reboot();
    } else if (my_streq(clean_cmd, "shutdown")) {
        g_api->shutdown();
    } else {
        char err[80];
        const char* prefix = "Unknown command: ";
        int idx = 0;
        while (prefix[idx]) { err[idx] = prefix[idx]; idx++; }
        int ci = 0;
        while (clean_cmd[ci] && idx < 78) err[idx++] = clean_cmd[ci++];
        err[idx] = '\0';
        scroll_add(err, COL_LIGHTRED);
        scroll_add("Type 'help' for commands.", COL_LIGHTGREY);
    }
}

// ---- Entry point (like desktop's _start) ----
extern "C" void _start(const RitOS_API* api) {
    g_api = api;

    api->enable_double_buffer(1);

    // Welcome messages
    scroll_add("\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCB\xCD\xCD\xCD\xCB\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC", COL_LIGHTCYAN);
    scroll_add("  \x0F  RitOS Shell v1.0  -  NOGUI Mode", COL_LIGHTGREEN);
    scroll_add("\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC", COL_LIGHTCYAN);
    scroll_add("Type 'help' for commands.", COL_LIGHTGREY);
    scroll_add("", COL_BLACK);

    draw_screen();
    api->flush();

    int prev_second = -1;

    while (1) {
        // Poll keyboard
        char key = 0;
        if (api->poll_keyboard(&key)) {
            if (key == '\n' || key == '\r') {
                g_input[g_input_len] = '\0';
                execute_command(g_input);
                g_input_len = 0;
                g_input[0]  = '\0';
            } else if (key == '\b' || key == 8 || key == 127) {
                if (g_input_len > 0) {
                    g_input_len--;
                    g_input[g_input_len] = '\0';
                }
            } else if (key >= 32 && key <= 126) {
                if (g_input_len < SCROLL_WIDTH - 4) {
                    g_input[g_input_len++] = key;
                    g_input[g_input_len]   = '\0';
                }
            }
            draw_screen();
            api->flush();
        }

        // Periodic clock update
        int h = 0, m = 0, s = 0;
        api->get_time(&h, &m, &s);
        if (s != prev_second) {
            draw_screen();
            api->flush();
            prev_second = s;
        }

        // Throttle
        for (volatile uint32_t i = 0; i < 40000; i++) {
            __asm__ volatile("nop");
        }
    }
}
