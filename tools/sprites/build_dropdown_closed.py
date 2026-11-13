#!/usr/bin/env python3
"""Build the collapsed-dropdown button sprite from scratch.

The collapsed (closed) dropdown is a dedicated element, not a reused
value box: a 104x26 button designed as a dropdown from the start, in
the colonies/game aesthetic (menu palette), with the triangle as part
of its design -- not a triangle stamped onto the reused value-box-wide
frame.

Design (104x26, menu-palette indices):
- Outer frame: the value-box's own border language, so the button
  matches its sibling edit buttons -- dark metallic top (15) and
  bottom (233), black right edge (0), 2 px. Not the flat 134/103
  chisel: that reads lavender and flat next to the value-box's
  textured frame.
- Face: the value-box face (238), so the button matches its sibling
  edit buttons.
- Triangle zone: the rightmost 12 px (x=90..101) is a darker panel
  (233), a recessed zone that reads as the dropdown's own area. The
  down-triangle (7x4, header white 250) sits centred in it -- the
  same geometry as settings_layout::Dropdown_Triangle_Rect (right
  edge 4 px inside the frame, vertically centred), so the value text
  (already truncated to clear the zone) still clears it. White, not
  black: a dark triangle on the light face would sit where the
  widest value ("overview", ending at button x=89) runs, and the
  zone's dark face takes the white triangle the way the dark header
  plates take the white header text.
- The value text area is x=2..89; settings_layout::Dropdown_Value_Text_Width
  clears the zone (max 74 px on a 104 px button, so "overview" fits).

Usage: build_dropdown_closed.py [--check]
  Writes data/base/sprites/dropdown-closed/normal.bin.
  --check verifies the output without rewriting it.
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DST = ROOT / "data/base/sprites/dropdown-closed/normal.bin"

W, H = 104, 26

# Menu-palette indices (MAINMENU.LBX entry 2).
TOP = 15     # value-box top edge (48, 48, 44)
BOTTOM = 233  # value-box bottom edge (60, 60, 68)
RIGHT = 0    # value-box right edge (black)
FACE = 238   # value-box face (108, 108, 116)
ZONE = 233   # triangle-zone face (60, 60, 68)
WHITE = 250  # header white (220, 220, 228)

# Triangle zone: the rightmost 12 px inside the border.
ZONE_X0 = 90
# Triangle: 7 wide, 4 tall, right edge 4 px inside the frame,
# vertically centred -- Dropdown_Triangle_Rect geometry.
TRI_W, TRI_H, TRI_INSET = 7, 4, 4


def build_grid():
    """Return the 104x26 grid of menu-palette indices."""
    grid = [[FACE] * W for _ in range(H)]
    # Outer frame: 2 px, the value-box's border language -- dark
    # metallic top, medium bottom, black right edge.
    for x in range(W):
        grid[0][x] = TOP
        grid[1][x] = TOP
        grid[H - 2][x] = BOTTOM
        grid[H - 1][x] = BOTTOM
    for y in range(H):
        grid[y][0] = TOP
        grid[y][1] = TOP
        grid[y][W - 2] = RIGHT
        grid[y][W - 1] = RIGHT
    # Corners: top wins on the top rows, bottom wins on the bottom.
    grid[0][W - 2] = TOP
    grid[0][W - 1] = TOP
    grid[1][W - 2] = TOP
    grid[1][W - 1] = TOP
    grid[H - 2][0] = BOTTOM
    grid[H - 2][1] = BOTTOM
    grid[H - 1][0] = BOTTOM
    grid[H - 1][1] = BOTTOM
    # Triangle zone: darker panel at the right end, inside the frame.
    for y in range(2, H - 2):
        for x in range(ZONE_X0, W - 2):
            grid[y][x] = ZONE
    # Down-triangle, centred in the zone, header white.
    tri_x = W - TRI_INSET - TRI_W
    tri_y = (H - TRI_H) // 2
    cx = tri_x + TRI_W // 2
    for r in range(TRI_H):
        half = TRI_W // 2 - r
        for x in range(cx - half, cx + half + 1):
            grid[tri_y + r][x] = WHITE
    return grid


def to_bin(grid):
    """Serialise the grid to the .bin format (16-byte header + pixels)."""
    header = struct.pack("<hhhhBBBBI", W, H, 0, 1, 0, 0, 0, 1, 16)
    assert len(header) == 16
    pixels = bytes(px for row in grid for px in row)
    assert len(pixels) == W * H
    blob = header + pixels
    pad = (-len(blob)) % 4
    return blob + b"\x00" * pad


def main():
    check = "--check" in sys.argv
    blob = to_bin(build_grid())
    if check:
        cur = DST.read_bytes()
        assert cur == blob, (
            f"{DST} differs from built output; re-run without --check")
        print("dropdown-closed: OK (matches built output)")
        return
    DST.parent.mkdir(parents=True, exist_ok=True)
    DST.write_bytes(blob)
    print(f"wrote {DST}: {len(blob)} bytes")


if __name__ == "__main__":
    main()
