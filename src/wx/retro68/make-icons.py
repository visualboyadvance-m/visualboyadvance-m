#!/usr/bin/env python3
"""Make the classic Mac OS icon resources of the Retro68 build.

Reads the VBA-M application icon PNGs (src/wx/icons/sizes/*/apps) and writes
a Rez source with the icon family the Finder of System 7 to Mac OS 9 shows:

  ICN#, icl8, icl4  32x32: 1-bit icon and mask, 8- and 4-bit colour
  ics#, ics8, ics4  16x16: the same, small
  icns              Mac OS 8.5 and later: 32-bit is32/il32/it32 (16, 32 and
                    128 pixels) with their 8-bit alpha masks s8mk/l8mk/t8mk

The colour icons use the Mac OS system palettes (clut 8 and clut 4), which is
what the Finder draws icl8/icl4 with (the 16-colour ones error-diffused); the
black-and-white one is the usual outline with the dark parts filled in.
Pixels that are partly transparent are composited over white first, as the
Finder would show them on the desktop.

Usage: make-icons.py [output.r]  (default: icons.r next to this script)

No third-party modules: PNG decoding is done here (8-bit RGBA only, which is
what the icon PNGs are).
"""

import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = os.path.join(HERE, "..", "icons", "sizes")


def read_png_rgba(path):
    """Returns (width, height, rows of (r, g, b, a) tuples)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    idat = b""
    width = height = None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype != 6 or interlace != 0:
                raise ValueError(f"{path}: only 8-bit non-interlaced RGBA is supported")
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break

    raw = zlib.decompress(idat)
    bpp = 4
    stride = width * bpp
    rows = []
    prev = bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        ftype = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
        prev = line
    return width, height, rows


def over_white(px):
    r, g, b, a = px
    return tuple((c * a + 255 * (255 - a)) // 255 for c in (r, g, b))


def mac_palette_8():
    """The Mac OS system palette (clut 8)."""
    levels = [0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00]
    pal = []
    for r in levels:
        for g in levels:
            for b in levels:
                pal.append((r, g, b))
    pal = pal[:215]  # the cube without black, which is the last entry
    ramp = [0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11]
    pal += [(v, 0, 0) for v in ramp]
    pal += [(0, v, 0) for v in ramp]
    pal += [(0, 0, v) for v in ramp]
    pal += [(v, v, v) for v in ramp]
    pal.append((0, 0, 0))
    assert len(pal) == 256
    return pal


# The Mac OS 16-colour system palette (clut 4).
MAC_PALETTE_4 = [
    (0xFF, 0xFF, 0xFF), (0xFC, 0xF3, 0x05), (0xFF, 0x64, 0x02), (0xDD, 0x08, 0x06),
    (0xF2, 0x08, 0x84), (0x46, 0x00, 0xA5), (0x00, 0x00, 0xD4), (0x02, 0xAB, 0xEA),
    (0x1F, 0xB7, 0x14), (0x00, 0x64, 0x11), (0x56, 0x2C, 0x05), (0x90, 0x71, 0x3A),
    (0xC0, 0xC0, 0xC0), (0x80, 0x80, 0x80), (0x40, 0x40, 0x40), (0x00, 0x00, 0x00),
]


def nearest(pal, rgb):
    r, g, b = rgb
    best, best_d = 0, None
    for i, (pr, pg, pb) in enumerate(pal):
        d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if best_d is None or d < best_d:
            best, best_d = i, d
    return best


def mask_bits(rows):
    return [[1 if px[3] >= 128 else 0 for px in row] for row in rows]


def one_bit(rows):
    """1-bit icon, the way black-and-white icons are drawn: a black outline
    along the edge of the mask and the darker half of the picture inside it."""
    h, w = len(rows), len(rows[0])
    mask = mask_bits(rows)
    bits = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if not mask[y][x]:
                continue
            edge = any(not (0 <= x + dx < w and 0 <= y + dy < h and mask[y + dy][x + dx])
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            r, g, b = over_white(rows[y][x])
            dark = 0.299 * r + 0.587 * g + 0.114 * b < 96
            bits[y][x] = 1 if edge or dark else 0
    return bits


def pack_bits(bits):
    out = bytearray()
    for row in bits:
        for x in range(0, len(row), 8):
            byte = 0
            for i in range(8):
                byte = (byte << 1) | row[x + i]
            out.append(byte)
    return bytes(out)


def indexed(rows, pal, bits_per_pixel):
    """Palette indices; for clut 4 error-diffused (Floyd-Steinberg) inside the
    mask, so its 16 colours still give the icon's tints."""
    h, w = len(rows), len(rows[0])
    work = [[list(map(float, over_white(px))) for px in row] for row in rows]
    idx = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if rows[y][x][3] < 128:
                continue
            want = [min(255.0, max(0.0, c)) for c in work[y][x]]
            i = nearest(pal, want)
            idx[y][x] = i
            if bits_per_pixel == 8:
                continue  # clut 8 is close enough as it is; dithering only adds noise
            err = [want[c] - pal[i][c] for c in range(3)]
            for dx, dy, f in ((1, 0, 7 / 16), (-1, 1, 3 / 16), (0, 1, 5 / 16), (1, 1, 1 / 16)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and ny < h and rows[ny][nx][3] >= 128:
                    for c in range(3):
                        work[ny][nx][c] += err[c] * f
    out = bytearray()
    for row in idx:
        if bits_per_pixel == 8:
            out += bytes(row)
        else:
            for x in range(0, len(row), 2):
                out.append((row[x] << 4) | row[x + 1])
    return bytes(out)


def icns_rle(channel):
    """The icns RGB run-length coding: n < 0x80 is n + 1 literal bytes, n >= 0x80
    a run of n - 0x80 + 3 copies of the next byte."""
    out = bytearray()
    i, n = 0, len(channel)
    while i < n:
        run = 1
        while i + run < n and run < 130 and channel[i + run] == channel[i]:
            run += 1
        if run >= 3:
            out += bytes((0x80 + run - 3, channel[i]))
            i += run
            continue
        start = i
        while i < n and i - start < 128:
            if i + 2 < n and channel[i] == channel[i + 1] == channel[i + 2]:
                break
            i += 1
        out.append(i - start - 1)
        out += channel[start:i]
    return bytes(out)


def icns_rgb(rows, prefix=b""):
    pixels = [px for row in rows for px in row]
    data = bytearray(prefix)
    for c in range(3):
        data += icns_rle(bytes(px[c] for px in pixels))
    return bytes(data)


def icns_mask(rows):
    return bytes(px[3] for row in rows for px in row)


def icns(elements):
    body = b"".join(kind + struct.pack(">I", 8 + len(data)) + data for kind, data in elements)
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


def rez_data(kind, rid, name, data):
    lines = [f"data '{kind}' ({rid}, \"{name}\") {{"]
    for i in range(0, len(data), 32):
        lines.append('    $"' + data[i:i + 32].hex().upper() + '"')
    lines.append("};")
    return "\n".join(lines)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "icons.r")

    def load(size):
        w, h, rows = read_png_rgba(os.path.join(ICONS, f"{size}x{size}", "apps", "visualboyadvance-m.png"))
        assert (w, h) == (size, size)
        return rows

    small, large, thumb = load(16), load(32), load(128)
    pal8 = mac_palette_8()

    resources = [
        ("ICN#", pack_bits(one_bit(large)) + pack_bits(mask_bits(large))),
        ("icl8", indexed(large, pal8, 8)),
        ("icl4", indexed(large, MAC_PALETTE_4, 4)),
        ("ics#", pack_bits(one_bit(small)) + pack_bits(mask_bits(small))),
        ("ics8", indexed(small, pal8, 8)),
        ("ics4", indexed(small, MAC_PALETTE_4, 4)),
        ("icns", icns([
            (b"is32", icns_rgb(small)), (b"s8mk", icns_mask(small)),
            (b"il32", icns_rgb(large)), (b"l8mk", icns_mask(large)),
            (b"it32", icns_rgb(thumb, prefix=b"\0\0\0\0")), (b"t8mk", icns_mask(thumb)),
        ])),
    ]

    with open(out_path, "w", newline="\n") as f:
        f.write("/* The VisualBoyAdvance-M application icon for classic Mac OS.\n"
                " * Generated by make-icons.py from src/wx/icons/sizes; do not edit. */\n\n")
        for kind, data in resources:
            f.write(rez_data(kind, 128, "VisualBoyAdvance-M", data) + "\n\n")


if __name__ == "__main__":
    main()
