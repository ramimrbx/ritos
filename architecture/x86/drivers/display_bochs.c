/*
 * Bochs/QEMU display controller (BGA / VBE_DISPI) — page-flip presenter.
 *
 * QEMU's std VGA, Bochs, and VirtualBox expose this register interface on
 * I/O ports 0x1CE/0x1CF regardless of whether the firmware that set the
 * mode was a legacy VGA BIOS (BIOS boot) or the EFI GOP driver (UEFI
 * boot). We re-program the already-running mode with a doubled virtual
 * height, which gives the kernel two full screens of VRAM to flip
 * between: fb_flush copies the frame into the hidden page and moves the
 * scanout origin atomically, so a frame can never appear half-drawn.
 *
 * On hardware without this controller detection fails and the portable
 * framebuffer core falls back to plain dirty-rect copies.
 */
#include <kernel/framebuffer.h>
#include <kernel/input_output.h>

#define DISPI_IOPORT_INDEX  0x01CE
#define DISPI_IOPORT_DATA   0x01CF

#define DISPI_INDEX_ID          0x0
#define DISPI_INDEX_XRES        0x1
#define DISPI_INDEX_YRES        0x2
#define DISPI_INDEX_BPP         0x3
#define DISPI_INDEX_ENABLE      0x4
#define DISPI_INDEX_VIRT_WIDTH  0x6
#define DISPI_INDEX_VIRT_HEIGHT 0x7
#define DISPI_INDEX_X_OFFSET    0x8
#define DISPI_INDEX_Y_OFFSET    0x9
#define DISPI_INDEX_VRAM_64K    0xA

#define DISPI_ID_MIN        0xB0C0
#define DISPI_ID_MAX        0xB0C6

#define DISPI_DISABLED      0x00
#define DISPI_ENABLED       0x01
#define DISPI_LFB_ENABLED   0x40
#define DISPI_NOCLEARMEM    0x80

static uint16_t dispi_read(uint16_t index) {
    outw(DISPI_IOPORT_INDEX, index);
    return inw(DISPI_IOPORT_DATA);
}

static void dispi_write(uint16_t index, uint16_t value) {
    outw(DISPI_IOPORT_INDEX, index);
    outw(DISPI_IOPORT_DATA, value);
}

/* Strong overrides of the weak hooks in kernel/drivers/framebuffer.c */

int fb_hw_flip_setup(uint64_t phys_addr, int w, int h, int bpp,
                     uint32_t* pitch_io) {
    (void)phys_addr;
    if (bpp != 32 || w <= 0 || h <= 0 || h * 2 > 0xFFFF) return 0;

    uint16_t id = dispi_read(DISPI_INDEX_ID);
    if (id < DISPI_ID_MIN || id > DISPI_ID_MAX) return 0;

    /* Enough VRAM for two full pages? */
    uint32_t vram = (uint32_t)dispi_read(DISPI_INDEX_VRAM_64K) * 64u * 1024u;
    uint32_t page = (uint32_t)w * 4u * (uint32_t)h;
    if (vram == 0 || vram < page * 2u) return 0;

    /* Re-set the current mode with a doubled virtual height. NOCLEARMEM
     * keeps VRAM intact; the first flush repaints everything anyway. */
    dispi_write(DISPI_INDEX_ENABLE,      DISPI_DISABLED);
    dispi_write(DISPI_INDEX_XRES,        (uint16_t)w);
    dispi_write(DISPI_INDEX_YRES,        (uint16_t)h);
    dispi_write(DISPI_INDEX_BPP,         32);
    dispi_write(DISPI_INDEX_VIRT_WIDTH,  (uint16_t)w);
    dispi_write(DISPI_INDEX_VIRT_HEIGHT, (uint16_t)(h * 2));
    dispi_write(DISPI_INDEX_X_OFFSET,    0);
    dispi_write(DISPI_INDEX_Y_OFFSET,    0);
    dispi_write(DISPI_INDEX_ENABLE,
                DISPI_ENABLED | DISPI_LFB_ENABLED | DISPI_NOCLEARMEM);

    /* Confirm the controller accepted the doubled virtual height */
    if (dispi_read(DISPI_INDEX_VIRT_HEIGHT) < (uint16_t)(h * 2)) {
        dispi_write(DISPI_INDEX_VIRT_HEIGHT, (uint16_t)h);
        return 0;
    }

    *pitch_io = (uint32_t)w * 4u;   /* linear: pitch == width * bytes/px */
    return 2;
}

void fb_hw_flip(int y_offset) {
    dispi_write(DISPI_INDEX_Y_OFFSET, (uint16_t)y_offset);
}
