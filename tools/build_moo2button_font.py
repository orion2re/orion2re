#!/usr/bin/env python3
"""Build the Moo2Button bitmap font: the metal-button label font.

The settings screen's MORE button (and the RETURN button's stale-frame
fallback) renders its label at runtime, but the baked RETURN text in
COLSUM.LBX entry 1 -- the derived repo sprite
data/base/sprites/return/normal.bin -- is hand-drawn pixel art, not a
verbatim font render (baked R leg 1 px vs engine-font 2 px, E bars 4 px
vs 5 px, U 7 wide vs 6, N diagonal 1 px vs 2 px). The best runtime
engine font (FONTS style 2, spacing 1) only reaches IoU 0.626 against
it. This font is the same answer as Moo2Menu for the main screen: an
orion2re-owned replica font, bundled with the game, that reproduces
the baked letterforms 1:1 where a baked reference exists.

How each glyph was obtained (see the JSON's provenance field):
- R, E, T, U, N, normal state: drawn by observation from the baked
  RETURN text in the derived repo sprite data/base/sprites/return/
  normal.bin (label band rows 8..16, x 14..60; core palette 0, AA
  15/17/46/72). The grids below were transcribed by looking at the
  sprite's label band and the rendered RETURN button, not by
  byte-extracting pixels out of any LBX file: no LBX file is read by
  this script to produce the font, and no sprite file is read either
  (--check only verifies the hand-drawn grids against the derived
  repo sprites; the JSON is generated from the literals below).
  The single R in the font is the FIRST baked R (x 17..22): the two
  baked Rs differ in a few AA pixels (the second R's leg is 1 px core
  with AA where the first is 2 px core, and its mid-bar AA sits one
  column further in), so the second R renders with those few AA
  diffs; the core skeleton is identical. --check reports them.
- R, E, T, U, N, pressed state: drawn by observation from the baked
  pressed RETURN text in data/base/sprites/return/pressed.bin
  (label band rows 8..18; the pressed letters are 11 rows tall, not
  9). The pressed face is NOT a recolour of the normal glyphs: the
  letters are bolder, with a bright-orange fill (palette 252) inside
  the strokes, a brown shadow (112) on the stroke edges, and a single
  light-tan pixel (207) at the first R's top-left corner. 252 and 112
  occur nowhere else in the pressed frame -- only inside the label
  band -- so they are letterform ink, not face dither (the normal
  frame's face dither, palette 54, by contrast spreads over the whole
  frame). Palette 54 (face grey) is NOT transcribed: the few 54s in
  the pressed label band sit below the text and belong to the face.
- M, O: designed to match. No baked M or O exists at this scale: the
  colony sort-tab frames carry no baked label (their labels render at
  runtime), and the only baked M/O in the repo are Moo2Menu's at QUIT
  scale (cap 11). M and O are drawn at cap 9 in the RETURN style --
  2 px stems like R, 1 px bars like E, the same AA palette -- with
  proportions from Moo2Menu (M 8 wide against R 6, vertex at ~55% of
  cap height; O 7 wide, the same width as U, rounded top and bottom
  like U's bottom curve). O follows U's AA conventions exactly (left
  edge AA 17, bottom-corner AA 72, inner-corner AA 15), plus a 17
  down both outer stem edges so the O reads smooth, not hard-edged.
  M is an open-top M: two 2 px stems, V arms that step in from the
  stems' inner edges (cols 2, 5) for two rows, then a vertex (cols
  3, 4) at rows 3-4, empty below; the top row is stems only. An
  open top is what an M IS, not damage. The 2026-11-10 eba950e
  attempt (V arms reaching the top row, core at cols 2, 5 in rows
  0-3) turned the M into an H -- two 3 px stems plus a horizontal
  bar at rows 4-5 -- and was reverted. No inner-edge AA below the
  vertex: the AA values (72 etc.) are dark enough to read as ink,
  so AA at cols 2, 5 below the vertex fills the middle and reads as
  a bar. The vertex carries the AA 72s that flank U's bottom bar
  (row 4 only). The 'damaged' look is pixelation, not shape: the
  MORE label's AA is sparse and dark against the baked RETURN's
  full mid-grey halo; full parity needs a font AA halo pass, a
  design decision, not done here. Their pressed states
  follow the measured pressed conventions (11 rows, orange core 221
  with 252 fill inside the strokes, 223 bright edges, top and bottom
  rows all core).
- E's normal width is 6, not 5: its top bar ends in an AA 17 at
  x 29, one column past the 5-column stem -- measured, not assumed.
- N carries a 1 px left bearing in both states: the baked N sits one
  pixel right of the uniform 1 px-spacing grid (the R-to-N gap is
  2 px; every other gap is 1 px), so N's cell is one column wider
  than its ink, with the first column blank. Its advance is 8.
- Boundary columns: the pressed band's letters touch, so two columns
  are shared between neighbours and are assigned by shape, not by
  grid. x30 (pressed) belongs to E: its pixels sit at E's three bar
  ends (top y08, middle y12, bottom y18, each with flanking pixels),
  while a T has no left vertical to explain them; T's top bar then
  stays 5 wide like the normal T. x51 (pressed) belongs to the second
  R: its pixels form that R's bowl right wall (rows 10-12) and leg
  corner (row 17) with 112 shadows; N's stem is the clean vertical at
  x52-53, and a detached core vertical one column left of it would
  not belong to N. The font's single R is drawn from the first R, so
  the second R's x51 pixels are not in the font at all: --check
  reports them as known second-R variance, and the census quantifies
  them. E's pressed bitmap is 7 wide against an advance of 6: its
  top bar reaches one column into the next cell, which is blank there
  for every real neighbour (T's pressed first column is blank; E is
  last in MORE).

Format (same top-level shape as Moo2Menu.json): name, cap_height (9),
descender_rows (2: the pressed glyphs extend two rows below the
normal baseline, so the cell is 11 rows), space_advance (4: no button
label contains a space; 4 px sits between the 1 px inter-letter gap
and the narrowest advance), provenance, glyphs. Each glyph: advance
(the origin grid, shared by both states), width (the NORMAL state's
cell width), bitmap {normal: 9 rows, pressed: 11 rows}. A state's
rows are lists of palette indices (null = transparent); pressed rows
may be longer than width -- their actual length is the pressed
state's width. The runtime (ui_kit::Bitmap_Font) top-aligns both
states at the label's y, so normal glyphs ink rows 8..16 and pressed
glyphs rows 8..18 of the 26 px button, exactly like the baked text.

Usage: tools/build_moo2button_font.py [--output PATH] [--check]
  --check composes "RETURN" from the grids below and compares it
  against the derived repo sprites, pixel by pixel. It passes only
  when the composition matches the baked bands outside the second-R
  cell (rel x 27..34), whose diffs are the known single-R variance
  and are reported separately. The check reads the sprites; the font
  itself is generated from the literals only.
"""

import argparse
import json
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / "data" / "base" / "fonts" / "Moo2Button.json"
RETURN_DIR = REPO_ROOT / "data" / "base" / "sprites" / "return"

NAME = "Moo2Button"
CAP_HEIGHT = 9
DESCENDER_ROWS = 2
SPACE_ADVANCE = 4

PROVENANCE = (
    "Drawn by observation from the baked RETURN button text "
    "(COLSUM.LBX entry 1, derived repo sprites "
    "data/base/sprites/return/{normal,pressed}.bin). R,E,T,U,N normal "
    "are the baked normal glyphs (9 rows, core 0, AA 15/17/46/72) and "
    "R,E,T,U,N pressed are the baked pressed glyphs (11 rows, core "
    "221, bright edge 223, fill 252, shadow 112, one 207 pixel at the "
    "first R's top-left corner); the single R is the first baked R, "
    "so the second R renders with a few AA diffs and without its "
    "extra right-edge column. M and O are designed to match (no baked "
    "M or O exists at this scale): same 2 px stems, 1 px bars and AA "
    "palette, proportions from Moo2Menu's QUIT-scale M and O, O "
    "following U's AA conventions. No LBX file was read to produce "
    "this font and no sprite pixels were byte-extracted into it; the "
    "grids below are hand-drawn transcriptions, verified by --check."
)

# None = transparent. Normal state: 9 rows. Pressed state: 11 rows.
GLYPHS = {
    "E": {
        "advance": 6,
        "width": 6,
        "bitmap": {
            "normal": [
                [17, 0, 0, 0, 0, 17],
                [17, 0, None, None, None, None],
                [17, 0, None, None, None, None],
                [17, 0, 15, None, None, None],
                [17, 0, 0, 0, 0, None],
                [17, 0, None, None, None, None],
                [17, 0, None, None, None, None],
                [15, 0, 17, None, None, None],
                [46, 0, 0, 0, 0, None],
            ],
            "pressed": [
                [None, 221, 221, 221, 221, 221, 221],
                [None, 223, 252, 252, 252, 252, 223],
                [None, 223, 252, 221, 112, 112, 112],
                [112, 223, 252, 221, None, None, None],
                [112, 223, 252, 223, 221, 221, 112],
                [None, 223, 252, 252, 252, 252, 221],
                [None, 223, 252, 221, 112, 221, None],
                [None, 223, 252, 221, None, None, None],
                [None, 223, 252, 221, 112, 112, 112],
                [112, 223, 252, 252, 252, 252, 221],
                [112, 221, 221, 221, 221, 221, 112],
            ],
        }
    },
    "M": {
        "advance": 9,
        "width": 8,
        "bitmap": {
            "normal": [
                [0, 0, None, None, None, None, 0, 0],
                [0, 0, 0, None, None, 0, 0, 0],
                [0, 0, 0, None, None, 0, 0, 0],
                [0, 0, None, 0, 0, None, 0, 0],
                [0, 0, 72, 0, 0, 72, 0, 0],
                [0, 0, None, None, None, None, 0, 0],
                [0, 0, None, None, None, None, 0, 0],
                [0, 0, None, None, None, None, 0, 0],
                [0, 0, None, None, None, None, 0, 0],
            ],
            "pressed": [
                [221, 221, None, None, None, None, None, 221, 221],
                [221, 252, 221, None, None, None, 221, 252, 221],
                [221, 252, 221, 223, None, 223, 221, 252, 221],
                [221, 252, 221, 223, None, 223, 221, 252, 221],
                [221, 252, 221, None, None, None, 221, 252, 221],
                [221, 252, None, 221, 221, 221, None, 252, 221],
                [221, 252, None, 221, 221, 221, None, 252, 221],
                [221, 252, None, None, None, None, None, 252, 221],
                [221, 252, None, None, None, None, None, 252, 221],
                [221, 252, None, None, None, None, None, 252, 221],
                [221, 221, None, None, None, None, None, 221, 221],
            ],
        }
    },
    "N": {
        "advance": 8,
        "width": 7,
        "bitmap": {
            "normal": [
                [None, 0, 0, 17, None, None, 17],
                [None, 72, 0, 72, None, None, 17],
                [None, None, 0, 0, None, None, 17],
                [None, None, 72, 0, None, None, 17],
                [None, None, None, 0, 46, None, 17],
                [None, None, None, 0, 0, None, 17],
                [None, None, None, 72, 0, 15, 17],
                [None, None, None, 17, 0, 0, 17],
                [None, None, None, None, 0, 0, 17],
            ],
            "pressed": [
                [None, 221, 221, 221, 221, None, 221, 221],
                [None, 223, 252, 252, 223, None, 221, 223],
                [None, 252, 223, 252, 223, 112, 221, 223],
                [None, 252, 223, 252, 252, 221, 221, 223],
                [None, 252, 221, 223, 252, 221, 221, 223],
                [None, 252, 221, 221, 252, 223, 221, 223],
                [None, 252, 221, 221, 252, 252, 221, 223],
                [None, 252, 221, 112, 223, 252, 223, 223],
                [None, 252, 221, None, 223, 252, 223, 223],
                [None, 252, 221, None, 221, 252, 252, 223],
                [None, 221, 221, None, 112, 221, 221, 221],
            ],
        }
    },
    "O": {
        "advance": 8,
        "width": 7,
        "bitmap": {
            "normal": [
                [None, 72, 0, 0, 0, 72, None],
                [17, 0, None, None, None, 0, 17],
                [17, 0, None, None, None, 0, 17],
                [17, 0, None, None, None, 0, 17],
                [17, 0, None, None, None, 0, 17],
                [17, 0, None, None, None, 0, 17],
                [17, 0, None, None, None, 0, 17],
                [17, 0, 15, None, 15, 0, 17],
                [None, 72, 0, 0, 0, 72, None],
            ],
            "pressed": [
                [None, 221, 221, 221, 221, 221, 221, None],
                [223, 252, 221, 252, 252, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, None, None, 221, 252, 223],
                [223, 252, 221, 252, 252, 221, 252, 223],
                [None, 221, 221, 221, 221, 221, 221, None],
            ],
        }
    },
    "R": {
        "advance": 7,
        "width": 6,
        "bitmap": {
            "normal": [
                [0, 0, 0, 0, 0, 15],
                [0, 0, None, None, 0, 72],
                [0, 0, None, None, 0, 0],
                [0, 0, None, None, 0, 0],
                [0, 0, 0, 0, 0, 46],
                [0, 0, None, 0, 0, None],
                [0, 0, None, 72, 0, None],
                [0, 0, None, None, 0, 15],
                [0, 0, None, None, 0, 72],
            ],
            "pressed": [
                [207, 221, 221, 221, 221, 221, 112],
                [221, 252, 252, 252, 252, 252, 223],
                [221, 252, 252, 112, 221, 252, 223],
                [221, 252, 252, 112, 112, 252, 252],
                [221, 252, 252, 221, 223, 252, 252],
                [221, 252, 252, 252, 252, 252, 223],
                [221, 252, 252, 221, 252, 252, 221],
                [221, 252, 252, 112, 223, 252, 221],
                [221, 252, 252, None, 223, 252, 223],
                [221, 252, 252, 112, 221, 252, 223],
                [112, 221, 221, 112, 112, 221, 221],
            ],
        }
    },
    "T": {
        "advance": 6,
        "width": 5,
        "bitmap": {
            "normal": [
                [0, 0, 0, 0, 0],
                [None, None, 0, 15, None],
                [None, None, 0, 17, None],
                [None, None, 0, 17, None],
                [None, None, 0, 17, None],
                [None, None, 0, 17, None],
                [None, None, 0, 17, None],
                [None, None, 0, 15, None],
                [None, None, 0, 46, None],
            ],
            "pressed": [
                [None, 221, 221, 221, 221, 221],
                [None, 252, 252, 252, 252, 252],
                [None, 112, 221, 252, 223, 112],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 252, 223, None],
                [None, None, 221, 221, 221, None],
            ],
        }
    },
    "U": {
        "advance": 8,
        "width": 7,
        "bitmap": {
            "normal": [
                [17, 0, None, None, None, None, 0],
                [17, 0, None, None, None, None, 0],
                [None, 0, None, None, None, None, 0],
                [None, 0, None, None, None, None, 0],
                [None, 0, None, None, None, None, 0],
                [17, 0, None, None, None, None, 0],
                [None, 0, None, None, None, None, 0],
                [None, 0, 15, None, None, None, None],
                [None, 72, 0, 0, 0, 72, None],
            ],
            "pressed": [
                [112, 221, 221, 221, None, None, 221, 221],
                [221, 223, 252, 223, None, None, 221, 223],
                [112, 223, 252, 221, None, None, 221, 252],
                [None, 223, 252, 221, None, None, 221, 252],
                [None, 223, 252, 221, None, None, 221, 252],
                [None, 223, 252, 221, None, None, 221, 252],
                [None, 223, 252, 221, None, None, 221, 252],
                [None, 221, 252, 221, None, None, 221, 252],
                [None, 221, 252, 223, 112, 112, 221, 252],
                [None, 221, 223, 252, 252, 252, 223, 112],
                [None, None, 112, 221, 221, 221, 112, None],
            ],
        }
    },
}

# The second-R cell (rel x 27..34 of the composed string): the font
# carries one R, drawn from the first baked R, so the second baked R
# -- which differs in a few AA pixels and carries an extra right-edge
# column at rel x 34 -- is excluded from --check's strict comparison
# and reported separately. (The census reports the full numbers.)
SECOND_R_CELL = range(27, 35)
INK_NORMAL = {0, 15, 17, 46, 72}
INK_PRESSED = {221, 223, 112, 207, 252}


def validate():
    """The grids are the font: reject anything malformed before writing."""
    assert 1 <= CAP_HEIGHT <= 32
    assert 0 <= DESCENDER_ROWS <= 8
    assert 1 <= SPACE_ADVANCE <= 32
    assert GLYPHS, "no glyphs"
    for ch, g in GLYPHS.items():
        assert len(ch) == 1 and ch.isupper(), f"bad glyph key {ch!r}"
        assert 1 <= g["advance"] <= 64, f"{ch}: bad advance"
        assert 1 <= g["width"] <= 64, f"{ch}: bad width"
        normal = g["bitmap"]["normal"]
        pressed = g["bitmap"]["pressed"]
        assert len(normal) == CAP_HEIGHT, f"{ch}: normal has {len(normal)} rows"
        assert len(pressed) == CAP_HEIGHT + DESCENDER_ROWS, (
            f"{ch}: pressed has {len(pressed)} rows")
        for row in normal:
            assert len(row) == g["width"], f"{ch}: normal row width {len(row)}"
        widths = {len(row) for row in pressed}
        assert len(widths) == 1, f"{ch}: pressed rows differ in width {widths}"
        for state in (normal, pressed):
            for row in state:
                for v in row:
                    assert v is None or 0 <= v <= 255, f"{ch}: bad value {v!r}"
    # The composed strings must fit their baked spans (see --check).
    assert sum(GLYPHS[c]["advance"] for c in "RETUR") + GLYPHS["N"]["width"] == 41
    assert (sum(GLYPHS[c]["advance"] for c in "RETUR")
            + len(GLYPHS["N"]["bitmap"]["pressed"][0]) == 42)


def build_font():
    validate()
    return {
        "name": NAME,
        "cap_height": CAP_HEIGHT,
        "descender_rows": DESCENDER_ROWS,
        "space_advance": SPACE_ADVANCE,
        "provenance": PROVENANCE,
        "glyphs": GLYPHS,
    }


def load_sprite(state):
    data = (RETURN_DIR / (state + ".bin")).read_bytes()
    w, h = struct.unpack_from("<hh", data, 0)
    return w, h, list(data[16:16 + w * h])


def compose(text, state):
    """Compose text from the grids: {(x, y): palette index}."""
    out = {}
    x = 0
    for ch in text:
        g = GLYPHS[ch]
        for y, row in enumerate(g["bitmap"][state]):
            for dx, v in enumerate(row):
                if v is not None:
                    out[(x + dx, y)] = v
        x += g["advance"]
    return out


def check():
    """Verify the hand-drawn grids against the derived repo sprites."""
    ok = True
    for state, ink, rows in (("normal", INK_NORMAL, range(8, 17)),
                             ("pressed", INK_PRESSED, range(8, 19))):
        w, h, px = load_sprite(state)
        baked = {(x - 17, y - 8): px[y * w + x]
                 for y in rows for x in range(14, 61)
                 if px[y * w + x] in ink}
        got = compose("RETURN", state)
        strict_baked = {k: v for k, v in baked.items()
                        if k[0] not in SECOND_R_CELL}
        strict_got = {k: v for k, v in got.items()
                      if k[0] not in SECOND_R_CELL}
        missing = sorted(set(strict_baked) - set(strict_got))
        extra = sorted(set(strict_got) - set(strict_baked))
        wrong = sorted(k for k in set(strict_baked) & set(strict_got)
                       if strict_baked[k] != strict_got[k])
        second_r = sorted(set(baked) | set(got))
        second_r = [k for k in second_r if k[0] in SECOND_R_CELL
                    and baked.get(k) != got.get(k)]
        inter = len(set(baked) & set(got))
        union = len(set(baked) | set(got))
        print(f"{state}: baked={len(baked)} font={len(got)} "
              f"IoU={inter / union:.3f} ({inter}/{union})")
        print(f"  strict (excl. second-R cell): missing={len(missing)} "
              f"extra={len(extra)} wrong-value={len(wrong)}")
        for k in missing:
            print(f"    missing baked {k}={baked[k]}")
        for k in extra:
            print(f"    extra font {k}={got[k]}")
        for k in wrong:
            print(f"    value {k}: baked {baked[k]} vs font {got[k]}")
        print(f"  second-R cell diffs (known single-R variance): "
              f"{len(second_r)} {second_r}")
        if missing or extra or wrong:
            ok = False
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true",
                        help="verify the grids against the derived sprites")
    args = parser.parse_args()
    font = build_font()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(font, indent=1) + "\n")
    print(f"wrote {args.output} ({len(GLYPHS)} glyphs)")
    if args.check:
        sys.exit(0 if check() else 1)


if __name__ == "__main__":
    main()
