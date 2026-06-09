#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool keyboard_poll(char* out_char);

#ifdef __cplusplus
}
#endif

#endif
