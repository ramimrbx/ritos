#ifndef KERNEL_MOUSE_H
#define KERNEL_MOUSE_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MouseState {
	int x;
	int y;
	uint8_t buttons;
} MouseState;

void mouse_init(void);
bool mouse_poll(MouseState* state);

#ifdef __cplusplus
}
#endif

#endif
