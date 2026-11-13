#!/usr/bin/env python3
"""Convert an authored RGB settings-background PNG to a native orion2re
bitmap (.bin) in menu-palette indices.

Reads the menu palette from mainmenu.lbx entry 2 (offset 24) -- the
palette the settings screen renders with. Remaps each RGB colour to the
nearest menu-palette index (Euclidean distance in 8-bit RGB), EXCLUDING
indices 160-223 (the transient navy ramp installed by
SETTINGS_BACKGROUND::Install_Background) and 253 (the vivid blue slot),
which the background install overwrites at runtime. Fails if any colour
maps at distance > 64 (same threshold as tools/extract_sprite.py);
warns if > 32.

Output is the in-memory layout bitmap::Create_Bitmap_Header produces:
12-byte s_animation_header (one frame, draw mode BITMAP), one frame
offset of 16, then width*height palette-index bytes, padded to a
multiple of 4. For 640x480 the file is 307,216 bytes.

Usage:
  png_to_bin.py <input.png> <mainmenu.lbx> <output.bin>
"""
import struct
import sys
from pathlib import Path

from PIL import Image

PALETTE_SIZE = 1024
REMAP_WARN_DISTANCE = 32.0
REMAP_FAIL_DISTANCE = 64.0
BITMAP_DRAW_MODE = 1
BITMAP_FRAME_DATA_OFFSET = 16

# Indices overwritten at runtime by SETTINGS_BACKGROUND::Install_Background:
# the 64-entry navy ramp (160-223) and the vivid-blue slot (253).
EXCLUDED = set(range(160, 224)) | {253}


def load_lbx(path):
    data = Path(path).read_bytes()
    entry_count, magic = struct.unpack_from("<HH", data, 0)
    if magic != 0xFEAD:
        raise ValueError(f"{path}: not an LBX file")
    offsets = struct.unpack_from("<%dI" % (entry_count + 1), data, 8)
    return data, offsets


def lbx_entry(data, offsets, index):
    if index < 0 or index >= len(offsets) - 1:
        raise ValueError(f"LBX entry {index} out of range")
    return data[offsets[index]:offsets[index + 1]]


def load_menu_palette(lbx_path):
    """Menu palette: MAINMENU.LBX entry 2 at offset 24 (after the 12-byte
    header and the frame table). 256 records {changed, r, g, b}, 6-bit."""
    data, offsets = load_lbx(lbx_path)
    payload = lbx_entry(data, offsets, 2)
    raw = payload[24:24 + PALETTE_SIZE]
    if len(raw) < PALETTE_SIZE:
        raise ValueError("menu palette is short")
    return [(raw[4 * i + 1] * 4, raw[4 * i + 2] * 4, raw[4 * i + 3] * 4)
            for i in range(256)]


def nearest_index(rgb, palette):
    best, best_dist_sq = None, None
    for index, candidate in enumerate(palette):
        if index in EXCLUDED:
            continue
        dist_sq = sum((a - b) ** 2 for a, b in zip(rgb, candidate))
        if best_dist_sq is None or dist_sq < best_dist_sq:
            best, best_dist_sq = index, dist_sq
    return best, best_dist_sq ** 0.5


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(2)
    png_path, lbx_path, out_path = sys.argv[1:4]
    palette = load_menu_palette(lbx_path)
    img = Image.open(png_path).convert('RGB')
    w, h = img.size
    pixels = list(img.getdata())
    unique = sorted(set(pixels))
    print(f"{w}x{h}, {len(unique)} unique RGB colours")
    mapping = {}
    for rgb in unique:
        target, dist = nearest_index(rgb, palette)
        if dist > REMAP_FAIL_DISTANCE:
            raise ValueError(
                f"RGB {rgb} maps to menu {target} {palette[target]} at "
                f"distance {dist:.1f} (> {REMAP_FAIL_DISTANCE}); no close "
                f"menu colour")
        flag = "  <-- WARN >32" if dist > REMAP_WARN_DISTANCE else ""
        print(f"  RGB {rgb} -> menu {target} {palette[target]} "
              f"dist {dist:.1f}{flag}")
        mapping[rgb] = target
    indices = bytes(mapping[px] for px in pixels)
    # Header: width, height, current_frame, frame_count (int16 LE),
    # loop_frame, keyframe_interval, unknown_0xa, flags (int8),
    # then one uint32 frame offset (=16).
    header = struct.pack("<hhhhBBBBI", w, h, 0, 1, 0, 0, 0,
                         BITMAP_DRAW_MODE, BITMAP_FRAME_DATA_OFFSET)
    assert len(header) == 16
    blob = header + indices
    # Pad to a multiple of 4.
    pad = (-len(blob)) % 4
    blob += b"\x00" * pad
    Path(out_path).write_bytes(blob)
    print(f"wrote {out_path}: {len(blob)} bytes "
          f"(expected {16 + w * h})")


if __name__ == '__main__':
    main()
