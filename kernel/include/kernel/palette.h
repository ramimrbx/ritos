#ifndef KERNEL_PALETTE_H
#define KERNEL_PALETTE_H

/* Modern remap of the 16 VGA colours used by all char-grid rendering.
 * Shared by the kernel framebuffer code and the SDK window chrome so the
 * two always agree on what e.g. "Black" looks like.
 *
 * Index:  0 Black      1 Blue       2 Green      3 Cyan
 *         4 Red        5 Magenta    6 Brown      7 LightGrey
 *         8 DarkGrey   9 LightBlue 10 LightGreen 11 LightCyan
 *        12 LightRed  13 LightMag  14 Yellow    15 White
 */
#define RITOS_PALETTE_INIT { \
    0xFF111A2Eu, 0xFF2C4B8Au, 0xFF2EBD6Bu, 0xFF38B4C8u, \
    0xFFE05252u, 0xFFB558D0u, 0xFFD08B3Eu, 0xFFB8C4DEu, \
    0xFF55657Fu, 0xFF5B8FFFu, 0xFF4ADE80u, 0xFF53E0D4u, \
    0xFFFF7B72u, 0xFFE879F9u, 0xFFFFD866u, 0xFFF0F4FFu, \
}

#endif /* KERNEL_PALETTE_H */
