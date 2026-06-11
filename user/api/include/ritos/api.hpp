#ifndef RITOS_API_HPP
#define RITOS_API_HPP

#include <rit/system.hpp>
#include <rit/string.hpp>

namespace ritos {

// Initialize the C++ Framework & APIs
void init();

// Print a message using the Ritos API
void print_message(const char* msg);

// Sleep helper (simple delay loop)
void sleep(uint32_t count);

} // namespace ritos

#endif
