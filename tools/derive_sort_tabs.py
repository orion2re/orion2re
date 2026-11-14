#!/usr/bin/env python3
"""Derive the colony sort-tab frames from the rendered colony screen.

The settings screen's three category tabs each draw their own
colony sort tab's frame (Name, Population, Food -- COLSUM
anims[4..10], the first three), grabbed from the rendered colony
screen, not extracted from the LBX: the screengrab is the source,
so the frames are the colony's own pixels. The derivation:

- takes the colony summary screen as rendered (three screenshots:
  the default sort, the Population sort, the Food sort -- the
  cursor parked off the strip in all of them, so no cursor sits
  on a tab),
- cuts each tab's 24 px band out of its shot (game coordinates:
  Name x 89..138, Population x 140..217, Food x 219..260, all
  y 446..469),
- blanks the baked label per state (normal: flat fill of the
  dark-navy interior, rows 4..19, cols 4..; active: column-copy
  from label-free row 5, rows 7..17, cols 4..) -- the same blank
  rects tools/extract_sprite.py used for the retired
  selector-frame asset,
- remaps the colony palette (COLSUM.LBX entry 0's embedded
  palette) to the menu palette (MAINMENU.LBX entry 2, offset 24)
  by nearest RGB, with the active state's blue-ramp overrides
  (the menu palette has no close hue for the colony blues),
- writes data/base/sprites/sort-{name,population,food}/
  {normal,active}.bin, and
- measures the window-band background the gaps between tabs show
  (one palette index per tab row), printed for ui_kit.cpp's
  kSort_Gap_Profiles.

The script fails loudly on any unmapped RGB, any label pixel
outside the blank rects, or any gap disagreement (rows 1..23
must agree across all five colony gaps; row 0 follows the
strip-wide arch and is printed per gap). With --cross-check it
also verifies the screen-derived Name frames against a pair of
reference bins (the retired LBX-derived selector-frame): the
one-time run of that check passed byte-identical, which is what
proved the screen pipeline faithful.

Usage:
  tools/derive_sort_tabs.py --game <game data dir> \
      --grabs <colony screenshots dir> [--out data/base/sprites] \
      [--cross-check <dir with normal.bin/active.bin>]
The game data dir holds COLSUM.LBX and MAINMENU.LBX; the grabs
dir holds colony-grab-default.png, colony-grab-population.png and
colony-grab-food.png (the colony summary screen with each sort
column selected, game pixels 640x480, the window's content at
its native position).
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import extract_sprite  # noqa: E402

# (sprite id, colony tab, game x, width, normal shot, active shot)
# Normal: the tab is inactive in the shot named here (the cursor is
# parked off the strip in every shot). Active: the shot where that
# tab is the selected sort column.
FRAMES = [
    ("sort-name", "Name", 89, 50, "population", "default"),
    ("sort-population", "Population", 140, 78, "default", "population"),
    ("sort-food", "Food", 219, 42, "default", "food"),
]
# The five 1 px gaps between the colony's seven sort tabs, game x.
GAPS = [139, 218, 261, 325, 392]
Y = 446  # sort strip top, game pixels
H = 24
# The active state's blue ramp has no close hue in the menu
# palette: colony blues 50-56 and 229 override onto the menu's own
# blues, exactly as tools/extract_sprite.py did for selector-frame.
ACTIVE_OVERRIDES = {
    50: 50, 51: 103, 52: 103, 53: 138, 54: 138, 55: 138, 56: 138,
    57: 50, 229: 138,
}

FAILURES = []


def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name +
          (" -- " + str(detail) if detail else ""))
    if not ok:
        FAILURES.append(name)


def label_bbox(rgb_grid):
    # Bright span over the label rows only (frame rows 7..17): the
    # bevel corners at rows 19..20 are chrome, not label, and the
    # left bevel's bright ridge sits at cols < 4.
    xs = [cx for ry in range(7, 18) for cx in range(len(rgb_grid[0]))
          if sum(rgb_grid[ry][cx]) > 450]
    return (min(xs), max(xs)) if xs else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True,
                        help="game data dir (COLSUM.LBX, MAINMENU.LBX)")
    parser.add_argument("--grabs", required=True,
                        help="colony screenshots dir (default/population/food.png)")
    parser.add_argument("--out", default=None,
                        help="output dir (default: <repo>/data/base/sprites)")
    parser.add_argument("--cross-check", default=None,
                        help="dir with reference normal.bin/active.bin "
                             "to compare the Name frames against")
    parser.add_argument("--origin", default=None, type=lambda s: tuple(
        int(v) for v in s.split(",")),
        help="game window origin in the shots as X,Y (default: "
             "auto-detect from the non-black content bbox)")
    args = parser.parse_args()

    if args.out is None:
        args.out = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "data", "base", "sprites")
    os.makedirs(args.out, exist_ok=True)

    colony_pal = extract_sprite.load_palette(
        {"COLSUM.LBX": os.path.join(args.game, "colsum.lbx")},
        {"embedded": True, "entry": 0, "lbx": "COLSUM.LBX"})
    menu_pal = extract_sprite.load_palette(
        {"MAINMENU.LBX": os.path.join(args.game, "mainmenu.lbx")},
        {"entry": 2, "lbx": "MAINMENU.LBX", "offset": 24})
    print("colony palette 57=%s 50=%s 53=%s" %
          (colony_pal[57], colony_pal[50], colony_pal[53]))
    print("menu palette 50=%s 103=%s 138=%s 239=%s" %
          (menu_pal[50], menu_pal[103], menu_pal[138], menu_pal[239]))

    # The shot's pixel -> colony index, by exact RGB match against
    # the colony palette. The shot is the rendered screen: every
    # pixel must be a palette colour, or the palette is wrong.
    def rgb_to_colony(rgb):
        for idx, prgb in enumerate(colony_pal):
            if prgb == rgb:
                return idx
        raise SystemExit("unmapped RGB %s -- not a colony palette colour"
                         % (rgb,))

    def shot_grid(name):
        from PIL import Image
        for candidate in ("colony-grab-%s.png" % name, name + ".png"):
            path = os.path.join(args.grabs, candidate)
            if os.path.exists(path):
                break
        else:
            raise SystemExit("no shot for %s in %s" % (name, args.grabs))
        img = Image.open(path).convert("RGB")
        # The game window sits centred in the Xvfb screen (1280x1024
        # here): the window's content is the non-black bbox. Derive
        # the origin from the shot, don't assume it -- a different
        # Xvfb size recentres the window.
        if args.origin is not None:
            ox, oy = args.origin
        else:
            bbox = img.convert("L").point(lambda p: 255 if p else 0).getbbox()
            if bbox is None:
                raise SystemExit("%s: shot is all black" % path)
            ox, oy, x1, y1 = bbox
            if (x1 - ox, y1 - oy) != (640, 480):
                raise SystemExit(
                    "%s: content bbox %s is not 640x480 -- not the colony screen"
                    % (path, bbox))
            print("  %s: game window origin (%d, %d)" % (name, ox, oy))
        return [[img.getpixel((ox + x, oy + y)) for x in range(640)]
                for y in range(480)]

    # The shots are 640x480 game pixels (the window's content at
    # its native position); the strip is at game y 446..469.
    shots = {name: shot_grid(name) for name in
             ("default", "population", "food")}

    for sprite, tab, x0, width, normal_shot, active_shot in FRAMES:
        for state, shot_name, overrides in (
                ("normal", normal_shot, None),
                ("active", active_shot, ACTIVE_OVERRIDES)):
            grid = [[rgb_to_colony(shots[shot_name][Y + ry][x0 + cx])
                     for cx in range(width)] for ry in range(H)]
            rgb_grid = [[colony_pal[idx] for idx in row] for row in grid]
            bbox = label_bbox(rgb_grid)
            print("%s/%s from %s: label bright x %s" %
                  (sprite, state, shot_name, bbox))
            check("%s %s label inside blank cols" % (tab, state),
                  bbox is not None and bbox[0] >= 4 and bbox[1] < width,
                  bbox)
            if state == "normal":
                for ry in range(4, 20):
                    for cx in range(4, width):
                        grid[ry][cx] = 57
                remapped, mapping = extract_sprite.remap_grid(
                    grid, colony_pal, menu_pal, {})
            else:
                for ry in range(7, 18):
                    for cx in range(4, width):
                        grid[ry][cx] = grid[5][cx]
                remapped, mapping = extract_sprite.remap_grid(
                    grid, colony_pal, menu_pal,
                    {str(k): v for k, v in ACTIVE_OVERRIDES.items()})
            print("  %s/%s mapping:" % (sprite, state))
            for src in sorted(mapping):
                dst, dist = mapping[src]
                mark = "  <-- override" if overrides and src in overrides else ""
                print("    %3d %s -> %3d %s dist %5.1f%s" %
                      (src, colony_pal[src], dst, menu_pal[dst], dist, mark))
            out_dir = os.path.join(args.out, sprite)
            os.makedirs(out_dir, exist_ok=True)
            path = os.path.join(out_dir, state + ".bin")
            with open(path, "wb") as f:
                f.write(extract_sprite.frame_bytes(remapped))
            print("  wrote %s (%d bytes)" %
                  (path, os.path.getsize(path)))
            # The middle column: the centre of the flat run on
            # label-free row 5 (the three-slice middle).
            row5 = remapped[5]
            runs, start = [], 0
            for cx in range(1, width + 1):
                if cx == width or row5[cx] != row5[start]:
                    runs.append((start, cx - 1))
                    start = cx
            flat = max(runs, key=lambda r: r[1] - r[0])
            print("  remapped row-5 menu-50 run: %d..%d (mid %d)" %
                  (flat[0], flat[1], (flat[0] + flat[1]) // 2))

    if args.cross_check:
        print("--- cross-check vs reference bins ---")
        for state in ("normal", "active"):
            screen = open(os.path.join(args.out, "sort-name", state + ".bin"),
                          "rb").read()
            ref = open(os.path.join(args.cross_check, state + ".bin"),
                       "rb").read()
            print("  %s: screen %d bytes, reference %d bytes, identical: %s" %
                  (state, len(screen), len(ref), screen == ref))
            check("screen Name %s == reference %s" % (state, state),
                  screen == ref)

    print("--- gap profile ---")
    profiles = {}
    for gx in GAPS:
        colony = [rgb_to_colony(shots["default"][Y + ry][gx])
                  for ry in range(H)]
        menu = [extract_sprite.remap_grid([[v]], colony_pal, menu_pal, {})[0][0][0]
                for v in colony]
        profiles[gx] = menu
        print("  gap x=%d colony idx: %s" % (gx, colony))
        print("  gap x=%d menu idx:   %s" % (gx, menu))
    agree = all(profiles[gx][1:] == profiles[139][1:] for gx in profiles)
    check("all five colony gaps share rows 1..23", agree,
          "%s" % ({gx: profiles[gx] for gx in profiles
                   if profiles[gx][1:] != profiles[139][1:]},))
    print("  gap row 0 (the strip-wide arch) per gap: %s"
          % ({gx: profiles[gx][0] for gx in profiles},))
    print("  gap profile rows 1..23 (menu indices): %s" % (profiles[139][1:],))

    print()
    if FAILURES:
        print("DERIVATION RESULT: %d FAILURES" % len(FAILURES))
        for name in FAILURES:
            print("  FAILED: %s" % name)
        return 1
    print("DERIVATION RESULT: ALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
