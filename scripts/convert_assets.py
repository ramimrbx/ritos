#!/usr/bin/env python3
"""
RitOS Asset Converter
Downloads icons/wallpaper and converts them to VGA half-block format.
"""
import os
import sys
import urllib.request
import urllib.error
import io

try:
    from PIL import Image, ImageDraw
    PIL_OK = True
except ImportError:
    print("ERROR: Pillow not available")
    sys.exit(1)

# ── VGA Palette ────────────────────────────────────────────────────────────────
VGA_PALETTE = [
    (0,   0,   0),   # 0  Black
    (0,   0,   170), # 1  Blue
    (0,   170, 0),   # 2  Green
    (0,   170, 170), # 3  Cyan
    (170, 0,   0),   # 4  Red
    (170, 0,   170), # 5  Magenta
    (170, 85,  0),   # 6  Brown
    (170, 170, 170), # 7  LightGrey
    (85,  85,  85),  # 8  DarkGrey
    (85,  85,  255), # 9  LightBlue
    (85,  255, 85),  # 10 LightGreen
    (85,  255, 255), # 11 LightCyan
    (255, 85,  85),  # 12 LightRed
    (255, 85,  255), # 13 LightMagenta
    (255, 255, 85),  # 14 Yellow
    (255, 255, 255), # 15 White
]

def nearest_vga_color(r, g, b, a=255):
    """Return the index of the nearest VGA palette color."""
    # Transparent/very transparent → black
    if a < 64:
        return 0
    best_idx = 0
    best_dist = float('inf')
    for idx, (pr, pg, pb) in enumerate(VGA_PALETTE):
        dr = int(r) - pr
        dg = int(g) - pg
        db = int(b) - pb
        d = dr*dr + dg*dg + db*db
        if d < best_dist:
            best_dist = d
            best_idx = idx
    return best_idx

def pixel_to_halfblock(top_idx, bot_idx):
    """
    Given top and bottom VGA color indices, return (char, fg, bg).
    Half-block rules:
      both 0        → ' ' (space), fg=0, bg=0
      top==bot!=0   → '\xDB' (█), fg=top, bg=top (or bg=0, same visual)
      top=0, bot!=0 → '\xDC' (▄), fg=bot, bg=0
      top!=0, bot=0 → '\xDF' (▀), fg=top, bg=0
      top!=bot, both!=0 → '\xDF' (▀), fg=top, bg=bot
    """
    if top_idx == 0 and bot_idx == 0:
        return (0x20, 0, 0)
    elif top_idx == bot_idx:
        return (0xDB, top_idx, 0)
    elif top_idx == 0:
        return (0xDC, bot_idx, 0)
    elif bot_idx == 0:
        return (0xDF, top_idx, 0)
    else:
        return (0xDF, top_idx, bot_idx)

def image_to_halfblock(img, w_cells, h_cells):
    """
    Convert a PIL image to half-block cell grid.
    img is resized to w_cells × (h_cells*2) pixels.
    Returns list of (char, fg, bg) tuples, row-major.
    """
    img = img.convert('RGBA')
    img = img.resize((w_cells, h_cells * 2), Image.LANCZOS)

    cells = []
    for row in range(h_cells):
        for col in range(w_cells):
            tr, tg, tb, ta = img.getpixel((col, row * 2))
            br, bg, bb, ba = img.getpixel((col, row * 2 + 1))
            top_idx = nearest_vga_color(tr, tg, tb, ta)
            bot_idx = nearest_vga_color(br, bg, bb, ba)
            cells.append(pixel_to_halfblock(top_idx, bot_idx))
    return cells

# ── Icon Drawing Fallbacks ─────────────────────────────────────────────────────
def make_fallback_icon(name):
    """Create a simple 32×32 programmatic icon for the given name."""
    img = Image.new('RGBA', (32, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Border
    draw.rectangle([0, 0, 31, 31], outline=(85, 255, 255), width=2)

    if name == 'monitor':
        draw.rectangle([3, 4, 28, 22], outline=(85, 255, 255))
        draw.rectangle([12, 22, 19, 27], fill=(170, 170, 170))
        draw.rectangle([8, 28, 23, 29], fill=(170, 170, 170))
        # bars
        for i, h in enumerate([6, 10, 8, 12]):
            x = 5 + i*6
            draw.rectangle([x, 20-h, x+4, 20], fill=(85, 255, 85))
    elif name == 'calculator':
        draw.rectangle([4, 4, 27, 27], outline=(255, 255, 85))
        for row in range(3):
            for col in range(3):
                x, y = 7 + col*7, 10 + row*6
                draw.rectangle([x, y, x+4, y+3], fill=(255, 255, 85))
        draw.rectangle([7, 5, 24, 8], fill=(170, 170, 170))
    elif name == 'editor':
        draw.rectangle([5, 3, 26, 28], fill=(170, 170, 170), outline=(85, 255, 255))
        for y in [8, 12, 16, 20]:
            draw.line([8, y, 22, y], fill=(0, 0, 0))
        draw.rectangle([8, 23, 11, 26], fill=(0, 0, 170))
    elif name == 'files':
        draw.rectangle([4, 8, 28, 28], fill=(255, 255, 85), outline=(170, 85, 0))
        draw.polygon([(4, 8), (4, 4), (14, 4), (16, 8)], fill=(255, 255, 85), outline=(170, 85, 0))
        draw.line([4, 14, 28, 14], fill=(170, 85, 0))
        draw.line([4, 20, 28, 20], fill=(170, 85, 0))
    elif name == 'clock':
        draw.ellipse([3, 3, 28, 28], outline=(255, 85, 255), width=2)
        # hands
        draw.line([15, 15, 15, 8], fill=(255, 255, 255), width=2)  # 12-o-clock
        draw.line([15, 15, 21, 18], fill=(255, 85, 85), width=1)   # 3-o-clock
    elif name == 'calendar':
        draw.rectangle([3, 5, 28, 28], fill=(255, 255, 255), outline=(85, 255, 255))
        draw.rectangle([3, 5, 28, 11], fill=(255, 85, 85))
        draw.line([3, 17, 28, 17], fill=(170, 170, 170))
        draw.line([11, 11, 11, 28], fill=(170, 170, 170))
        draw.line([19, 11, 19, 28], fill=(170, 170, 170))
        # Rings
        draw.line([9, 3, 9, 9], fill=(85, 85, 85), width=2)
        draw.line([22, 3, 22, 9], fill=(85, 85, 85), width=2)
    elif name == 'settings':
        # Gear
        draw.ellipse([10, 10, 21, 21], outline=(255, 85, 255), width=2)
        for angle_deg in range(0, 360, 45):
            import math
            a = math.radians(angle_deg)
            x1 = int(15 + 9*math.cos(a))
            y1 = int(15 + 9*math.sin(a))
            x2 = int(15 + 13*math.cos(a))
            y2 = int(15 + 13*math.sin(a))
            draw.line([x1, y1, x2, y2], fill=(255, 85, 255), width=2)
    elif name == 'terminal':
        draw.rectangle([3, 3, 28, 28], fill=(0, 0, 0), outline=(85, 255, 85))
        # >_ prompt
        draw.polygon([(6, 10), (6, 16), (11, 13)], fill=(85, 255, 85))
        draw.line([13, 18, 22, 18], fill=(85, 255, 85), width=2)
    elif name == 'imgview':
        draw.rectangle([3, 5, 28, 26], fill=(170, 170, 170), outline=(85, 255, 255))
        # Mountain
        draw.polygon([(5, 24), (12, 14), (19, 20), (24, 13), (27, 24)], fill=(0, 170, 0))
        # Sun
        draw.ellipse([5, 7, 11, 13], fill=(255, 255, 85))
    else:
        # Generic: just an 'X'
        draw.line([6, 6, 25, 25], fill=(85, 255, 255), width=2)
        draw.line([25, 6, 6, 25], fill=(85, 255, 255), width=2)

    return img

# ── Download Icons ─────────────────────────────────────────────────────────────
ICONS_RAW_DIR = '/home/ramim/Projects/Software/ritos/assets/icons_raw'
WALLPAPERS_DIR = '/home/ramim/Projects/Software/ritos/assets/wallpapers'
os.makedirs(ICONS_RAW_DIR, exist_ok=True)
os.makedirs(WALLPAPERS_DIR, exist_ok=True)

# Iconify PNG API - requires conversion from SVG
# Instead use a direct PNG source: Iconify's PNG export endpoint
ICON_URLS = {
    'monitor':    'https://api.iconify.design/mdi/monitor.svg?width=32&height=32&color=%2355ffff',
    'calculator': 'https://api.iconify.design/mdi/calculator.svg?width=32&height=32&color=%23ffff55',
    'editor':     'https://api.iconify.design/mdi/file-document-edit.svg?width=32&height=32&color=%2355ff55',
    'files':      'https://api.iconify.design/mdi/folder.svg?width=32&height=32&color=%23ffaa00',
    'clock':      'https://api.iconify.design/mdi/clock.svg?width=32&height=32&color=%23ff55ff',
    'calendar':   'https://api.iconify.design/mdi/calendar.svg?width=32&height=32&color=%23ff5555',
    'settings':   'https://api.iconify.design/mdi/cog.svg?width=32&height=32&color=%23ff55ff',
    'terminal':   'https://api.iconify.design/mdi/console.svg?width=32&height=32&color=%2355ff55',
    'imgview':    'https://api.iconify.design/mdi/image.svg?width=32&height=32&color=%2355ffff',
}

icon_images = {}

for name, url in ICON_URLS.items():
    raw_path = os.path.join(ICONS_RAW_DIR, f'{name}.svg')
    png_path = os.path.join(ICONS_RAW_DIR, f'{name}.png')

    # Try download
    downloaded = False
    if not os.path.exists(png_path):
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
            with urllib.request.urlopen(req, timeout=10) as resp:
                data = resp.read()

            # Check if it's SVG
            if data[:5] == b'<svg ' or data[:14] == b'<?xml version=':
                # Save SVG and try cairosvg
                with open(raw_path, 'wb') as f:
                    f.write(data)
                try:
                    import cairosvg
                    cairosvg.svg2png(url=raw_path, write_to=png_path, output_width=32, output_height=32)
                    downloaded = True
                    print(f"  {name}: converted SVG->PNG via cairosvg")
                except ImportError:
                    # Try rsvg-convert
                    ret = os.system(f'rsvg-convert -w 32 -h 32 "{raw_path}" -o "{png_path}" 2>/dev/null')
                    if ret == 0 and os.path.exists(png_path):
                        downloaded = True
                        print(f"  {name}: converted SVG->PNG via rsvg-convert")
                    else:
                        # Try inkscape
                        ret = os.system(f'inkscape --export-type=png --export-width=32 --export-height=32 "{raw_path}" --export-filename="{png_path}" 2>/dev/null')
                        if ret == 0 and os.path.exists(png_path):
                            downloaded = True
                            print(f"  {name}: converted SVG->PNG via inkscape")
                        else:
                            print(f"  {name}: SVG download OK but no SVG->PNG converter; using fallback")
            else:
                # Might be PNG directly
                with open(png_path, 'wb') as f:
                    f.write(data)
                downloaded = True
                print(f"  {name}: downloaded PNG directly")

        except Exception as e:
            print(f"  {name}: download failed ({e}); using fallback")
    else:
        downloaded = True
        print(f"  {name}: using cached PNG")

    # Load or create fallback
    if downloaded and os.path.exists(png_path):
        try:
            icon_images[name] = Image.open(png_path).convert('RGBA')
        except Exception as e:
            print(f"  {name}: PNG load failed ({e}); using fallback")
            icon_images[name] = make_fallback_icon(name)
    else:
        icon_images[name] = make_fallback_icon(name)
        # Save the fallback so we have it
        icon_images[name].save(png_path.replace('.png', '_fallback.png'))
        print(f"  {name}: using programmatic fallback")

print(f"\nLoaded {len(icon_images)} icons")

# ── Download Wallpaper ─────────────────────────────────────────────────────────
wp_path = os.path.join(WALLPAPERS_DIR, 'wallpaper.jpg')
wallpaper_img = None

if not os.path.exists(wp_path):
    # Try picsum
    for wp_url in [
        'https://picsum.photos/320/184',
        'https://picsum.photos/seed/ritos/320/184',
    ]:
        try:
            req = urllib.request.Request(wp_url, headers={'User-Agent': 'Mozilla/5.0'})
            with urllib.request.urlopen(req, timeout=15) as resp:
                data = resp.read()
            with open(wp_path, 'wb') as f:
                f.write(data)
            wallpaper_img = Image.open(wp_path).convert('RGB')
            print(f"\nWallpaper downloaded ({len(data)} bytes)")
            break
        except Exception as e:
            print(f"  Wallpaper download failed: {e}")

if wallpaper_img is None:
    if os.path.exists(wp_path):
        try:
            wallpaper_img = Image.open(wp_path).convert('RGB')
            print(f"\nWallpaper loaded from cache")
        except Exception:
            pass

if wallpaper_img is None:
    print("\nGenerating gradient wallpaper programmatically...")
    wp_w, wp_h = 320, 184
    wallpaper_img = Image.new('RGB', (wp_w, wp_h))
    import math
    for y in range(wp_h):
        for x in range(wp_w):
            # Blue-to-dark gradient with subtle wave
            t = x / wp_w
            s = y / wp_h
            wave = int(20 * math.sin(x * 0.05 + y * 0.03))
            r = int(0   + t * 40  + wave)
            g = int(10  + s * 30  + wave // 2)
            b = int(100 + t * 80  + (1-s)*60)
            wallpaper_img.putpixel((x, y), (
                max(0, min(255, r)),
                max(0, min(255, g)),
                max(0, min(255, b))
            ))
    wallpaper_img.save(wp_path)
    print("  Gradient wallpaper generated")

# ── Convert Icons → HalfBlock (6×4 cells) ─────────────────────────────────────
ICON_W = 6  # character cells wide
ICON_H = 4  # character cells tall

icon_cells = {}
for name, img in icon_images.items():
    cells = image_to_halfblock(img, ICON_W, ICON_H)
    icon_cells[name] = cells

# ── Convert Wallpaper → HalfBlock (80×23 cells) ───────────────────────────────
WP_W = 80
WP_H = 23

wp_cells = image_to_halfblock(wallpaper_img, WP_W, WP_H)
print(f"\nWallpaper converted: {len(wp_cells)} cells")

# ── Generate icons_hb.h ────────────────────────────────────────────────────────
ICONS_HB_PATH = '/home/ramim/Projects/Software/ritos/assets/icons/icons_hb.h'

# Map icon names to C identifiers and labels
ICON_MAP = [
    ('monitor',    'HB_SYSMON',    'SysMon'),
    ('calculator', 'HB_CALC',      'Calc'),
    ('editor',     'HB_EDITOR',    'Editor'),
    ('files',      'HB_FILES',     'Files'),
    ('clock',      'HB_CLOCK',     'Clock'),
    ('calendar',   'HB_CAL',       'Calen'),
    ('settings',   'HB_SETTINGS',  'Config'),
    ('terminal',   'HB_TERM',      'Term'),
    ('imgview',    'HB_IMGVIEW',   'ImgVw'),
]

def format_byte_array(values, per_row=6):
    """Format a list of ints as C initializer rows."""
    lines = []
    for i in range(0, len(values), per_row):
        chunk = values[i:i+per_row]
        lines.append('    ' + ','.join(f'0x{v:02X}' for v in chunk))
    return ',\n'.join(lines)

def format_char_array(values, per_row=6):
    """Format a list of char codes as C initializer rows (escaped)."""
    lines = []
    for i in range(0, len(values), per_row):
        chunk = values[i:i+per_row]
        items = []
        for v in chunk:
            if v == 0x20:
                items.append("'\\x20'")
            elif 0x20 < v < 0x7F and v != ord("'") and v != ord('\\'):
                items.append(f"'\\x{v:02X}'")
            else:
                items.append(f"'\\x{v:02X}'")
        lines.append('    ' + ','.join(items))
    return ',\n'.join(lines)

with open(ICONS_HB_PATH, 'w') as f:
    f.write('#pragma once\n')
    f.write('#include <stdint.h>\n\n')
    f.write('// Half-block icon data generated by convert_assets.py\n')
    f.write('// Each icon: 6 cells wide x 4 cells tall = 24 cells\n\n')
    f.write('struct HalfBlockIcon {\n')
    f.write('    const char* chars;      // 24 char codes (6x4)\n')
    f.write('    const uint8_t* fg;      // 24 foreground color indices\n')
    f.write('    const uint8_t* bg;      // 24 background color indices\n')
    f.write('    const char* label;\n')
    f.write('};\n\n')

    for name, cid, label in ICON_MAP:
        cells = icon_cells.get(name)
        if cells is None:
            print(f"  WARNING: no cells for '{name}', skipping")
            continue
        chars = [c[0] for c in cells]
        fgs   = [c[1] for c in cells]
        bgs   = [c[2] for c in cells]

        f.write(f'// {name.upper()} ({label})\n')
        f.write(f'static const char {cid}_CHARS[24] = {{\n')
        f.write(format_char_array(chars))
        f.write('\n};\n')
        f.write(f'static const uint8_t {cid}_FG[24] = {{\n')
        f.write(format_byte_array(fgs))
        f.write('\n};\n')
        f.write(f'static const uint8_t {cid}_BG[24] = {{\n')
        f.write(format_byte_array(bgs))
        f.write('\n};\n\n')

print(f"\nWrote {ICONS_HB_PATH}")

# ── Generate wallpaper_hb.h ───────────────────────────────────────────────────
WP_HB_PATH = '/home/ramim/Projects/Software/ritos/assets/wallpapers/wallpaper_hb.h'

total = WP_W * WP_H
wp_chars = [c[0] for c in wp_cells]
wp_fgs   = [c[1] for c in wp_cells]
wp_bgs   = [c[2] for c in wp_cells]

with open(WP_HB_PATH, 'w') as f:
    f.write('#pragma once\n')
    f.write('#include <stdint.h>\n\n')
    f.write('// Half-block wallpaper data generated by convert_assets.py\n')
    f.write(f'#define WP_COLS {WP_W}\n')
    f.write(f'#define WP_ROWS {WP_H}\n\n')
    f.write(f'static const char WP_CHARS[{total}] = {{\n')
    f.write(format_char_array(wp_chars, per_row=WP_W))
    f.write('\n};\n\n')
    f.write(f'static const uint8_t WP_FG[{total}] = {{\n')
    f.write(format_byte_array(wp_fgs, per_row=WP_W))
    f.write('\n};\n\n')
    f.write(f'static const uint8_t WP_BG[{total}] = {{\n')
    f.write(format_byte_array(wp_bgs, per_row=WP_W))
    f.write('\n};\n')

print(f"Wrote {WP_HB_PATH}")

# ── Verify headers parse as Python (basic syntax check) ───────────────────────
print("\n--- Verifying generated header files ---")
for path in [ICONS_HB_PATH, WP_HB_PATH]:
    with open(path) as f:
        content = f.read()
    # Count braces
    opens = content.count('{')
    closes = content.count('}')
    if opens == closes:
        print(f"  {os.path.basename(path)}: OK ({opens} matching braces, {len(content)} bytes)")
    else:
        print(f"  {os.path.basename(path)}: WARNING brace mismatch: {opens} opens vs {closes} closes")

print("\nDone!")
