#!/usr/bin/env python3
"""Stretch an orion2re-owned sprite frame wider by repeating one column.

Some of the colony's frames are horizontally uniform except for their
edges: the sort-tab frames carry a flat dark-navy middle (their top
and bottom chrome rows are uniform across the middle columns), and
the blank RETURN frame's face is flat in the middle. Such a frame
stretches exactly -- pixel-for-pixel colonies art -- by repeating a
single column that is identical to its neighbour in every row
(a period-1 column). The tool asserts that: the repeat column must
equal the next column in all rows, otherwise it fails loudly instead
of shipping a visible seam.

Usage:
  tools/stretch_sprite.py --in <sprite.bin> --column <c> --repeat <n> \\
      --out <out.bin>
  tools/stretch_sprite.py --check --in <sprite.bin> --column <c> \\
      --repeat <n> --out <out.bin>

--check also byte-compares the output against the existing file.
The .bin format is the one tools/extract_sprite.py writes (16-byte
header, frame offset 16, width*height palette-index bytes).
"""
import argparse
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
import extract_sprite  # noqa: E402


def read_bin(path):
    """Read a .bin frame: returns (width, height, grid of indices)."""
    data = Path(path).read_bytes()
    width, height = struct.unpack_from("<hh", data, 0)
    if width <= 0 or height <= 0:
        raise ValueError(f"{path}: bad size {width}x{height}")
    pixels = data[16:16 + width * height]
    if len(pixels) != width * height:
        raise ValueError(
            f"{path}: short pixel data ({len(pixels)} != {width * height})")
    grid = [list(pixels[y * width:(y + 1) * width]) for y in range(height)]
    return width, height, grid


def stretch(grid, column, repeat):
    """Return the grid widened by repeating `column` `repeat` times.

    Asserts the column is period-1 (identical to its right neighbour
    in every row), so the repeat is seamless.
    """
    height = len(grid)
    width = len(grid[0])
    if not 0 <= column < width - 1:
        raise ValueError(
            f"column {column} out of range for width {width}")
    for y in range(height):
        if grid[y][column] != grid[y][column + 1]:
            raise ValueError(
                f"column {column} != column {column + 1} at row {y} "
                f"({grid[y][column]} vs {grid[y][column + 1]}): "
                f"repeating it would leave a seam")
    out = []
    for y in range(height):
        row = grid[y]
        out.append(row[:column + 1] + [row[column]] * repeat +
                   row[column + 1:])
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--in", dest="src", required=True,
                        help="source .bin frame")
    parser.add_argument("--column", type=int, required=True,
                        help="column to repeat (must equal its neighbour)")
    parser.add_argument("--repeat", type=int, required=True,
                        help="how many extra copies of the column")
    parser.add_argument("--out", required=True, help="output .bin frame")
    parser.add_argument("--check", action="store_true",
                        help="byte-compare the output against --out "
                             "if it already exists")
    args = parser.parse_args(argv)

    width, height, grid = read_bin(args.src)
    new_grid = stretch(grid, args.column, args.repeat)
    new_width = width + args.repeat
    encoded = extract_sprite.frame_bytes(new_grid)
    out_path = Path(args.out)
    if args.check and out_path.exists():
        existing = out_path.read_bytes()
        if existing != encoded:
            print(f"FAIL {args.out}: differs from stretched "
                  f"{args.src} (column {args.column} x{args.repeat})",
                  file=sys.stderr)
            raise SystemExit(1)
        print(f"PASS {args.out}: {new_width}x{height} matches "
              f"stretched {args.src}")
        return 0
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(encoded)
    print(f"wrote {args.out}: {width}x{height} -> {new_width}x{height} "
          f"(column {args.column} repeated {args.repeat} times)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
