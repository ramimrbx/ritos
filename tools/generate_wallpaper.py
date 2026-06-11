#!/usr/bin/env python3
"""Packs the default wallpaper for embedding into the kernel image.

Reads the first image in assets/sources/wallpapers/, cover-crops it to
TARGET size and writes:
  build/generated/wallpaper.argb - uint32 width, uint32 height, then
                                   width*height little-endian ARGB pixels
  build/generated/wallpaper.s    - .incbin stub exporting g_wallpaper_blob

The kernel scales the blob to the real screen resolution at runtime
(fb_draw_wallpaper), so TARGET only bounds quality and kernel size.
If no source image exists an empty (width=0) blob is emitted so the
kernel still links and falls back to a flat colour desktop.
"""
import os, struct, sys

TARGET_W, TARGET_H = 1920, 1080
SRC_DIR = "assets/sources/wallpapers"
OUT_DIR = "build/generated"

def find_source():
    if not os.path.isdir(SRC_DIR):
        return None
    for name in sorted(os.listdir(SRC_DIR)):
        if name.lower().endswith((".jpg", ".jpeg", ".png", ".bmp")):
            return os.path.join(SRC_DIR, name)
    return None

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    argb_path = os.path.join(OUT_DIR, "wallpaper.argb")
    src = find_source()

    if src is None:
        with open(argb_path, "wb") as f:
            f.write(struct.pack("<II", 0, 0))
        print("No wallpaper source found; embedded empty blob")
    else:
        from PIL import Image
        img = Image.open(src).convert("RGB")
        sw, sh = img.size
        # Cover-crop to the target aspect ratio, then resize
        if sw * TARGET_H >= TARGET_W * sh:
            new_w = sh * TARGET_W // TARGET_H
            x0 = (sw - new_w) // 2
            img = img.crop((x0, 0, x0 + new_w, sh))
        else:
            new_h = sw * TARGET_H // TARGET_W
            y0 = (sh - new_h) // 2
            img = img.crop((0, y0, sw, y0 + new_h))
        img = img.resize((TARGET_W, TARGET_H), Image.LANCZOS)

        with open(argb_path, "wb") as f:
            f.write(struct.pack("<II", TARGET_W, TARGET_H))
            data = bytearray(TARGET_W * TARGET_H * 4)
            px = img.load()
            i = 0
            for y in range(TARGET_H):
                for x in range(TARGET_W):
                    r, g, b = px[x, y]
                    data[i] = b; data[i+1] = g; data[i+2] = r; data[i+3] = 0xFF
                    i += 4
            f.write(data)
        print(f"Packed {src} ({sw}x{sh}) -> {argb_path} ({TARGET_W}x{TARGET_H})")

    with open(os.path.join(OUT_DIR, "wallpaper.s"), "w") as f:
        f.write(".section .rodata\n"
                ".align 4\n"
                ".global g_wallpaper_blob\n"
                "g_wallpaper_blob:\n"
                f".incbin \"{argb_path}\"\n")
    print("Generated build/generated/wallpaper.s")

if __name__ == "__main__":
    sys.exit(main())
