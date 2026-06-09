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

// VFS redirects
bool VFS::exists(const char* name) { return g_api->vfs_exists(name) != 0; }
bool VFS::create_file(const char* name, const char* content, int length) { return g_api->vfs_create_file(name, content, length) != 0; }
const char* VFS::read_file(const char* name, int* out_length) { return g_api->vfs_read_file(name, out_length); }
bool VFS::write_file(const char* name, const char* content, int length) { return g_api->vfs_write_file(name, content, length) != 0; }
bool VFS::delete_file(const char* name) { return g_api->vfs_delete_file(name) != 0; }
bool VFS::rename_file(const char* old_name, const char* new_name) { return g_api->vfs_rename_file(old_name, new_name) != 0; }
int VFS::get_file_list(const char* names[], int max_files) { return g_api->vfs_get_file_list(names, max_files); }

} // namespace rit
