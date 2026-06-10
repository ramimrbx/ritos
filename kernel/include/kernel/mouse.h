#ifndef KERNEL_MOUSE_H
#define KERNEL_MOUSE_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MouseState {
	int x;          /* char-cell column (pixel_x / 8)  */
	int y;          /* char-cell row    (pixel_y / 16) */
	int pixel_x;    /* exact pixel position, 0..1023   */
	int pixel_y;    /* exact pixel position, 0..767    */
	uint8_t buttons;
} MouseState;

void mouse_init(void);
bool mouse_poll(MouseState* state);

#ifdef __cplusplus
}
#endif

#endif
