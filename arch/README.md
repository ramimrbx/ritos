# Architecture ports

Each subdirectory is one CPU/platform port of RitOS. The portable code
(`kernel/core`, `kernel/drivers`, `kernel/lib`, all of `user/`) never changes
between ports; everything machine-specific lives here.

A port directory looks like:

```
arch/<name>/
  arch.mk            Build definitions: toolchain, target flags, linker
                     script, the port's driver sources, and the recipe that
                     packages the bootable image (ISO, SD-card image, ...).
  boot/              Startup code that runs before kmain: entry assembly,
                     linker script, bootloader config.
  drivers/           Implementations of the driver interfaces declared in
                     kernel/include/kernel/ (keyboard, mouse, terminal,
                     power, ...) using this machine's hardware.
  include/kernel/    Arch-provided headers. This directory is on the include
                     path *before* kernel/include, so a port can supply
                     machine headers like io.h under the same <kernel/...>
                     names the portable code already uses.
```

Build a specific port with:

```bash
make ARCH=x86        # default
```

## Adding a new port (e.g. arm)

1. Create `arch/arm/` with the layout above. `arch.mk` sets the
   cross-toolchain (`CC = arm-none-eabi-gcc`, ...), `TARGET_CFLAGS`,
   `LINKER_SCRIPT`, `ARCH_C_SOURCES`, `ARCH_OBJS`, and an `IMAGE` rule for
   whatever the platform boots from.
2. Implement the driver interfaces from `kernel/include/kernel/` for the
   machine (a linear framebuffer is enough for the GUI — `kernel/drivers/fb.c`
   is already portable and just needs `fb_init(addr, pitch, w, h, bpp)`).
3. Boards/SoCs that share a CPU architecture should live inside the port as
   `arch/<name>/platform/<board>/` selected by a `PLATFORM` variable in that
   port's `arch.mk`, rather than as separate top-level ports.

Current status: `x86` (i386 BIOS/GRUB) is the only implemented port. Parts of
`kernel/core/kernel.c` (multiboot parsing, COM1 serial helpers) are still
x86-specific and should migrate into `arch/x86/` as the second port appears.
