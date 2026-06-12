#include <rit/system.hpp>
#include <rit/virtual_filesystem.hpp>
#include <rit/rbx_format.h>
#include <kernel/terminal.h>
#include <kernel/mouse.h>
#include <kernel/power.h>
#include <kernel/keyboard.h>
#include <kernel/input_output.h>
#include <kernel/memory.h>
#include <kernel/framebuffer.h>
#include <kernel/fat_filesystem.h>

namespace rit {

void System::print(const char* text) {
	Terminal_writestring(g_sys_terminal, text);
}

void System::print(const String& text) {
	Terminal_writestring(g_sys_terminal, text.c_str());
}

void System::println(const char* text) {
	Terminal_writestring(g_sys_terminal, text);
	Terminal_putchar(g_sys_terminal, '\n');
}

void System::println(const String& text) {
	Terminal_writestring(g_sys_terminal, text.c_str());
	Terminal_putchar(g_sys_terminal, '\n');
}

void System::clear_screen() {
	if (fb_is_available()) {
		fb_clear_back(0xFF000000u);
		fb_flush();
	} else {
		Terminal_clear(g_sys_terminal);
	}
}

void System::set_color(Color fg, Color bg) {
	uint8_t color_byte = (uint8_t)fg | ((uint8_t)bg << 4);
	Terminal_set_color(g_sys_terminal, color_byte);
}

void System::draw_char(char c, Color fg, Color bg, int x, int y) {
	if (fb_is_available()) {
		fb_draw_char_grid(c, (uint8_t)fg, (uint8_t)bg, x, y);
	} else {
		uint8_t color_byte = (uint8_t)fg | ((uint8_t)bg << 4);
		Terminal_putentryat(g_sys_terminal, c, color_byte, x, y);
	}
}

void System::init_mouse() {
	mouse_init();
}

bool System::poll_mouse(int& x, int& y, uint8_t& buttons) {
	MouseState state;
	if (mouse_poll(&state)) {
		x = state.x;
		y = state.y;
		buttons = state.buttons;
		return true;
	}
	return false;
}

bool System::poll_mouse_px(int& x, int& y, uint8_t& buttons) {
	MouseState state;
	if (mouse_poll(&state)) {
		x = state.pixel_x;
		y = state.pixel_y;
		buttons = state.buttons;
		return true;
	}
	return false;
}

void System::shutdown() {
	sys_shutdown();
}

void System::reboot() {
	sys_reboot();
}

static uint8_t get_rtc_register(uint8_t reg) {
	outb(0x70, reg);
	return inb(0x71);
}

bool System::poll_keyboard(char& out_char) {
	return keyboard_poll(&out_char);
}

size_t System::get_heap_usage() {
	return ::get_heap_usage();
}

void System::get_time(int& hour, int& min, int& sec) {
	// Wait until RTC update is not in progress
	outb(0x70, 0x0A);
	while (inb(0x71) & 0x80) {
		// Wait
	}
	sec = get_rtc_register(0x00);
	min = get_rtc_register(0x02);
	hour = get_rtc_register(0x04);

	uint8_t registerB = get_rtc_register(0x0B);

	// Convert BCD to binary if necessary
	if (!(registerB & 4)) {
		sec = (sec & 0x0F) + ((sec / 16) * 10);
		min = (min & 0x0F) + ((min / 16) * 10);
		hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
	}

	// Convert 12-hour clock to 24-hour if necessary
	if (!(registerB & 2) && (hour & 0x80)) {
		hour = ((hour & 0x7F) + 12) % 24;
	}
}

void System::get_date(int& year, int& month, int& day) {
	// Wait until RTC update is not in progress
	outb(0x70, 0x0A);
	while (inb(0x71) & 0x80) {
		// Wait
	}
	day = get_rtc_register(0x07);
	month = get_rtc_register(0x08);
	year = get_rtc_register(0x09);

	uint8_t registerB = get_rtc_register(0x0B);

	// Convert BCD to binary if necessary
	if (!(registerB & 4)) {
		day = (day & 0x0F) + ((day / 16) * 10);
		month = (month & 0x0F) + ((month / 16) * 10);
		year = (year & 0x0F) + ((year / 16) * 10);
	}
}

void System::enable_double_buffer(bool enable) {
	if (!fb_is_available())
		Terminal_enable_double_buffer(g_sys_terminal, enable ? 1 : 0);
}

void System::flush() {
	if (fb_is_available()) fb_flush();
	else Terminal_flush(g_sys_terminal);
}

/* ── Framebuffer pixel API – kernel-side implementations ─────────────── */
void System::fb_fill_rect(int x, int y, int w, int h, uint32_t argb)
    { ::fb_fill_rect(x, y, w, h, argb); }
void System::fb_fill_rect_blend(int x, int y, int w, int h, uint32_t argb)
    { ::fb_fill_rect_blend(x, y, w, h, argb); }
void System::fb_blit_argb(const uint32_t* p, int x, int y, int w, int h)
    { ::fb_blit_argb(p, x, y, w, h); }
void System::fb_draw_string_px(const char* t, uint32_t fg, uint32_t bg,
                               int x, int y, int tb)
    { ::fb_draw_string(t, fg, bg, x, y, tb); }
void System::fb_fill_grad_v(int x, int y, int w, int h, uint32_t top, uint32_t bot)
    { ::fb_fill_grad_v(x, y, w, h, top, bot); }
void System::fb_fill_grad_h(int x, int y, int w, int h, uint32_t l, uint32_t r)
    { ::fb_fill_grad_h(x, y, w, h, l, r); }
void System::fb_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb)
    { ::fb_fill_rounded_rect(x, y, w, h, r, argb); }
void System::fb_fill_circle(int cx, int cy, int r, uint32_t argb)
    { ::fb_fill_circle(cx, cy, r, argb); }
void System::fb_draw_hline_px(int x, int y, int w, uint32_t argb)
    { ::fb_draw_hline(x, y, w, argb); }
void System::fb_blit_argb_scaled(const uint32_t* src, int sw, int sh,
                                 int dx, int dy, int dw, int dh)
    { ::fb_blit_argb_scaled(src, sw, sh, dx, dy, dw, dh); }
void System::fb_draw_string_scaled(const char* t, uint32_t fg, int x, int y, int s)
    { ::fb_draw_string_scaled(t, fg, x, y, s); }
int  System::fb_draw_wallpaper() { return ::fb_draw_wallpaper(); }
void System::fb_draw_text(const char* t, int x, int y, int font, uint32_t argb)
    { ::fb_draw_text(t, x, y, font, argb); }
int  System::fb_text_width(const char* t, int font)
    { return ::fb_text_width(t, font); }
int  System::fb_font_height(int font) { return ::fb_font_height(font); }
void System::fb_stroke_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb)
    { ::fb_stroke_rounded_rect(x, y, w, h, r, argb); }
void System::fb_shadow_rounded_rect(int x, int y, int w, int h, int r, int blur, uint32_t argb)
    { ::fb_shadow_rounded_rect(x, y, w, h, r, blur, argb); }
void System::fb_flush_px() { ::fb_flush(); }
int  System::fb_get_width()  { return ::fb_get_width(); }
int  System::fb_get_height() { return ::fb_get_height(); }
int  System::fb_is_avail()   { return ::fb_is_available(); }

void* System::load_rbx(const char* filepath) {
	int length = 0;
	const char* file_data = VFS::read_file(filepath, &length);
	if (!file_data || (uint32_t)length < sizeof(rbx_header_t)) {
		return nullptr;
	}

	const rbx_header_t* header = (const rbx_header_t*)file_data;

	// Validate the executable before touching memory: magic, format
	// version, target CPU, header layout, payload bounds, and integrity.
	if (header->magic[0] != 'R' || header->magic[1] != 'B' ||
	    header->magic[2] != 'X' || header->magic[3] != RBX_MAGIC[3]) {
		return nullptr;
	}
	if (header->version != RBX_VERSION)              return nullptr;
	if (header->arch != RBX_ARCH_X86)                return nullptr;
	if (header->header_size != sizeof(rbx_header_t)) return nullptr;
	if (header->code_size > (uint32_t)length - sizeof(rbx_header_t)) return nullptr;
	if (header->entry_point < header->load_addr ||
	    header->entry_point >= header->load_addr + header->code_size) return nullptr;

	const uint8_t* src = (const uint8_t*)(file_data + sizeof(rbx_header_t));
	if (rbx_checksum(src, header->code_size) != header->checksum) {
		return nullptr;
	}

	// Place the image at its load address
	uint8_t* dest = (uint8_t*)header->load_addr;
	for (uint32_t i = 0; i < header->code_size; i++) {
		dest[i] = src[i];
	}

	// Clear BSS
	uint8_t* bss = dest + header->code_size;
	for (uint32_t i = 0; i < header->bss_size; i++) {
		bss[i] = 0;
	}

	return (void*)header->entry_point;
}

} // namespace rit

// API C functions wrapper implementation
static void api_print(const char* text) { rit::System::print(text); }
static void api_println(const char* text) { rit::System::println(text); }
static void api_clear_screen() { rit::System::clear_screen(); }
static void api_set_color(uint8_t fg, uint8_t bg) { rit::System::set_color((rit::Color)fg, (rit::Color)bg); }
static void api_draw_char(char c, uint8_t fg, uint8_t bg, int x, int y) { rit::System::draw_char(c, (rit::Color)fg, (rit::Color)bg, x, y); }
static void api_init_mouse() { rit::System::init_mouse(); }
static int api_poll_mouse(int* x, int* y, uint8_t* buttons) { return rit::System::poll_mouse(*x, *y, *buttons) ? 1 : 0; }
static int api_poll_mouse_px(int* x, int* y, uint8_t* buttons) { return rit::System::poll_mouse_px(*x, *y, *buttons) ? 1 : 0; }
static void api_shutdown() { rit::System::shutdown(); }
static void api_reboot() { rit::System::reboot(); }
static int api_poll_keyboard(char* out_char) { return rit::System::poll_keyboard(*out_char) ? 1 : 0; }
static size_t api_get_heap_usage() { return rit::System::get_heap_usage(); }
static void api_get_time(int* hour, int* min, int* sec) { rit::System::get_time(*hour, *min, *sec); }
static void api_get_date(int* year, int* month, int* day) { rit::System::get_date(*year, *month, *day); }

static int api_vfs_exists(const char* name) { return rit::VFS::exists(name) ? 1 : 0; }
static int api_vfs_create_file(const char* name, const char* content, int length) { return rit::VFS::create_file(name, content, length) ? 1 : 0; }
static const char* api_vfs_read_file(const char* name, int* out_length) { return rit::VFS::read_file(name, out_length); }
static int api_vfs_write_file(const char* name, const char* content, int length) { return rit::VFS::write_file(name, content, length) ? 1 : 0; }
static int api_vfs_delete_file(const char* name) { return rit::VFS::delete_file(name) ? 1 : 0; }
static int api_vfs_rename_file(const char* old_name, const char* new_name) { return rit::VFS::rename_file(old_name, new_name) ? 1 : 0; }
static int api_vfs_get_file_list(const char* names[], int max_files) { return rit::VFS::get_file_list(names, max_files); }

extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);

static void (*s_launch_app_cb)(const char* title) = nullptr;

namespace rit {
void System::launch_app(const char* title) {
	if (s_launch_app_cb) {
		s_launch_app_cb(title);
	}
}
void System::set_launch_app_handler(void (*handler)(const char* title)) {
	s_launch_app_cb = handler;
}
}

static void api_enable_double_buffer(int enable) { rit::System::enable_double_buffer(enable != 0); }
static void api_flush() { rit::System::flush(); }
static void* api_load_rbx(const char* filepath) { return rit::System::load_rbx(filepath); }
static void* api_kmalloc(size_t size) { return kmalloc(size); }
static void api_kfree(void* ptr) { kfree(ptr); }
static void api_launch_app(const char* title) { rit::System::launch_app(title); }
static void api_register_launch_handler(void (*handler)(const char* title)) { rit::System::set_launch_app_handler(handler); }
static uint16_t* api_get_screen_buffer() { return g_sys_terminal->buffer; }

/* fb pixel wrappers */
static void api_fb_fill_rect(int x,int y,int w,int h,uint32_t c) { ::fb_fill_rect(x,y,w,h,c); }
static void api_fb_fill_rect_blend(int x,int y,int w,int h,uint32_t c) { ::fb_fill_rect_blend(x,y,w,h,c); }
static void api_fb_blit_argb(const uint32_t* p,int x,int y,int w,int h) { ::fb_blit_argb(p,x,y,w,h); }
static void api_fb_draw_string_px(const char* t,uint32_t fg,uint32_t bg,int x,int y,int tb) { ::fb_draw_string(t,fg,bg,x,y,tb); }
static void api_fb_fill_grad_v(int x,int y,int w,int h,uint32_t t,uint32_t b) { ::fb_fill_grad_v(x,y,w,h,t,b); }
static void api_fb_fill_grad_h(int x,int y,int w,int h,uint32_t l,uint32_t r) { ::fb_fill_grad_h(x,y,w,h,l,r); }
static void api_fb_fill_rounded_rect(int x,int y,int w,int h,int r,uint32_t c) { ::fb_fill_rounded_rect(x,y,w,h,r,c); }
static void api_fb_fill_circle(int cx,int cy,int r,uint32_t c) { ::fb_fill_circle(cx,cy,r,c); }
static void api_fb_draw_hline_px(int x,int y,int w,uint32_t c) { ::fb_draw_hline(x,y,w,c); }
static void api_fb_blit_argb_scaled(const uint32_t* p,int sw,int sh,int dx,int dy,int dw,int dh) { ::fb_blit_argb_scaled(p,sw,sh,dx,dy,dw,dh); }
static void api_fb_draw_string_scaled(const char* t,uint32_t fg,int x,int y,int s) { ::fb_draw_string_scaled(t,fg,x,y,s); }
static int  api_fb_draw_wallpaper() { return ::fb_draw_wallpaper(); }
static void api_fb_flush_px() { ::fb_flush(); }
static int  api_fb_get_width()  { return ::fb_get_width(); }
static int  api_fb_get_height() { return ::fb_get_height(); }
static int  api_fb_is_available() { return ::fb_is_available(); }

/* ── Real storage: import/export between the disk and the VFS ─────────── */

struct StorageFileEntry { char name[16]; uint32_t size; };
static StorageFileEntry g_storage_listing[32];
static int g_storage_listing_count = 0;

static void storage_collect_callback(const char* name, uint32_t size, void* user) {
	(void)user;
	if (g_storage_listing_count >= 32) return;
	StorageFileEntry& e = g_storage_listing[g_storage_listing_count++];
	int i = 0;
	while (name[i] && i < 15) { e.name[i] = name[i]; i++; }
	e.name[i] = '\0';
	e.size = size;
}

static int api_storage_describe(char* out, int out_max) {
	return fat_volume_description(out, out_max);
}

static int api_storage_import(void) {
	/* Two phases: the FAT driver's sector buffers are not reentrant, so
	 * collect the listing first, then read each file. */
	g_storage_listing_count = 0;
	int total = fat_list_root_directory(storage_collect_callback, nullptr);
	if (total < 0) return -1;

	int imported = 0;
	for (int f = 0; f < g_storage_listing_count; f++) {
		if (g_storage_listing[f].size > 65535) continue; /* VFS file limit */
		char* buffer = (char*)kmalloc(65536);
		if (!buffer) break;
		uint32_t got = 0;
		if (fat_read_file(g_storage_listing[f].name, buffer, 65536, &got)) {
			char path[64];
			const char* prefix = "/volumes/disk/";
			int p = 0;
			while (prefix[p]) { path[p] = prefix[p]; p++; }
			int n = 0;
			while (g_storage_listing[f].name[n] && p < 63) path[p++] = g_storage_listing[f].name[n++];
			path[p] = '\0';
			rit::VFS::create_file(path, buffer, (int)got);
			imported++;
		}
		kfree(buffer);
	}
	return imported;
}

static int api_storage_export(const char* vfs_path) {
	int length = 0;
	const char* data = rit::VFS::read_file(vfs_path, &length);
	if (!data || length <= 0) return 0;
	/* The on-disk name is the path's last component */
	const char* base = vfs_path;
	for (const char* c = vfs_path; *c; c++) if (*c == '/') base = c + 1;
	if (!*base) return 0;
	return fat_write_file(base, data, (uint32_t)length);
}

static int api_storage_delete(const char* name) {
	return fat_delete_file(name);
}

static int api_storage_rename(const char* old_name, const char* new_name) {
	return fat_rename_file(old_name, new_name);
}

static void api_fb_set_cursor_image(const uint32_t* argb, int w, int h) {
	::fb_set_cursor_image(argb, w, h);
}
static void api_fb_set_cursor_pos(int x, int y) {
	::fb_set_cursor_pos(x, y);
}

static void api_fb_draw_text(const char* t, int x, int y, int font, uint32_t argb)
	{ ::fb_draw_text(t, x, y, font, argb); }
static int  api_fb_text_width(const char* t, int font)
	{ return ::fb_text_width(t, font); }
static int  api_fb_font_height(int font)
	{ return ::fb_font_height(font); }
static void api_fb_stroke_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb)
	{ ::fb_stroke_rounded_rect(x, y, w, h, r, argb); }
static void api_fb_shadow_rounded_rect(int x, int y, int w, int h, int r, int blur, uint32_t argb)
	{ ::fb_shadow_rounded_rect(x, y, w, h, r, blur, argb); }

static const RitOS_API g_api_table = {
	.version = 1,
	.print = api_print,
	.println = api_println,
	.clear_screen = api_clear_screen,
	.set_color = api_set_color,
	.draw_char = api_draw_char,
	.init_mouse = api_init_mouse,
	.poll_mouse = api_poll_mouse,
	.shutdown = api_shutdown,
	.reboot = api_reboot,
	.poll_keyboard = api_poll_keyboard,
	.get_heap_usage = api_get_heap_usage,
	.get_time = api_get_time,
	.get_date = api_get_date,
	.vfs_exists = api_vfs_exists,
	.vfs_create_file = api_vfs_create_file,
	.vfs_read_file = api_vfs_read_file,
	.vfs_write_file = api_vfs_write_file,
	.vfs_delete_file = api_vfs_delete_file,
	.vfs_rename_file = api_vfs_rename_file,
	.vfs_get_file_list = api_vfs_get_file_list,
	.enable_double_buffer = api_enable_double_buffer,
	.flush = api_flush,
	.load_rbx = api_load_rbx,
	.kmalloc = api_kmalloc,
	.kfree = api_kfree,
	.launch_app = api_launch_app,
	.register_launch_handler = api_register_launch_handler,
	.get_screen_buffer = api_get_screen_buffer,
	.fb_fill_rect = api_fb_fill_rect,
	.fb_fill_rect_blend = api_fb_fill_rect_blend,
	.fb_blit_argb = api_fb_blit_argb,
	.fb_draw_string_px = api_fb_draw_string_px,
	.fb_fill_grad_v = api_fb_fill_grad_v,
	.fb_fill_grad_h = api_fb_fill_grad_h,
	.fb_fill_rounded_rect = api_fb_fill_rounded_rect,
	.fb_fill_circle = api_fb_fill_circle,
	.fb_draw_hline_px = api_fb_draw_hline_px,
	.fb_flush_px = api_fb_flush_px,
	.fb_get_width = api_fb_get_width,
	.fb_get_height = api_fb_get_height,
	.fb_is_available = api_fb_is_available,
	.poll_mouse_px = api_poll_mouse_px,
	.storage_describe = api_storage_describe,
	.storage_import = api_storage_import,
	.storage_export = api_storage_export,
	.fb_blit_argb_scaled = api_fb_blit_argb_scaled,
	.fb_draw_string_scaled = api_fb_draw_string_scaled,
	.fb_draw_wallpaper = api_fb_draw_wallpaper,
	.fb_set_cursor_image = api_fb_set_cursor_image,
	.fb_set_cursor_pos = api_fb_set_cursor_pos,
	.storage_delete = api_storage_delete,
	.storage_rename = api_storage_rename,
	.fb_draw_text = api_fb_draw_text,
	.fb_text_width = api_fb_text_width,
	.fb_font_height = api_fb_font_height,
	.fb_stroke_rounded_rect = api_fb_stroke_rounded_rect,
	.fb_shadow_rounded_rect = api_fb_shadow_rounded_rect,
};

namespace rit {
const RitOS_API* System::get_api_table() {
	return &g_api_table;
}
}
