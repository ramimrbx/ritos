#include <rit/vfs.hpp>
#include <kernel/string.h> // for memcpy

namespace rit {

#define VFS_MAX_FILES 64
static VirtualFile g_files[VFS_MAX_FILES];
static int g_file_count = 0;

static void str_copy(char* dst, const char* src, int max_len) {
	int i = 0;
	while (src[i] != '\0' && i < max_len - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static bool str_equals(const char* s1, const char* s2) {
	int i = 0;
	while (s1[i] != '\0' && s2[i] != '\0') {
		if (s1[i] != s2[i]) return false;
		i++;
	}
	return s1[i] == s2[i];
}

void VFS::init() {
	for (int i = 0; i < VFS_MAX_FILES; i++) {
		g_files[i].is_used = false;
		g_files[i].name[0] = '\0';
		g_files[i].content[0] = '\0';
		g_files[i].length = 0;
	}
	g_file_count = 0;

	// Populate initial files
	create_file("/sys/readme.txt", 
		"Welcome to RitOS C++ OS!\n\n"
		"This is a custom freestanding\n"
		"operating system running in\n"
		"VGA text mode with a memory-\n"
		"resident Virtual File System.\n"
		"Use File Explorer to CRUD\n"
		"files, and edit them in this\n"
		"Text Editor."
	);
	create_file("/sys/todo.txt", 
		"- Check memory allocations\n"
		"- Write a new document\n"
		"- Modify settings theme"
	);
}

bool VFS::exists(const char* name) {
	for (int i = 0; i < VFS_MAX_FILES; i++) {
		if (g_files[i].is_used && str_equals(g_files[i].name, name)) {
			return true;
		}
	}
	return false;
}

bool VFS::create_file(const char* name, const char* content, int length) {
	if (exists(name)) return false;

	for (int i = 0; i < VFS_MAX_FILES; i++) {
		if (!g_files[i].is_used) {
			str_copy(g_files[i].name, name, 32);
			if (content) {
				if (length < 0) {
					length = 0;
					while (content[length] != '\0') length++;
				}
				if (length > 65535) length = 65535;
				memcpy(g_files[i].content, content, length);
				g_files[i].length = length;
			} else {
				g_files[i].content[0] = '\0';
				g_files[i].length = 0;
			}
			g_files[i].is_used = true;
			g_file_count++;
			return true;
		}
	}
	return false;
}

const char* VFS::read_file(const char* name, int* out_length) {
	for (int i = 0; i < VFS_MAX_FILES; i++) {
		if (g_files[i].is_used && str_equals(g_files[i].name, name)) {
			if (out_length) *out_length = g_files[i].length;
			return g_files[i].content;
		}
	}
	return nullptr;
}

bool VFS::write_file(const char* name, const char* content, int length) {
	for (int i = 0; i < VFS_MAX_FILES; i++) {
		if (g_files[i].is_used && str_equals(g_files[i].name, name)) {
			if (length < 0) {
				length = 0;
				while (content[length] != '\0') length++;
			}
			if (length > 65535) length = 65535;
			memcpy(g_files[i].content, content, length);
			g_files[i].length = length;
			return true;
		}
	}
	return false;
}

bool VFS::delete_file(const char* name) {
	for (int i = 0; i < VFS_MAX_FILES; i++) {
		if (g_files[i].is_used && str_equals(g_files[i].name, name)) {
			g_files[i].is_used = false;
			g_files[i].name[0] = '\0';
			g_files[i].content[0] = '\0';
			g_files[i].length = 0;
			g_file_count--;
			return true;
		}
	}
	return false;
}

bool VFS::rename_file(const char* old_name, const char* new_name) {
	if (exists(new_name)) return false;
	for (int i = 0; i < VFS_MAX_FILES; i++) {
		if (g_files[i].is_used && str_equals(g_files[i].name, old_name)) {
			str_copy(g_files[i].name, new_name, 32);
			return true;
		}
	}
	return false;
}

int VFS::get_file_list(const char* names[], int max_files) {
	int count = 0;
	for (int i = 0; i < VFS_MAX_FILES && count < max_files; i++) {
		if (g_files[i].is_used) {
			names[count++] = g_files[i].name;
		}
	}
	return count;
}

} // namespace rit
