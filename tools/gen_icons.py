#!/usr/bin/env python3
"""Converts downloaded SVG icons to 32x32 ARGB C array header for framebuffer rendering."""
import os, sys

# (svg name, C identifier, label, target app rbx name)
ICONS = [
    ("monitor",   "SYSMON",    "SysMon",    "sysmon"),
    ("calculator","CALC",      "Calc",      "calculator"),
    ("editor",    "EDITOR",    "Editor",    "texteditor"),
    ("files",     "FILES",     "Files",     "filemanager"),
    ("clock",     "CLOCK",     "Clock",     "clock"),
    ("calendar",  "CALENDAR",  "Calendar",  "calendar"),
    ("settings",  "SETTINGS",  "Settings",  "settings"),
    ("terminal",  "TERMINAL",  "Terminal",  "terminal"),
    ("imgview",   "IMGVIEW",   "ImgViewer", "imgview"),
]

def main():
    try:
        from PIL import Image
        import io as _io
    except ImportError:
        import subprocess
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'Pillow', '-q'])
        from PIL import Image
        import io as _io
    import io as _io

    try:
        import cairosvg
    except ImportError:
        import subprocess
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'cairosvg', '-q'])
        import cairosvg

    SIZE = 32
    os.makedirs('assets/gen', exist_ok=True)

    # Accent colors for fallback icons
    FALLBACK_COLORS = [
        0xFF7B72FF, 0xFF4ECDC4, 0xFFFF6B6B, 0xFFFFD93D,
        0xFF6BCB77, 0xFFFF9F43, 0xFFA29BFE, 0xFF74B9FF, 0xFFFF85A1,
    ]

    all_pixels = []
    for idx, (svg_name, var, label, app) in enumerate(ICONS):
        svg_path = f"assets/src/icons/{svg_name}.svg"
        if os.path.exists(svg_path):
            try:
                png = cairosvg.svg2png(url=svg_path, output_width=SIZE*2, output_height=SIZE*2)
                img = Image.open(_io.BytesIO(png)).convert('RGBA')
                img = img.resize((SIZE, SIZE), Image.LANCZOS)
                pixels = []
                for y in range(SIZE):
                    for x in range(SIZE):
                        r, g, b, a = img.getpixel((x, y))
                        pixels.append((a << 24) | (r << 16) | (g << 8) | b)
                print(f"  OK: {svg_name}")
            except Exception as e:
                print(f"  FAIL {svg_name}: {e}, using fallback")
                fc = FALLBACK_COLORS[idx % len(FALLBACK_COLORS)]
                pixels = _make_fallback(SIZE, fc, label[0])
        else:
            print(f"  MISSING: {svg_path}, using fallback")
            fc = FALLBACK_COLORS[idx % len(FALLBACK_COLORS)]
            pixels = _make_fallback(SIZE, fc, label[0])
        all_pixels.append((var, label, pixels, app))

    with open('assets/gen/icons_32.h', 'w') as f:
        f.write('#ifndef ICONS_32_H\n#define ICONS_32_H\n#include <stdint.h>\n\n')
        f.write(f'#define ICON_SIZE 32\n#define ICON_COUNT {len(ICONS)}\n\n')
        for (var, label, pixels, _app) in all_pixels:
            f.write(f'static const uint32_t ICON_{var}[{SIZE*SIZE}] = {{\n')
            for i, p in enumerate(pixels):
                if i % 8 == 0: f.write('  ')
                f.write(f'0x{p:08X}')
                if i < len(pixels)-1: f.write(', ')
                if (i+1) % 8 == 0: f.write('\n')
            f.write('\n};\n\n')

        f.write(f'static const uint32_t* const g_icon_pixels[ICON_COUNT] = {{\n')
        for (var, _, _, _app) in all_pixels:
            f.write(f'  ICON_{var},\n')
        f.write('};\n\n')

        f.write(f'static const char* const g_icon_labels[ICON_COUNT] = {{\n')
        for (_, label, _, _app) in all_pixels:
            f.write(f'  "{label}",\n')
        f.write('};\n\n#endif\n')
    print("Generated assets/gen/icons_32.h")

    # Raw ARGB blobs, one per app, embedded into each .rbx by elf2rbx
    # (--icon=...) so executables carry their own icon.
    import struct
    os.makedirs('assets/gen/icons', exist_ok=True)
    for (_, _, pixels, app) in all_pixels:
        with open(f'assets/gen/icons/{app}.argb', 'wb') as f:
            f.write(struct.pack(f'<{len(pixels)}I', *pixels))
        print(f"Generated assets/gen/icons/{app}.argb")

def _make_fallback(size, color, letter):
    """Create a simple colored square with letter for fallback."""
    try:
        from PIL import Image, ImageDraw, ImageFont
        import io as _io
        img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        r = (color >> 16) & 0xFF
        g = (color >> 8) & 0xFF
        b = color & 0xFF
        # Rounded rect
        draw.rounded_rectangle([0, 0, size-1, size-1], radius=6, fill=(r, g, b, 230))
        # Letter
        try:
            font = ImageFont.load_default(size=16)
        except Exception:
            font = ImageFont.load_default()
        draw.text((size//2-4, size//2-8), letter, fill=(255, 255, 255, 255), font=font)
        pixels = []
        for py in range(size):
            for px in range(size):
                ri, gi, bi, ai = img.getpixel((px, py))
                pixels.append((ai << 24) | (ri << 16) | (gi << 8) | bi)
        return pixels
    except Exception:
        r = (color >> 16) & 0xFF
        g = (color >> 8) & 0xFF
        b = color & 0xFF
        c = 0xFF000000 | (r << 16) | (g << 8) | b
        return [c] * (size * size)

if __name__ == '__main__':
    main()
