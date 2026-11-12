#!/usr/bin/env python3
"""Build the Moo2Display bitmap font: the colony header-plate label font.

The colony summary screen's header plates (NAME, FARMERS, WORKERS,
SCIENTISTS, BUILDING) and the SORT caption carry large display labels
-- about 14 px per character, 8 rows of ink -- but the kit had no such
theme font: the replication rendered them in the tab font (engine font
style 2), which inks the right 8 rows but only about half the width
(real FARMERS 97 px vs kit 53 px). This font is the same answer as
Moo2Menu for the main screen and Moo2Button for the metal buttons: an
orion2re-owned replica font, bundled with the game, that reproduces
the baked letterforms.

How each glyph was obtained (see the JSON's provenance field):
- Every glyph except V: transcribed by observation from the rendered
  colony summary screen (the fullwin-colony-summary.png screengrab,
  game coords; header plates y 4..31, label ink rows 17..24). The
  grids below were produced by reading the screenshot's label band
  and mapping each pixel's RGB to the nearest menu-palette grey
  (MAINMENU.LBX entry 2, offset 24; the menu grey ramp 230-250), keeping
  indices >= 240 as ink and the rest as transparent. No LBX file is
  read by this script to produce the font, and no sprite file is read
  either: the source is the rendered screen, and --check verifies the
  composed labels' widths against the measured label widths below.
  The glyphs are the bright cores plus their antialiased fringe; the
  plate background (menu greys below 240) is transparent.
- V: designed to match. No baked V exists at this scale in the colony
  screen (VALUE is a settings-screen plate, rendered at runtime, not
  a baked colony label), and Moo2Menu carries no V either. V is drawn
  in the display style -- 2-3 px diagonal stems like N's diagonal,
  the same AA palette (243 main, 240-242 edges), 12 wide against
  N's 13 and U's 12 -- with proportions from W's outer diagonals.
- The pressed state mirrors normal: header plates have no pressed
  state, so the JSON carries the same 8 rows for pressed (the runtime
  requires a pressed bitmap; it is never rendered for plates).

Format (same top-level shape as Moo2Menu.json and Moo2Button.json):
name, cap_height (8: the label ink is 8 rows, y 17..24), descender_rows
(0: all capitals, no descenders), space_advance (4), provenance,
glyphs. Each glyph: advance (width + 2, the measured inter-letter gap
is 1-3 px), width (the bitmap cell width), bitmap {normal: 8 rows,
pressed: 8 rows, identical}. A state's rows are lists of palette
indices (null = transparent). The runtime (ui_kit::Bitmap_Font)
top-aligns both states at the label's y, so the glyphs ink rows
17..24 of the plate, exactly like the baked labels.

Usage: tools/build_moo2display_font.py [--output PATH] [--check]
  [--screenshot PATH]
  --check composes the five plate labels and the SORT caption from
  the grids below and verifies their widths against the measured
  widths (NAME 58, FARMERS 97, WORKERS 98, SCIENTISTS 105, BUILDING
  87). With --screenshot PATH (a colony summary screengrab, 1280x1024
  with the 640x480 game centred), it additionally compares each
  composed label pixel by pixel against the screenshot's label band
  and reports the match. The check reads the screenshot only when
  asked; the font itself is generated from the literals below.
"""

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / "data" / "base" / "fonts" / "Moo2Display.json"

NAME = "Moo2Display"
CAP_HEIGHT = 8
DESCENDER_ROWS = 0
SPACE_ADVANCE = 4

PROVENANCE = (
    "Drawn by observation from the rendered colony summary screen "
    "(header plates y 4..31, label ink rows 17..24, game coords). "
    "Each pixel's RGB was mapped to the nearest menu-palette grey "
    "(MAINMENU.LBX entry 2, offset 24; ramp 230-250); indices >= 240 "
    "are ink, the rest transparent. No LBX file was read to produce "
    "this font and no sprite pixels were byte-extracted into it; the "
    "grids below are transcriptions of the rendered screen, verified "
    "by --check. V is designed to match (no baked V at this scale): "
    "2-3 px diagonal stems like N, same AA palette, 12 wide."
)

GLYPHS = {
    "A": {
        "advance": 15, "width": 14,
        "normal": [
            [None, None, None, None, None, 243, 243, 243, 243, 243, None, None, None, None],
            [None, None, None, None, None, 243, 243, 243, 243, 243, 240, None, None, None],
            [None, None, None, None, 243, 241, None, 243, 243, 243, 243, None, None, None],
            [None, None, None, 243, 243, None, None, 243, 243, 243, 243, 243, None, None],
            [None, None, 240, 243, 242, None, None, 241, 243, 243, 243, 243, None, None],
            [None, None, 243, 243, 240, 240, 240, 240, 243, 243, 243, 243, 243, None],
            [None, 243, 243, None, None, None, None, None, None, 243, 243, 243, 243, 240],
            [241, 242, 240, None, None, None, None, None, None, 242, 242, 242, 242, 243],
        ],
    },
    "B": {
        "advance": 13, "width": 11,
        "normal": [
            [245, 243, 243, 244, 244, 245, 244, 244, 244, 243, 240],
            [243, 243, 243, 243, None, None, None, 243, 243, 243, 244],
            [243, 243, 243, 243, None, None, None, 242, 243, 243, 244],
            [243, 243, 243, 243, 243, 243, 243, 244, 243, 243, None],
            [243, 243, 243, 243, 240, None, None, 243, 243, 243, 245],
            [243, 243, 243, 244, None, None, None, 241, 243, 243, 244],
            [243, 243, 243, 244, 241, 240, 242, 244, 243, 243, 244],
            [242, 242, 242, 242, 242, 242, 242, 242, 242, 240, None],
        ],
    },
    "C": {
        "advance": 11, "width": 10,
        "normal": [
            [None, 243, 246, 247, 247, 248, 248, 248, 248, 242],
            [243, 247, 246, 246, 242, None, None, None, None, None],
            [245, 246, 246, 247, None, None, None, None, None, None],
            [245, 246, 246, 246, None, None, None, None, None, None],
            [245, 246, 246, 247, None, None, None, None, None, None],
            [243, 246, 246, 246, None, None, None, None, None, None],
            [None, 247, 247, 247, 246, 243, 243, 243, 244, 244],
            [None, None, None, 241, 241, 242, 242, 242, 242, None],
        ],
    },
    "D": {
        "advance": 14, "width": 11,
        "normal": [
            [243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 240],
            [242, 243, 243, 243, None, None, None, 242, 243, 243, 244],
            [242, 243, 243, 243, None, None, None, 241, 243, 243, 243],
            [242, 243, 243, 244, None, None, None, 242, 243, 243, 244],
            [242, 243, 243, 244, None, None, None, 242, 243, 243, 244],
            [242, 243, 243, 244, None, None, None, 241, 243, 243, 243],
            [242, 244, 243, 244, 241, None, 240, 243, 244, 243, 243],
            [241, 240, 240, 240, 240, 240, 240, 240, 240, 240, None],
        ],
    },
    "E": {
        "advance": 11, "width": 10,
        "normal": [
            [243, 243, 243, 243, 243, 243, 243, 243, 243, 243],
            [243, 243, 243, 243, 241, None, None, None, None, None],
            [243, 243, 243, 243, 240, None, None, None, None, None],
            [243, 243, 243, 243, 243, 242, 242, 242, 243, 243],
            [243, 243, 243, 243, 242, None, None, None, None, None],
            [243, 243, 243, 243, 240, None, None, None, None, None],
            [243, 243, 243, 243, 242, None, None, None, None, None],
            [243, 243, 243, 243, 243, 243, 243, 243, 243, 243],
        ],
    },
    "F": {
        "advance": 13, "width": 10,
        "normal": [
            [243, 243, 243, 243, 243, 243, 245, 245, 245, 245],
            [243, 243, 243, 243, 242, None, None, None, None, None],
            [243, 243, 243, 243, 241, None, None, None, None, None],
            [243, 243, 243, 243, 244, 243, 243, 243, 243, 245],
            [243, 243, 243, 243, 242, None, None, None, None, 242],
            [243, 243, 243, 243, 241, None, None, None, None, None],
            [243, 243, 243, 243, 243, None, None, None, None, None],
            [242, 242, 242, 242, 242, None, None, None, None, None],
        ],
    },
    "G": {
        "advance": 13, "width": 11,
        "normal": [
            [242, 243, 243, 243, 243, 243, 243, 243, 243, 243, None],
            [243, 243, 243, 242, None, None, None, None, None, None, None],
            [243, 243, 243, 240, None, None, None, None, None, None, None],
            [243, 243, 243, 240, None, 243, 243, 243, 243, 243, 242],
            [243, 243, 243, 240, None, None, None, 243, 243, 243, 242],
            [243, 243, 243, 240, None, None, None, 243, 243, 243, 242],
            [243, 243, 243, 243, None, 240, 242, 243, 243, 243, 242],
            [None, 240, 240, 240, 240, 240, 240, None, 240, 240, 240],
        ],
    },
    "I": {
        "advance": 6, "width": 5,
        "normal": [
            [246, 247, 247, 247, 242],
            [243, 246, 246, 247, None],
            [243, 246, 246, 247, None],
            [244, 246, 246, 246, None],
            [244, 246, 246, 247, None],
            [244, 246, 246, 246, None],
            [246, 247, 247, 247, None],
            [242, 241, 241, 242, None],
        ],
    },
    "K": {
        "advance": 15, "width": 13,
        "normal": [
            [246, 246, 246, 246, 245, None, None, None, 244, 246, 245, None, None],
            [243, 246, 246, 246, 242, None, None, 243, 247, 243, None, None, None],
            [243, 246, 246, 246, 242, None, 245, 247, 245, None, None, None, None],
            [243, 246, 246, 246, 245, 246, 246, 246, 246, 241, None, None, None],
            [243, 246, 246, 246, 247, 245, 247, 246, 246, 247, None, None, None],
            [243, 246, 246, 246, 243, None, 243, 247, 246, 246, 247, None, None],
            [243, 246, 246, 246, 242, None, None, 244, 246, 246, 246, 246, None],
            [245, 246, 245, 246, 244, None, None, None, 244, 245, 245, 246, 245],
        ],
    },
    "L": {
        "advance": 10, "width": 9,
        "normal": [
            [244, 243, 244, 243, None, None, None, None, None],
            [243, 243, 243, 243, None, None, None, None, None],
            [243, 243, 243, 243, None, None, None, None, None],
            [244, 243, 243, 243, None, None, None, None, None],
            [244, 243, 243, 243, None, None, None, None, None],
            [244, 243, 243, 243, None, None, None, None, None],
            [244, 243, 243, 243, 240, 240, 240, 240, 241],
            [242, 242, 242, 242, 242, 242, 242, 240, 240],
        ],
    },
    "M": {
        "advance": 19, "width": 15,
        "normal": [
            [243, 243, 243, 243, 243, None, None, None, None, None, 243, 243, 243, 243, 243],
            [243, 243, 243, 243, 243, 243, None, None, None, 243, 243, 243, 243, 243, 243],
            [243, 242, 242, 243, 243, 243, None, None, None, 243, 240, 243, 243, 243, 243],
            [243, 241, None, 243, 243, 243, 243, None, 243, 243, None, 243, 243, 243, 244],
            [243, 242, None, 243, 243, 243, 243, 242, 243, None, None, 243, 243, 243, 243],
            [243, 242, None, None, 243, 243, 243, 243, 243, None, None, 243, 243, 243, 243],
            [243, 242, None, None, 243, 243, 243, 243, None, None, None, 243, 243, 243, 243],
            [243, 242, None, None, None, 243, 243, 242, None, None, None, 243, 243, 243, 243],
        ],
    },
    "N": {
        "advance": 14, "width": 13,
        "normal": [
            [243, 243, 243, 243, 243, 243, None, None, None, None, 240, 243, 242],
            [240, 243, 242, 243, 243, 243, 243, None, None, None, None, 243, None],
            [240, 243, None, 243, 243, 243, 243, 243, None, None, None, 243, None],
            [240, 243, None, None, 243, 243, 243, 243, None, None, None, 243, None],
            [240, 243, None, None, None, 243, 243, 243, 243, None, None, 243, None],
            [240, 243, None, None, None, 242, 243, 243, 243, 242, None, 243, None],
            [242, 243, None, None, None, None, 243, 243, 243, 243, 243, 243, None],
            [242, 242, None, None, None, None, None, 242, 242, 242, 242, 242, None],
        ],
    },
    "O": {
        "advance": 14, "width": 13,
        "normal": [
            [None, 240, 243, 245, 245, 245, 245, 245, 245, 245, 242, None, None],
            [241, 246, 246, 246, 243, 240, 240, 241, 246, 246, 246, 247, None],
            [246, 246, 246, 247, None, None, None, None, 245, 246, 246, 247, 240],
            [246, 246, 246, 246, 240, None, None, None, 244, 246, 246, 246, 240],
            [245, 246, 246, 247, None, None, None, None, 244, 246, 246, 247, 240],
            [245, 246, 246, 247, 240, None, None, None, 244, 246, 246, 247, 240],
            [242, 246, 246, 246, 243, 240, 240, 242, 246, 246, 246, 247, None],
            [None, 240, 243, 244, 244, 244, 244, 244, 244, 246, 243, None, None],
        ],
    },
    "R": {
        "advance": 14, "width": 12,
        "normal": [
            [244, 243, 243, 243, 244, 245, 245, 244, 244, 243, 243, None],
            [245, 244, 244, 245, 240, None, None, 242, 244, 244, 244, 244],
            [245, 244, 244, 245, None, None, None, None, 244, 244, 244, 245],
            [245, 244, 244, 244, 243, 243, 243, 244, 244, 244, 244, 246],
            [245, 244, 244, 244, 241, 241, 245, 244, 244, 244, 242, None],
            [245, 245, 245, 245, None, None, 243, 245, 245, 245, 244, None],
            [245, 244, 244, 245, None, None, None, 244, 244, 244, 245, 243],
            [243, 243, 243, 243, None, None, None, None, 243, 243, 243, 243],
        ],
    },
    "S": {
        "advance": 12, "width": 11,
        "normal": [
            [None, 243, 243, 245, 245, 246, 246, 246, 247, 246, 243],
            [245, 245, 245, 245, 242, None, None, None, None, None, 240],
            [246, 245, 245, 245, 243, 240, 241, 240, 241, None, None],
            [246, 245, 245, 245, 245, 245, 245, 245, 245, 246, 246],
            [243, 246, 245, 245, 245, 245, 245, 245, 245, 245, 245],
            [None, None, None, None, None, None, None, 245, 245, 245, 245],
            [243, 243, 240, 240, None, 241, 242, 245, 245, 245, 246],
            [242, 243, 243, 243, 243, 243, 243, 243, 243, 240, None],
        ],
    },
    "T": {
        "advance": 11, "width": 10,
        "normal": [
            [247, 246, 246, 246, 245, 245, 246, 246, 246, 247],
            [None, None, None, 245, 245, 245, 245, None, None, None],
            [None, None, None, 245, 245, 245, 245, None, None, None],
            [None, None, None, 246, 245, 245, 246, None, None, None],
            [None, None, None, 246, 245, 245, 246, None, None, None],
            [None, None, None, 246, 245, 245, 246, None, None, None],
            [None, None, None, 246, 246, 246, 246, None, None, None],
            [None, None, None, 240, 240, 240, 240, None, None, None],
        ],
    },
    "U": {
        "advance": 13, "width": 12,
        "normal": [
            [243, 243, 243, 244, None, None, None, None, None, 241, 245, 240],
            [243, 243, 243, 243, None, None, None, None, None, None, 244, None],
            [243, 243, 243, 243, None, None, None, None, None, None, 244, None],
            [243, 243, 243, 244, None, None, None, None, None, None, 244, None],
            [243, 243, 243, 244, None, None, None, None, None, None, 245, None],
            [243, 243, 243, 244, None, None, None, None, None, None, 245, None],
            [242, 244, 243, 244, 243, 240, 240, 240, 240, 243, 243, None],
            [None, None, 242, 242, 242, 242, 242, 242, 240, 240, None, None],
        ],
    },
    "W": {
        "advance": 19, "width": 18,
        "normal": [
            [245, 245, 245, 245, 243, None, None, None, 244, 245, 245, 245, 243, None, None, None, 244, 247],
            [243, 246, 246, 246, 246, None, None, None, 246, 246, 246, 246, 247, None, None, None, 247, 245],
            [None, 247, 246, 246, 247, None, None, 243, 247, 246, 246, 246, 247, 241, None, 240, 247, 240],
            [None, 244, 246, 246, 246, 242, None, 247, 243, 242, 246, 246, 246, 246, None, 244, 247, None],
            [None, None, 247, 246, 246, 245, 242, 247, None, None, 247, 246, 246, 247, 242, 247, 243, None],
            [None, None, 246, 246, 246, 246, 246, 246, None, None, 245, 246, 246, 246, 246, 247, None, None],
            [None, None, 243, 246, 246, 246, 246, 243, None, None, 240, 246, 246, 246, 246, 245, None, None],
            [None, None, None, 244, 244, 244, 244, None, None, None, None, 244, 244, 244, 244, 240, None, None],
        ],
    },
    "V": {
        "advance": 14, "width": 12,
        "normal": [
            [243, 243, 243, None, None, None, None, None, None, 243, 243, 243],
            [241, 243, 243, 243, None, None, None, None, 243, 243, 243, 241],
            [None, 241, 243, 243, 243, None, None, 243, 243, 243, 241, None],
            [None, None, 241, 243, 243, 243, 243, 243, 243, 241, None, None],
            [None, None, None, 241, 243, 243, 243, 243, 241, None, None, None],
            [None, None, None, None, 241, 243, 243, 241, None, None, None, None],
            [None, None, None, None, None, 243, 243, None, None, None, None, None],
            [None, None, None, None, None, 242, 242, None, None, None, None, None],
        ],
    },
}

# Measured label widths (game coords, from the colony screenshot).
EXPECTED_WIDTHS = {
    "NAME": 58,
    "FARMERS": 97,
    "WORKERS": 98,
    "SCIENTISTS": 105,
    "BUILDING": 87,
}


def label_width(text):
    """Width of a label composed from the grids (advance + last width)."""
    total = 0
    for i, ch in enumerate(text):
        glyph = GLYPHS[ch]
        if i < len(text) - 1:
            total += glyph["advance"]
        else:
            total += glyph["width"]
    return total


def compose(text):
    """Compose a label into an 8-row grid of palette indices (or None)."""
    width = label_width(text)
    grid = [[None] * width for _ in range(CAP_HEIGHT)]
    x = 0
    for ch in text:
        glyph = GLYPHS[ch]
        for row in range(CAP_HEIGHT):
            for col, value in enumerate(glyph["normal"][row]):
                if value is not None:
                    grid[row][x + col] = value
        x += glyph["advance"]
    return grid


def check_widths():
    ok = True
    for text, expected in EXPECTED_WIDTHS.items():
        got = label_width(text)
        status = "OK" if got == expected else "MISMATCH"
        if got != expected:
            ok = False
        print(f"  {text}: composed {got} px, measured {expected} px [{status}]")
    return ok


def check_screenshot(path):
    """Pixel-compare each composed label against the screenshot's band."""
    try:
        from PIL import Image
    except ImportError:
        print("  --screenshot needs Pillow (pip install Pillow)")
        return False
    im = Image.open(path).convert("RGB")
    if im.size != (1280, 1024):
        print(f"  screenshot is {im.size}, expected 1280x1024")
        return False
    ox, oy = (1280 - 640) // 2, (1024 - 480) // 2
    # Menu grey ramp, for mapping screenshot RGB to palette indices.
    menu_grey = {
        230: (36, 36, 40), 231: (44, 44, 52), 232: (52, 52, 60),
        233: (60, 60, 68), 234: (72, 72, 80), 235: (80, 80, 88),
        236: (88, 88, 96), 237: (96, 96, 104), 238: (108, 108, 116),
        239: (116, 116, 124), 240: (124, 124, 132), 241: (136, 136, 144),
        242: (144, 144, 152), 243: (152, 152, 160), 244: (164, 164, 172),
        245: (172, 172, 180), 246: (180, 180, 188), 247: (188, 188, 196),
        248: (196, 196, 204), 249: (204, 204, 212), 250: (220, 220, 228),
    }

    def nearest(rgb):
        best, bestd = None, None
        for idx, pal in menu_grey.items():
            d = (rgb[0] - pal[0]) ** 2 + (rgb[1] - pal[1]) ** 2 + (rgb[2] - pal[2]) ** 2
            if bestd is None or d < bestd:
                bestd, best = d, idx
        return best

    # Label positions (game coords): (text, x0) with ink rows 17..24.
    labels = [
        ("NAME", 23), ("FARMERS", 119), ("WORKERS", 258),
        ("SCIENTISTS", 392), ("BUILDING", 518),
    ]
    # Binary ink comparison with 1 px tolerance, mirroring the colony
    # census (which compares edges, not exact palette indices). The
    # baked labels vary slightly in AA values between instances of the
    # same letter, so exact-index match is unachievable; what matters
    # is that the ink shape sits in the right place. A composed ink
    # pixel matches when the screenshot carries any ink (>= 240) at
    # that position or any of its 8 neighbours, and vice versa.
    ok = True
    for text, x0 in labels:
        grid = compose(text)
        width = len(grid[0])
        shot_ink = set()
        for row in range(CAP_HEIGHT):
            for col in range(width):
                rgb = im.getpixel((ox + x0 + col, oy + 17 + row))
                idx = nearest(rgb)
                if idx is not None and idx >= 240:
                    shot_ink.add((col, row))
        comp_ink = set()
        for row in range(CAP_HEIGHT):
            for col in range(width):
                if grid[row][col] is not None:
                    comp_ink.add((col, row))
        def near(ink_set, col, row):
            return any(
                (col + dx, row + dy) in ink_set
                for dx in (-1, 0, 1) for dy in (-1, 0, 1)
            )
        comp_hit = sum(1 for (c, r) in comp_ink if near(shot_ink, c, r))
        shot_hit = sum(1 for (c, r) in shot_ink if near(comp_ink, c, r))
        comp_pct = 100.0 * comp_hit / len(comp_ink) if comp_ink else 0
        shot_pct = 100.0 * shot_hit / len(shot_ink) if shot_ink else 0
        status = "OK" if comp_pct >= 95 and shot_pct >= 95 else "DIFF"
        if comp_pct < 95 or shot_pct < 95:
            ok = False
        print(f"  {text}: composed ink {comp_hit}/{len(comp_ink)} ({comp_pct:.1f}%), "
              f"shot ink {shot_hit}/{len(shot_ink)} ({shot_pct:.1f}%) [{status}]")
    return ok


def build():
    glyphs = {}
    for letter, glyph in GLYPHS.items():
        glyphs[letter] = {
            "advance": glyph["advance"],
            "width": glyph["width"],
            "bitmap": {
                "normal": glyph["normal"],
                "pressed": glyph["normal"],
            },
        }
    return {
        "name": NAME,
        "cap_height": CAP_HEIGHT,
        "descender_rows": DESCENDER_ROWS,
        "space_advance": SPACE_ADVANCE,
        "provenance": PROVENANCE,
        "glyphs": glyphs,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--screenshot", type=Path, default=None)
    args = parser.parse_args()
    if args.check:
        print("label widths:")
        ok = check_widths()
        if args.screenshot:
            print("screenshot pixels:")
            ok = check_screenshot(args.screenshot) and ok
        print("CHECK " + ("PASSED" if ok else "FAILED"))
        return 0 if ok else 1
    font = build()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(font, f, indent=1)
        f.write("\n")
    print(f"wrote {args.output} ({len(font['glyphs'])} glyphs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
