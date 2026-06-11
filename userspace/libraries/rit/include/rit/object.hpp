#ifndef RIT_OBJECT_HPP
#define RIT_OBJECT_HPP

namespace rit {

class Object {
public:
	Object() {}
	virtual ~Object() {}

	virtual const char* get_class_name() const {
		return "Object";
	}

	virtual bool equals(const Object& other) const {
		return this == &other;
	}
};

} // namespace rit

#endif
