#include "../include/rit/system.hpp"
#include "../include/rit/vfs.hpp"
#include <stddef.h>

// Global API pointer accessed by all parts of the application
const RitOS_API* g_api = nullptr;

// Redirection of memory allocation operators for apps to use kernel heap via API table
void* operator new(size_t size) {
	return g_api->kmalloc(size);
}

void* operator new[](size_t size) {
	return g_api->kmalloc(size);
}

void operator delete(void* ptr) noexcept {
	g_api->kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
	g_api->kfree(ptr);
}

void operator delete(void* ptr, size_t size) noexcept {
	(void)size;
	g_api->kfree(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
	(void)size;
	g_api->kfree(ptr);
}

// Global constructors support fallback
extern "C" void __cxa_pure_virtual() {
	while (1) {
		__asm__ volatile("cli; hlt");
	}
}

namespace rit {

void System::print(const char* text) { g_api->print(text); }
void System::print(const String& text) { g_api->print(text.c_str()); }
void System::println(const char* text) { g_api->println(text); }
void System::println(const String& text) { g_api->println(text.c_str()); }
void System::clear_screen() { g_api->clear_screen(); }
void System::set_color(Color fg, Color bg) { g_api->set_color((uint8_t)fg, (uint8_t)bg); }
void System::draw_char(char c, Color fg, Color bg, int x, int y) { g_api->draw_char(c, (uint8_t)fg, (uint8_t)bg, x, y); }

void System::init_mouse() { g_api->init_mouse(); }
bool System::poll_mouse(int& x, int& y, uint8_t& buttons) { return g_api->poll_mouse(&x, &y, &buttons) != 0; }
bool System::poll_mouse_px(int& x, int& y, uint8_t& buttons) { return g_api->poll_mouse_px(&x, &y, &buttons) != 0; }

void System::shutdown() { g_api->shutdown(); }
void System::reboot() { g_api->reboot(); }

bool System::poll_keyboard(char& out_char) { return g_api->poll_keyboard(&out_char) != 0; }

size_t System::get_heap_usage() { return g_api->get_heap_usage(); }
void System::get_time(int& hour, int& min, int& sec) { g_api->get_time(&hour, &min, &sec); }
void System::get_date(int& year, int& month, int& day) { g_api->get_date(&year, &month, &day); }

void System::enable_double_buffer(bool enable) { g_api->enable_double_buffer(enable ? 1 : 0); }
void System::flush() { g_api->flush(); }
void* System::load_rbx(const char* filepath) { return g_api->load_rbx(filepath); }
void System::launch_app(const char* title) { g_api->launch_app(title); }
void System::set_launch_app_handler(void (*handler)(const char* title)) { g_api->register_launch_handler(handler); }

/* ── Framebuffer pixel API – app-side forwarding ──────────────────────── */
void System::fb_fill_rect(int x,int y,int w,int h,uint32_t c) { g_api->fb_fill_rect(x,y,w,h,c); }
void System::fb_fill_rect_blend(int x,int y,int w,int h,uint32_t c) { g_api->fb_fill_rect_blend(x,y,w,h,c); }
void System::fb_blit_argb(const uint32_t* p,int x,int y,int w,int h) { g_api->fb_blit_argb(p,x,y,w,h); }
void System::fb_draw_string_px(const char* t,uint32_t fg,uint32_t bg,int x,int y,int tb) { g_api->fb_draw_string_px(t,fg,bg,x,y,tb); }
void System::fb_fill_grad_v(int x,int y,int w,int h,uint32_t top,uint32_t bot) { g_api->fb_fill_grad_v(x,y,w,h,top,bot); }
void System::fb_fill_grad_h(int x,int y,int w,int h,uint32_t l,uint32_t r) { g_api->fb_fill_grad_h(x,y,w,h,l,r); }
void System::fb_fill_rounded_rect(int x,int y,int w,int h,int r,uint32_t c) { g_api->fb_fill_rounded_rect(x,y,w,h,r,c); }
void System::fb_fill_circle(int cx,int cy,int r,uint32_t c) { g_api->fb_fill_circle(cx,cy,r,c); }
void System::fb_draw_hline_px(int x,int y,int w,uint32_t c) { g_api->fb_draw_hline_px(x,y,w,c); }
void System::fb_flush_px() { g_api->fb_flush_px(); }
int  System::fb_get_width()  { return g_api->fb_get_width(); }
int  System::fb_get_height() { return g_api->fb_get_height(); }
int  System::fb_is_avail()   { return g_api->fb_is_available(); }

// VFS redirects
bool VFS::exists(const char* name) { return g_api->vfs_exists(name) != 0; }
bool VFS::create_file(const char* name, const char* content, int length) { return g_api->vfs_create_file(name, content, length) != 0; }
const char* VFS::read_file(const char* name, int* out_length) { return g_api->vfs_read_file(name, out_length); }
bool VFS::write_file(const char* name, const char* content, int length) { return g_api->vfs_write_file(name, content, length) != 0; }
bool VFS::delete_file(const char* name) { return g_api->vfs_delete_file(name) != 0; }
bool VFS::rename_file(const char* old_name, const char* new_name) { return g_api->vfs_rename_file(old_name, new_name) != 0; }
int VFS::get_file_list(const char* names[], int max_files) { return g_api->vfs_get_file_list(names, max_files); }

} // namespace rit
