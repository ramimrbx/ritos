#ifndef RIT_STRING_HPP
#define RIT_STRING_HPP

#include "object.hpp"
#include <stddef.h>

extern "C" size_t strlen(const char* str);
extern "C" void* memcpy(void* dst, const void* src, size_t n);

namespace rit {

class String : public Object {
private:
	char* m_buffer;
	size_t m_length;

	void allocate_and_copy(const char* str, size_t len) {
		m_length = len;
		m_buffer = new char[m_length + 1];
		memcpy(m_buffer, str, m_length);
		m_buffer[m_length] = '\0';
	}

public:
	String() : m_buffer(nullptr), m_length(0) {
		allocate_and_copy("", 0);
	}

	String(const char* str) {
		size_t len = str ? strlen(str) : 0;
		allocate_and_copy(str ? str : "", len);
	}

	String(const String& other) {
		allocate_and_copy(other.m_buffer, other.m_length);
	}

	~String() override {
		delete[] m_buffer;
	}

	String& operator=(const String& other) {
		if (this != &other) {
			delete[] m_buffer;
			allocate_and_copy(other.m_buffer, other.m_length);
		}
		return *this;
	}

	const char* c_str() const {
		return m_buffer;
	}

	size_t length() const {
		return m_length;
	}

	const char* get_class_name() const override {
		return "String";
	}

	bool equals(const Object& other) const override {
		if (other.get_class_name() != get_class_name()) {
			return false;
		}
		const auto& str_other = static_cast<const String&>(other);
		if (str_other.m_length != m_length) {
			return false;
		}
		for (size_t i = 0; i < m_length; i++) {
			if (str_other.m_buffer[i] != m_buffer[i]) {
				return false;
			}
		}
		return true;
	}
};

} // namespace rit

#endif
