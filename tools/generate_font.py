#!/usr/bin/env python3
"""Generates the 8x16 CP437 bitmap font header for RitOS framebuffer rendering.

Glyphs are sourced from a system VGA console font (PSF format) by Unicode
codepoint; box-drawing and block characters missing from the PSF are drawn
procedurally so the full CP437 range is always covered.
"""
import gzip, os, struct, sys

CHAR_W, CHAR_H = 8, 16

PSF_CANDIDATES = [
    "/usr/share/consolefonts/Uni1-VGA16.psf.gz",
    "/usr/share/consolefonts/Uni2-VGA16.psf.gz",
    "/usr/share/consolefonts/Lat15-VGA16.psf.gz",
    "/usr/share/consolefonts/Uni1-Fixed16.psf.gz",
]

# CP437 codepoints for 0x00-0x1F and 0x7F (Python's cp437 codec keeps these
# as control characters, but the IBM PC charset assigns them glyphs)
CP437_LOW = [
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
]
CP437_DEL = 0x2302  # house


def cp437_unicode(code):
    if code < 0x20:
        return CP437_LOW[code]
    if code == 0x7F:
        return CP437_DEL
    return ord(bytes([code]).decode('cp437'))


def load_psf(path):
    """Returns {unicode_codepoint: [16 row bytes]} or None."""
    raw = gzip.open(path, 'rb').read() if path.endswith('.gz') else open(path, 'rb').read()
    if raw[:2] == b'\x36\x04':           # PSF1
        mode, charsize = raw[2], raw[3]
        nglyphs = 512 if (mode & 0x01) else 256
        has_tab = bool(mode & 0x02)
        glyph_data = raw[4:4 + nglyphs * charsize]
        glyphs = [list(glyph_data[i*charsize:(i+1)*charsize]) for i in range(nglyphs)]
        table = {}
        if has_tab:
            pos = 4 + nglyphs * charsize
            for gi in range(nglyphs):
                while pos + 1 < len(raw):
                    cp = struct.unpack('<H', raw[pos:pos+2])[0]
                    pos += 2
                    if cp == 0xFFFF:
                        break
                    if cp != 0xFFFE and cp not in table:
                        table[cp] = glyphs[gi]
        else:
            table = {i: g for i, g in enumerate(glyphs)}
        return table if charsize == CHAR_H else None
    if raw[:4] == b'\x72\xb5\x4a\x86':   # PSF2
        (version, hdrsize, flags, nglyphs, charsize, height, width) = \
            struct.unpack('<6I', raw[8:32])[0:6] + (struct.unpack('<I', raw[28:32])[0],)
        version, hdrsize, flags, nglyphs, charsize, height = struct.unpack('<6I', raw[4:28])
        width = struct.unpack('<I', raw[28:32])[0]
        if width != CHAR_W or height != CHAR_H:
            return None
        glyph_data = raw[hdrsize:hdrsize + nglyphs * charsize]
        glyphs = [list(glyph_data[i*charsize:(i+1)*charsize]) for i in range(nglyphs)]
        table = {}
        if flags & 0x01:
            pos = hdrsize + nglyphs * charsize
            gi = 0
            entry = b''
            while pos < len(raw) and gi < nglyphs:
                b = raw[pos]; pos += 1
                if b == 0xFF:
                    s = entry.decode('utf-8', 'ignore')
                    for ch in s:
                        table.setdefault(ord(ch), glyphs[gi])
                    entry = b''; gi += 1
                elif b == 0xFE:
                    s = entry.decode('utf-8', 'ignore')
                    for ch in s:
                        table.setdefault(ord(ch), glyphs[gi])
                    entry = b''
                else:
                    entry += bytes([b])
        else:
            table = {i: g for i, g in enumerate(glyphs)}
        return table
    return None


# ── Procedural glyphs ──────────────────────────────────────────────────────
def blank():
    return [0x00] * CHAR_H

def full_block():
    return [0xFF] * CHAR_H

def shade(level):
    """░=1 ▒=2 ▓=3 dither patterns."""
    rows = []
    for y in range(CHAR_H):
        if level == 1:
            rows.append(0x88 if y % 2 == 0 else 0x22)
        elif level == 2:
            rows.append(0xAA if y % 2 == 0 else 0x55)
        else:
            rows.append(0xEE if y % 2 == 0 else 0xBB)
    return rows

def half_block(top):
    return [0xFF]*8 + [0x00]*8 if top else [0x00]*8 + [0xFF]*8

# Box drawing: single line uses col 4 / row 8; double uses cols 3,5 / rows 7,9
V1, V2A, V2B = 0x08, 0x10, 0x04       # column masks (bit7 = x0): col4, col3, col5
R1, R2A, R2B = 8, 7, 9                # row indices

def box(up=0, down=0, left=0, right=0):
    """1 = single line, 2 = double line, in each direction."""
    g = blank()
    rows_h = [R1] if max(left, right) == 1 else [R2A, R2B]
    cols_v = [V1] if max(up, down) == 1 else [V2A, V2B]
    # vertical strokes
    if up:
        for m in ([V1] if up == 1 else [V2A, V2B]):
            for y in range(0, max(rows_h) + 1):
                g[y] |= m
    if down:
        for m in ([V1] if down == 1 else [V2A, V2B]):
            for y in range(min(rows_h), CHAR_H):
                g[y] |= m
    # horizontal strokes
    def hspan(mask_lo, mask_hi):
        return mask_lo | mask_hi
    if left:
        for y in ([R1] if left == 1 else [R2A, R2B]):
            # bits x0..x4 => masks 0x80..0x08
            span = 0xF8 if not (up or down) else (0xF8 | cols_v[0])
            g[y] |= 0xF8
    if right:
        for y in ([R1] if right == 1 else [R2A, R2B]):
            g[y] |= 0x0F
    # joints: clear inner gap for double corners (keep it simple/legible)
    return g

BOX_MAP = {
    0x2500: dict(left=1, right=1),                # ─
    0x2502: dict(up=1, down=1),                   # │
    0x250C: dict(down=1, right=1),                # ┌
    0x2510: dict(down=1, left=1),                 # ┐
    0x2514: dict(up=1, right=1),                  # └
    0x2518: dict(up=1, left=1),                   # ┘
    0x251C: dict(up=1, down=1, right=1),          # ├
    0x2524: dict(up=1, down=1, left=1),           # ┤
    0x252C: dict(down=1, left=1, right=1),        # ┬
    0x2534: dict(up=1, left=1, right=1),          # ┴
    0x253C: dict(up=1, down=1, left=1, right=1),  # ┼
    0x2550: dict(left=2, right=2),                # ═
    0x2551: dict(up=2, down=2),                   # ║
    0x2554: dict(down=2, right=2),                # ╔
    0x2557: dict(down=2, left=2),                 # ╗
    0x255A: dict(up=2, right=2),                  # ╚
    0x255D: dict(up=2, left=2),                   # ╝
    0x2560: dict(up=2, down=2, right=2),          # ╠
    0x2563: dict(up=2, down=2, left=2),           # ╣
    0x2566: dict(down=2, left=2, right=2),        # ╦
    0x2569: dict(up=2, left=2, right=2),          # ╩
    0x256C: dict(up=2, down=2, left=2, right=2),  # ╬
    # mixed single/double simplified to double
    0x2555: dict(down=1, left=2), 0x2556: dict(down=2, left=1),
    0x2558: dict(up=1, right=2),  0x2559: dict(up=2, right=1),
    0x2552: dict(down=1, right=2), 0x2553: dict(down=2, right=1),
    0x255B: dict(up=1, left=2),   0x255C: dict(up=2, left=1),
    0x255E: dict(up=1, down=1, right=2), 0x255F: dict(up=2, down=2, right=1),
    0x2561: dict(up=1, down=1, left=2),  0x2562: dict(up=2, down=2, left=1),
    0x2564: dict(down=1, left=2, right=2), 0x2565: dict(down=2, left=1, right=1),
    0x2567: dict(up=1, left=2, right=2),   0x2568: dict(up=2, left=1, right=1),
    0x256A: dict(up=1, down=1, left=2, right=2),
    0x256B: dict(up=2, down=2, left=1, right=1),
}

def bits(*pattern):
    """Build glyph rows from a list of 8-char strings ('#'=on)."""
    g = blank()
    for i, line in enumerate(pattern):
        v = 0
        for x, ch in enumerate(line[:8]):
            if ch == '#':
                v |= 0x80 >> x
        g[i] = v
    return g

SYMBOLS = {
    0x2022: bits('', '', '', '', '', '..###...', '.#####..', '.#####..',
                 '.#####..', '..###...'),                                  # •
    0x25CB: bits('', '', '', '..###...', '.#...#..', '#.....#.', '#.....#.',
                 '#.....#.', '.#...#..', '..###...'),                      # ○
    0x263C: bits('', '#..#..#.', '.#.#.#..', '..###...', '.#...#..',
                 '##.#.##.', '.#...#..', '..###...', '.#.#.#..',
                 '#..#..#.'),                                              # ☼
    0x25BA: bits('', '#.......', '##......', '###.....', '####....',
                 '#####...', '####....', '###.....', '##......',
                 '#.......'),                                              # ►
    0x25C4: bits('', '......#.', '.....##.', '....###.', '...####.',
                 '..#####.', '...####.', '....###.', '.....##.',
                 '......#.'),                                              # ◄
    0x2191: bits('', '...#....', '..###...', '.#.#.#..', '#..#..#.',
                 '...#....', '...#....', '...#....', '...#....',
                 '...#....'),                                              # ↑
    0x2193: bits('', '...#....', '...#....', '...#....', '...#....',
                 '...#....', '#..#..#.', '.#.#.#..', '..###...',
                 '...#....'),                                              # ↓
    0x2192: bits('', '', '...#....', '....#...', '.....#..', '#######.',
                 '.....#..', '....#...', '...#....'),                      # →
    0x2190: bits('', '', '...#....', '..#.....', '.#......', '#######.',
                 '.#......', '..#.....', '...#....'),                      # ←
    0x2195: bits('', '...#....', '..###...', '.#.#.#..', '...#....',
                 '...#....', '...#....', '.#.#.#..', '..###...',
                 '...#....'),                                              # ↕
    0x2194: bits('', '', '..#..#..', '.#....#.', '########', '.#....#.',
                 '..#..#..'),                                              # ↔
    0x25B2: bits('', '', '...#....', '...#....', '..###...', '..###...',
                 '.#####..', '.#####..', '#######.'),                      # ▲
    0x25BC: bits('', '', '#######.', '.#####..', '.#####..', '..###...',
                 '..###...', '...#....', '...#....'),                      # ▼
    0x263A: bits('', '..####..', '.#....#.', '#.#..#.#', '#......#',
                 '#.#..#.#', '#..##..#', '.#....#.', '..####..'),          # ☺
    0x266A: bits('', '...##...', '...#.#..', '...#..#.', '...#....',
                 '...#....', '.###....', '####....', '.##.....'),          # ♪
    0x266B: bits('', '..#####.', '..#...#.', '..#...#.', '..#...#.',
                 '.##..##.', '###.###.', '.#...#..'),                      # ♫
    0x2302: bits('', '...#....', '..###...', '.#...#..', '#.....#.',
                 '#######.', '#.....#.', '#.....#.', '#######.'),          # ⌂
    0x00B7: bits('', '', '', '', '', '', '...##...', '...##...'),          # ·
    0x2219: bits('', '', '', '', '', '', '..###...', '..###...',
                 '..###...'),                                              # ∙
    0x25AC: bits('', '', '', '', '', '########', '########', '########',
                 '########'),                                              # ▬
    0x25D8: [0xFF]*5 + [0xE7, 0xC3, 0xC3, 0xC3, 0xE7] + [0xFF]*6,          # ◘
    0x25D9: [0xFF]*4 + [0xE7, 0xDB, 0xBD, 0xBD, 0xDB, 0xE7] + [0xFF]*6,    # ◙
    0x221F: bits('', '', '', '#.......', '#.......', '#.......', '#.......',
                 '#.......', '########'),                                  # ∟
    0x203C: bits('', '.#...#..', '.#...#..', '.#...#..', '.#...#..',
                 '.#...#..', '', '.#...#..', '.#...#..'),                  # ‼
    0x21A8: bits('', '...#....', '..###...', '.#####..', '...#....',
                 '...#....', '.#####..', '..###...', '...#....', '',
                 '########'),                                              # ↨
    0x2640: bits('', '..###...', '.#...#..', '.#...#..', '.#...#..',
                 '..###...', '...#....', '..###...', '...#....'),          # ♀
    0x2642: bits('', '....####', '......##', '.....#.#', '..###...',
                 '.#...#..', '.#...#..', '.#...#..', '..###...'),          # ♂
    0x2665: bits('', '', '.##..##.', '########', '########', '.######.',
                 '..####..', '...##...'),                                  # ♥
    0x2666: bits('', '...#....', '..###...', '.#####..', '#######.',
                 '.#####..', '..###...', '...#....'),                      # ♦
    0x2663: bits('', '...##...', '..####..', '...##...', '.######.',
                 '########', '...##...', '..####..'),                      # ♣
    0x2660: bits('', '...#....', '..###...', '.#####..', '#######.',
                 '#######.', '...#....', '..###...'),                      # ♠
    0x263B: [0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0xDB, 0xE7, 0x7E, 0x3C],        # ☻
}

def procedural(cp):
    if cp in BOX_MAP:
        return box(**BOX_MAP[cp])
    if cp in SYMBOLS:
        g = SYMBOLS[cp]
        return (list(g) + [0]*CHAR_H)[:CHAR_H]
    if cp == 0x2588: return full_block()      # █
    if cp == 0x2591: return shade(1)          # ░
    if cp == 0x2592: return shade(2)          # ▒
    if cp == 0x2593: return shade(3)          # ▓
    if cp == 0x2580: return half_block(True)  # ▀
    if cp == 0x2584: return half_block(False) # ▄
    if cp == 0x258C: return [0xF0]*CHAR_H     # ▌
    if cp == 0x2590: return [0x0F]*CHAR_H     # ▐
    if cp == 0x25A0: return [0]*5 + [0x7E]*7 + [0]*4   # ■
    return None


def main():
    table = None
    for p in PSF_CANDIDATES:
        if os.path.exists(p):
            try:
                table = load_psf(p)
            except Exception:
                table = None
            if table and ord('A') in table and any(table[ord('A')]):
                print(f"Font source: {p}", file=sys.stderr)
                break
            table = None
    if table is None:
        print("ERROR: no usable PSF console font found", file=sys.stderr)
        sys.exit(1)

    data, missing = [], []
    for code in range(256):
        cp = cp437_unicode(code)
        glyph = procedural(cp)          # prefer procedural for box/blocks: pixel-exact
        if glyph is None:
            glyph = table.get(cp)
        if glyph is None:
            glyph = blank()
            if code not in (0,):
                missing.append((code, cp))
        data.append(glyph)

    if missing:
        print("Note: blank glyphs for:",
              ' '.join(f'{c:02X}(U+{u:04X})' for c, u in missing), file=sys.stderr)

    os.makedirs('assets/generated', exist_ok=True)
    with open('assets/generated/font_8x16.h', 'w') as f:
        f.write('#ifndef FONT_8X16_H\n#define FONT_8X16_H\n#include <stdint.h>\n')
        f.write('static const uint8_t g_font_8x16[256][16] = {\n')
        for code, rows in enumerate(data):
            c = chr(code) if 32 <= code <= 126 else '.'
            if c in ("'", '\\'):
                c = '.'
            hexs = ', '.join(f'0x{b:02X}' for b in rows)
            f.write(f'  /* {code:3d} \'{c}\' */ {{{hexs}}},\n')
        f.write('};\n#endif\n')
    print("Generated assets/generated/font_8x16.h")


if __name__ == '__main__':
    main()
