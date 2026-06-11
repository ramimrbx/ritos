#include <ritos/application_windows.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>

extern const RitOS_API* g_api;

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
	g_api = api;
	ritos::Window::set_desktop_interface(desktop);

	static ritos::ClockWindow* clock = nullptr;
	if (!clock) {
		clock = new ritos::ClockWindow(90, 10);
	}

	static rbx_module mod;
	mod.type = RBX_MODULE_APP_WINDOW;
	const char* nm = "Clock";
	int i = 0;
	while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
	mod.name[i] = '\0';
	mod.instance = clock;

	mod.draw = [](void* inst) { static_cast<ritos::ClockWindow*>(inst)->draw(); };
	mod.handle_click = [](void* inst, int mx, int my) { static_cast<ritos::ClockWindow*>(inst)->handle_click(mx, my); };
	mod.handle_key = [](void* inst, char key) { static_cast<ritos::ClockWindow*>(inst)->handle_key(key); };
	mod.destroy = [](void* inst) { delete static_cast<ritos::ClockWindow*>(inst); };

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
