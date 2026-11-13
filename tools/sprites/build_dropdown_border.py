#!/usr/bin/env python3
"""Build the dropdown window border strips from the screen frame bitmap.

The expanded dropdown list window's border is a BITMAP like the one we
draw on the background all around the screen (the outer frame: riveted,
textured, chiseled), just thinner than the screen border (Tom,
2026-11-13). The screen frame is ~8 px thick; these strips are 5 px,
derived by compressing the frame's own 8 px edge profile -- so the
border reads as the same border family, only thinner. It is NOT a
code-drawn chisel (the old 1 px lines): the border comes from the
frame's own pixels.

Derivation (from the authored background, settings-bg-v8.png):
- The frame's four edges are 8 px bands. Each is compressed to 5 px by
  sampling rows/cols [0, 2, 4, 5, 7] in outer-to-inner order, which
  keeps the chisel character (dark outer, bright ridge, dark inner)
  while thinning it.
- The strips are 256 px long, taken from the middle of each edge
  (x/y 64..319), away from the corners, so they carry the frame's
  natural rivet texture. The dropdown window is ~213 px wide and
  ~130 px tall at most, so one strip covers it; the draw code tiles
  the strip if a wider/taller window ever needs it.
- RGB is remapped to menu-palette indices with the same rule as
  tools/sprites/settings-background/png_to_bin.py (MAINMENU.LBX entry
  2, excluding the transient navy ramp 160-223 and vivid blue 253),
  so the border strips are pixel-identical (in palette indices) to
  the frame they came from.

Layout (two .bin files, 16-byte header + palette-index pixels):
- dropdown-border-h (256x10): rows 0..4 = TOP strip (frame top edge,
  row 0 = outer/top), rows 5..9 = BOTTOM strip (frame bottom edge,
  row 0 = inner/top, row 4 = outer/bottom -- stored so a top-to-bottom
  blit lands the outer edge at the window's bottom).
- dropdown-border-v (10x256): cols 0..4 = LEFT strip (frame left edge,
  col 0 = outer/left), cols 5..9 = RIGHT strip (frame right edge,
  col 0 = inner/left, col 4 = outer/right).

Usage: build_dropdown_border.py [--check] [--png <png>] [--lbx <lbx>]
  Writes data/base/sprites/dropdown-border-h/normal.bin and
  data/base/sprites/dropdown-border-v/normal.bin.
  --check verifies the outputs without rewriting them.
  Defaults: --png tools/sprites/settings-background/settings-bg-v8.png,
  --lbx the game data's mainmenu.lbx (pass --lbx explicitly).
"""
import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
DST_H = ROOT / "data/base/sprites/dropdown-border-h/normal.bin"
DST_V = ROOT / "data/base/sprites/dropdown-border-v/normal.bin"
DEFAULT_PNG = ROOT / "tools/sprites/settings-background/settings-bg-v8.png"

# Border thickness (px): thinner than the screen frame's ~8 px.
BORDER = 5
# Strip length (px): covers the widest/tallest dropdown window (~213x130).
STRIP_LEN = 256
# Frame band thickness (px) compressed into BORDER.
FRAME_BAND = 8
# Sample indices compressing FRAME_BAND rows/cols to BORDER.
SAMPLE = [0, 2, 4, 5, 7]
# Strip source range along each edge (away from the corners).
STRIP_SRC = 64

PALETTE_SIZE = 1024
REMAP_WARN_DISTANCE = 32.0
REMAP_FAIL_DISTANCE = 64.0
BITMAP_DRAW_MODE = 1
BITMAP_FRAME_DATA_OFFSET = 16
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
    """Menu palette: MAINMENU.LBX entry 2 at offset 24, 256x {changed,r,g,b} 6-bit."""
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


def compress_band(band, palette):
    """Compress an 8-row (or 8-col) band to BORDER rows (or cols).

    `band` is a list of FRAME_BAND rows, each a list of STRIP_LEN RGB
    pixels, in the order the strip is drawn (top-to-bottom for a
    horizontal strip, left-to-right for a vertical one). Returns a
    list of BORDER rows of palette indices.
    """
    out = []
    for si in SAMPLE:
        row = band[si]
        idx_row = []
        for rgb in row:
            target, dist = nearest_index(rgb, palette)
            if dist > REMAP_FAIL_DISTANCE:
                raise ValueError(
                    f"RGB {rgb} maps to menu {target} at distance "
                    f"{dist:.1f} (> {REMAP_FAIL_DISTANCE})")
            idx_row.append(target)
        out.append(idx_row)
    return out


def to_bin(w, h, grid):
    """Serialise a grid of palette indices to the .bin format."""
    header = struct.pack("<hhhhBBBBI", w, h, 0, 1, 0, 0, 0,
                         BITMAP_DRAW_MODE, BITMAP_FRAME_DATA_OFFSET)
    assert len(header) == 16
    pixels = bytes(px for row in grid for px in row)
    assert len(pixels) == w * h
    blob = header + pixels
    pad = (-len(blob)) % 4
    return blob + b"\x00" * pad


def main():
    check = "--check" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--check"]
    png_path = DEFAULT_PNG
    lbx_path = None
    i = 0
    while i < len(args):
        if args[i] == "--png":
            png_path = Path(args[i + 1])
            i += 2
        elif args[i] == "--lbx":
            lbx_path = Path(args[i + 1])
            i += 2
        else:
            print(f"unknown argument: {args[i]}")
            return 2
    if lbx_path is None:
        print("pass --lbx <mainmenu.lbx> (game data)")
        return 2

    palette = load_menu_palette(lbx_path)
    img = Image.open(png_path).convert("RGB")
    w, h = img.size
    assert (w, h) == (640, 480), f"expected 640x480 background, got {w}x{h}"

    # Top strip: frame rows 0..7, cols STRIP_SRC..STRIP_SRC+STRIP_LEN-1.
    # Row order is already outer (row 0) to inner (row 7).
    top_band = [[img.getpixel((x, y)) for x in range(STRIP_SRC, STRIP_SRC + STRIP_LEN)]
                for y in range(FRAME_BAND)]
    top = compress_band(top_band, palette)
    # Bottom strip: frame rows 472..479. Stored inner (row 472) to
    # outer (row 479), so a top-to-bottom blit lands the outer edge
    # at the window's bottom.
    bot_band = [[img.getpixel((x, y)) for x in range(STRIP_SRC, STRIP_SRC + STRIP_LEN)]
                for y in range(h - FRAME_BAND, h)]
    bottom = compress_band(bot_band, palette)
    # Left strip: frame cols 0..7, rows STRIP_SRC..STRIP_SRC+STRIP_LEN-1.
    # Stored as 5 columns x STRIP_LEN rows; build it column by column.
    left_cols = []
    for ci in SAMPLE:
        col = [img.getpixel((ci, y)) for y in range(STRIP_SRC, STRIP_SRC + STRIP_LEN)]
        left_cols.append([nearest_index(rgb, palette)[0] for rgb in col])
    # Right strip: frame cols 632..639. Stored inner (col 632) to
    # outer (col 639), so a left-to-right blit lands the outer edge
    # at the window's right.
    right_cols = []
    for ci in [w - FRAME_BAND + s for s in SAMPLE]:
        col = [img.getpixel((ci, y)) for y in range(STRIP_SRC, STRIP_SRC + STRIP_LEN)]
        right_cols.append([nearest_index(rgb, palette)[0] for rgb in col])

    # Horizontal sprite: 256 wide x 10 tall (top rows 0..4, bottom 5..9).
    grid_h = top + bottom
    blob_h = to_bin(STRIP_LEN, 2 * BORDER, grid_h)
    # Vertical sprite: 10 wide x 256 tall (left cols 0..4, right 5..9).
    grid_v = [[left_cols[c][y] for c in range(BORDER)] +
              [right_cols[c][y] for c in range(BORDER)]
              for y in range(STRIP_LEN)]
    blob_v = to_bin(2 * BORDER, STRIP_LEN, grid_v)

    if check:
        for dst, blob, name in ((DST_H, blob_h, "dropdown-border-h"),
                                (DST_V, blob_v, "dropdown-border-v")):
            cur = dst.read_bytes()
            assert cur == blob, (
                f"{dst} differs from built output; re-run without --check")
            print(f"{name}: OK (matches built output)")
        return 0

    for dst, blob in ((DST_H, blob_h), (DST_V, blob_v)):
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(blob)
        print(f"wrote {dst}: {len(blob)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
