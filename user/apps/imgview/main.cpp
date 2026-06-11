#include <ritos/window.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>

extern const RitOS_API* g_api;

namespace ritos {

// Image data: 44 chars wide x 14 rows each
struct ImageData {
    const char*    name;
    const char*    rows[14];
    const uint8_t* colors[14];  // per-row color (single fg, Black bg)
};

// ---- RitOS Logo image: spells "RitOS" in block chars ----
static const char IMG_LOGO_R0[]  = "                                            ";
static const char IMG_LOGO_R1[]  = " \xDB\xDB\xDB\xDB  \xDB  \xDB  \xDB\xDB\xDB  \xDB\xDB\xDB   \xDB\xDB\xDB  ";
static const char IMG_LOGO_R2[]  = " \xDB  \xDB  \xDB  \xDB  \xDB    \xDB  \xDB \xDB   \xDB ";
static const char IMG_LOGO_R3[]  = " \xDB\xDB\xDB\xDB  \xDB  \xDB   \xDB   \xDB  \xDB \xDB\xDB\xDB\xDB ";
static const char IMG_LOGO_R4[]  = " \xDB  \xDB  \xDB  \xDB  \xDB    \xDB  \xDB \xDB      ";
static const char IMG_LOGO_R5[]  = " \xDB  \xDB  \xDB\xDB\xDB  \xDB\xDB\xDB  \xDB\xDB\xDB   \xDB\xDB\xDB  ";
static const char IMG_LOGO_R6[]  = "                                            ";
static const char IMG_LOGO_R7[]  = "   \xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD   ";
static const char IMG_LOGO_R8[]  = "   RitOS - Real-time Interactive OS         ";
static const char IMG_LOGO_R9[]  = "   Version 1.1.0  |  x86 VGA Text Mode      ";
static const char IMG_LOGO_R10[] = "   \xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD   ";
static const char IMG_LOGO_R11[] = "                                            ";
static const char IMG_LOGO_R12[] = "   \xFE  Bare-metal  \xFE  Multi-app  \xFE  VFS      ";
static const char IMG_LOGO_R13[] = "                                            ";

static const uint8_t IMG_LOGO_C0  = 0;
static const uint8_t IMG_LOGO_C1  = 11;
static const uint8_t IMG_LOGO_C2  = 10;
static const uint8_t IMG_LOGO_C3  = 11;
static const uint8_t IMG_LOGO_C4  = 10;
static const uint8_t IMG_LOGO_C5  = 11;
static const uint8_t IMG_LOGO_C6  = 0;
static const uint8_t IMG_LOGO_C7  = 8;
static const uint8_t IMG_LOGO_C8  = 7;
static const uint8_t IMG_LOGO_C9  = 7;
static const uint8_t IMG_LOGO_C10 = 8;
static const uint8_t IMG_LOGO_C11 = 0;
static const uint8_t IMG_LOGO_C12 = 14;
static const uint8_t IMG_LOGO_C13 = 0;

// ---- Landscape image ----
static const char IMG_LAND_R0[]  = "                                            ";
static const char IMG_LAND_R1[]  = " \xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0 ";
static const char IMG_LAND_R2[]  = " \xB0  \x0F   Sun  \xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0 ";
static const char IMG_LAND_R3[]  = " \xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0\xB0 ";
static const char IMG_LAND_R4[]  = "                                            ";
static const char IMG_LAND_R5[]  = "          /\\                /\\              ";
static const char IMG_LAND_R6[]  = "         /  \\    /\\        /  \\  /\\         ";
static const char IMG_LAND_R7[]  = "        /    \\  /  \\      /    \\/  \\        ";
static const char IMG_LAND_R8[]  = "       /      \\/    \\    /            \\     ";
static const char IMG_LAND_R9[]  = "      /              \\  /              \\    ";
static const char IMG_LAND_R10[] = " \xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2\xB2 ";
static const char IMG_LAND_R11[] = " \xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB ";
static const char IMG_LAND_R12[] = "                                            ";
static const char IMG_LAND_R13[] = "   Mountains  \xC4\xC4  Landscape  \xC4\xC4  Scene     ";

static const uint8_t IMG_LAND_C0  = 0;
static const uint8_t IMG_LAND_C1  = 9;
static const uint8_t IMG_LAND_C2  = 14;
static const uint8_t IMG_LAND_C3  = 9;
static const uint8_t IMG_LAND_C4  = 0;
static const uint8_t IMG_LAND_C5  = 8;
static const uint8_t IMG_LAND_C6  = 8;
static const uint8_t IMG_LAND_C7  = 8;
static const uint8_t IMG_LAND_C8  = 8;
static const uint8_t IMG_LAND_C9  = 8;
static const uint8_t IMG_LAND_C10 = 2;
static const uint8_t IMG_LAND_C11 = 2;
static const uint8_t IMG_LAND_C12 = 0;
static const uint8_t IMG_LAND_C13 = 7;

// ---- Pattern image ----
static const char IMG_PAT_R0[]  = "                                            ";
static const char IMG_PAT_R1[]  = " \xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE ";
static const char IMG_PAT_R2[]  = " \xBA  \xFA  \xBA  \xFA  \xBA  \xFA  \xBA  \xFA  \xBA  \xFA  \xBA  \xFA  \xBA  \xFA  \xBA ";
static const char IMG_PAT_R3[]  = " \xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA ";
static const char IMG_PAT_R4[]  = " \xBA  \xB1  \xBA  \xB1  \xBA  \xB1  \xBA  \xB1  \xBA  \xB1  \xBA  \xB1  \xBA  \xB1  \xBA ";
static const char IMG_PAT_R5[]  = " \xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE ";
static const char IMG_PAT_R6[]  = " \xBA  \xB2  \xBA  \xB2  \xBA  \xB2  \xBA  \xB2  \xBA  \xB2  \xBA  \xB2  \xBA  \xB2  \xBA ";
static const char IMG_PAT_R7[]  = " \xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA\xCD\xCD\xCD\xBA ";
static const char IMG_PAT_R8[]  = " \xBA  \xDB  \xBA  \xDB  \xBA  \xDB  \xBA  \xDB  \xBA  \xDB  \xBA  \xDB  \xBA  \xDB  \xBA ";
static const char IMG_PAT_R9[]  = " \xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCE ";
static const char IMG_PAT_R10[] = "                                            ";
static const char IMG_PAT_R11[] = "                                            ";
static const char IMG_PAT_R12[] = "                                            ";
static const char IMG_PAT_R13[] = "        Circuit Pattern / Grid Art          ";

static const uint8_t IMG_PAT_C0  = 0;
static const uint8_t IMG_PAT_C1  = 11;
static const uint8_t IMG_PAT_C2  = 8;
static const uint8_t IMG_PAT_C3  = 11;
static const uint8_t IMG_PAT_C4  = 8;
static const uint8_t IMG_PAT_C5  = 11;
static const uint8_t IMG_PAT_C6  = 10;
static const uint8_t IMG_PAT_C7  = 11;
static const uint8_t IMG_PAT_C8  = 14;
static const uint8_t IMG_PAT_C9  = 11;
static const uint8_t IMG_PAT_C10 = 0;
static const uint8_t IMG_PAT_C11 = 0;
static const uint8_t IMG_PAT_C12 = 0;
static const uint8_t IMG_PAT_C13 = 7;

// ---- Image table ----
static const ImageData g_images[3] = {
    {
        "RitOS Logo",
        { IMG_LOGO_R0, IMG_LOGO_R1, IMG_LOGO_R2, IMG_LOGO_R3, IMG_LOGO_R4, IMG_LOGO_R5,
          IMG_LOGO_R6, IMG_LOGO_R7, IMG_LOGO_R8, IMG_LOGO_R9, IMG_LOGO_R10, IMG_LOGO_R11,
          IMG_LOGO_R12, IMG_LOGO_R13 },
        { &IMG_LOGO_C0, &IMG_LOGO_C1, &IMG_LOGO_C2, &IMG_LOGO_C3, &IMG_LOGO_C4, &IMG_LOGO_C5,
          &IMG_LOGO_C6, &IMG_LOGO_C7, &IMG_LOGO_C8, &IMG_LOGO_C9, &IMG_LOGO_C10, &IMG_LOGO_C11,
          &IMG_LOGO_C12, &IMG_LOGO_C13 }
    },
    {
        "Landscape",
        { IMG_LAND_R0, IMG_LAND_R1, IMG_LAND_R2, IMG_LAND_R3, IMG_LAND_R4, IMG_LAND_R5,
          IMG_LAND_R6, IMG_LAND_R7, IMG_LAND_R8, IMG_LAND_R9, IMG_LAND_R10, IMG_LAND_R11,
          IMG_LAND_R12, IMG_LAND_R13 },
        { &IMG_LAND_C0, &IMG_LAND_C1, &IMG_LAND_C2, &IMG_LAND_C3, &IMG_LAND_C4, &IMG_LAND_C5,
          &IMG_LAND_C6, &IMG_LAND_C7, &IMG_LAND_C8, &IMG_LAND_C9, &IMG_LAND_C10, &IMG_LAND_C11,
          &IMG_LAND_C12, &IMG_LAND_C13 }
    },
    {
        "Pattern",
        { IMG_PAT_R0, IMG_PAT_R1, IMG_PAT_R2, IMG_PAT_R3, IMG_PAT_R4, IMG_PAT_R5,
          IMG_PAT_R6, IMG_PAT_R7, IMG_PAT_R8, IMG_PAT_R9, IMG_PAT_R10, IMG_PAT_R11,
          IMG_PAT_R12, IMG_PAT_R13 },
        { &IMG_PAT_C0, &IMG_PAT_C1, &IMG_PAT_C2, &IMG_PAT_C3, &IMG_PAT_C4, &IMG_PAT_C5,
          &IMG_PAT_C6, &IMG_PAT_C7, &IMG_PAT_C8, &IMG_PAT_C9, &IMG_PAT_C10, &IMG_PAT_C11,
          &IMG_PAT_C12, &IMG_PAT_C13 }
    }
};

class ImageViewerWindow : public Window {
private:
    int m_image_index;

    static int str_len(const char* s) {
        int n = 0;
        while (s[n]) n++;
        return n;
    }

public:
    ImageViewerWindow(int x, int y)
        : Window("Image Viewer", x, y, 50, 20), m_image_index(0) {
        m_body_color  = rit::Color::Black;
        m_title_color = rit::Color::LightCyan;
        m_border_color = rit::Color::LightCyan;
    }

    void draw() override {
        if (!m_visible || m_minimized) return;
        Window::draw();

        const ImageData& img = g_images[m_image_index];

        // Navigation bar at row m_y+1
        int bar_y = m_y + 1;
        int bar_x = m_x + 1;
        // Clear nav bar
        for (int i = 0; i < m_width - 2; i++)
            g_api->draw_char(' ', (uint8_t)rit::Color::LightGrey, (uint8_t)rit::Color::Black, bar_x + i, bar_y);

        // [< Prev]
        const char* prev_lbl = "\x11 Prev";
        int pi = 0;
        while (prev_lbl[pi]) {
            g_api->draw_char(prev_lbl[pi], (uint8_t)rit::Color::LightCyan, (uint8_t)rit::Color::Black, bar_x + pi, bar_y);
            pi++;
        }

        // Centered image name
        int name_len = str_len(img.name);
        int center_x = bar_x + (m_width - 2 - name_len) / 2;
        for (int i = 0; img.name[i]; i++) {
            g_api->draw_char(img.name[i], (uint8_t)rit::Color::White, (uint8_t)rit::Color::Black, center_x + i, bar_y);
        }

        // [Next >]
        const char* next_lbl = "Next \x10";
        int nl = str_len(next_lbl);
        int nx = bar_x + m_width - 2 - nl;
        int ni = 0;
        while (next_lbl[ni]) {
            g_api->draw_char(next_lbl[ni], (uint8_t)rit::Color::LightCyan, (uint8_t)rit::Color::Black, nx + ni, bar_y);
            ni++;
        }

        // Draw image rows
        for (int row = 0; row < 14; row++) {
            int draw_y = m_y + 2 + row;
            if (draw_y >= m_y + m_height - 1) break;
            const char* line = img.rows[row];
            uint8_t color = *img.colors[row];
            int col = 0;
            while (line[col] && col < m_width - 2) {
                g_api->draw_char(line[col], color, (uint8_t)rit::Color::Black, bar_x + col, draw_y);
                col++;
            }
            // Fill rest with spaces
            for (; col < m_width - 2; col++) {
                g_api->draw_char(' ', color, (uint8_t)rit::Color::Black, bar_x + col, draw_y);
            }
        }

        // Bottom status bar
        int stat_y = m_y + m_height - 2;
        if (stat_y > m_y + 1 && stat_y < m_y + m_height) {
            for (int i = 0; i < m_width - 2; i++)
                g_api->draw_char(' ', (uint8_t)rit::Color::DarkGrey, (uint8_t)rit::Color::Black, bar_x + i, stat_y);
            // Show image index
            char idx_str[32];
            idx_str[0] = 'I'; idx_str[1] = 'm'; idx_str[2] = 'g'; idx_str[3] = ' ';
            idx_str[4] = '0' + (m_image_index + 1);
            idx_str[5] = '/';
            idx_str[6] = '3';
            idx_str[7] = ' ';
            idx_str[8] = '\xC4';
            idx_str[9] = ' ';
            idx_str[10] = 'L';
            idx_str[11] = 'e';
            idx_str[12] = 'f';
            idx_str[13] = 't';
            idx_str[14] = '/';
            idx_str[15] = 'R';
            idx_str[16] = 'i';
            idx_str[17] = 'g';
            idx_str[18] = 'h';
            idx_str[19] = 't';
            idx_str[20] = ':';
            idx_str[21] = ' ';
            idx_str[22] = 'n';
            idx_str[23] = 'a';
            idx_str[24] = 'v';
            idx_str[25] = 'i';
            idx_str[26] = 'g';
            idx_str[27] = 'a';
            idx_str[28] = 't';
            idx_str[29] = 'e';
            idx_str[30] = '\0';
            for (int i = 0; idx_str[i]; i++) {
                g_api->draw_char(idx_str[i], (uint8_t)rit::Color::LightGrey, (uint8_t)rit::Color::Black, bar_x + i, stat_y);
            }
        }
    }

    void handle_key(char key) override {
        if (key == 'n' || key == 'd' || key == '\x4D') {  // next
            m_image_index = (m_image_index + 1) % 3;
        } else if (key == 'p' || key == 'a' || key == '\x4B') {  // prev
            m_image_index = (m_image_index + 2) % 3;
        }
    }

    void handle_click(int mx, int my) override {
        int bar_y = m_y + 1;
        if (my == bar_y) {
            if (mx < m_x + 8) {
                m_image_index = (m_image_index + 2) % 3;  // prev
            } else if (mx > m_x + m_width - 8) {
                m_image_index = (m_image_index + 1) % 3;  // next
            }
        }
    }
};

} // namespace ritos

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
    g_api = api;
    (void)desktop;

    static ritos::ImageViewerWindow* viewer = nullptr;
    if (!viewer) {
        viewer = new ritos::ImageViewerWindow(15, 3);
    }

    static rbx_module mod;
    mod.type = RBX_MODULE_APP_WINDOW;
    const char* nm = "Image Viewer";
    int i = 0;
    while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
    mod.name[i] = '\0';
    mod.instance = viewer;

    mod.draw = [](void* inst) { static_cast<ritos::ImageViewerWindow*>(inst)->draw(); };
    mod.handle_click = [](void* inst, int mx, int my) { static_cast<ritos::ImageViewerWindow*>(inst)->handle_click(mx, my); };
    mod.handle_key = [](void* inst, char key) { static_cast<ritos::ImageViewerWindow*>(inst)->handle_key(key); };
    mod.destroy = [](void* inst) { delete static_cast<ritos::ImageViewerWindow*>(inst); };

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
    mod.is_maximized = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_maximized() ? 1 : 0; };
    mod.set_maximized = [](void* inst, int maximized) { static_cast<ritos::Window*>(inst)->set_maximized(maximized != 0); };

    return &mod;
}
