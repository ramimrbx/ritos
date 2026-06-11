#ifndef RIT_VFS_HPP
#define RIT_VFS_HPP

namespace rit {

struct VirtualFile {
	char name[64];
	char content[65536]; // 64KB file size limit
	int length;
	bool is_used;
};

class VFS {
public:
	static void init();
	static bool create_file(const char* name, const char* content, int length = -1);
	static const char* read_file(const char* name, int* out_length = nullptr);
	static bool write_file(const char* name, const char* content, int length = -1);
	static bool delete_file(const char* name);
	static bool rename_file(const char* old_name, const char* new_name);
	static int get_file_list(const char* names[], int max_files);
	static bool exists(const char* name);
};

} // namespace rit

#endif
