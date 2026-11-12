# Fonts in `data/base/fonts/`

These are orion2re-owned replica fonts, drawn by observation from the
rendered game screens and bundled with the game. They reproduce the
baked letterforms where no runtime engine font can (the baked text is
hand-drawn pixel art, not a font render). No font is a verbatim copy
of game data: the grids are transcriptions of the rendered screen,
verified by each font's --check, and the fonts are generated from
literals in the build scripts -- no LBX file is read to produce them.

## Moo2Menu.json

The main-screen button font. Recreated from the baked MAINMENU.LBX
button labels; see `tools/build_moo2menu_font.py` for its provenance
and regeneration.

## Moo2Button.json

The metal-button label font, used at runtime for the settings
screen's MORE button (and for the RETURN button's label on the
stale-base fallback path). It reproduces the baked RETURN button text
(COLSUM.LBX entry 1) 1:1 where a baked reference exists.

### Why it exists

The RETURN button's text is baked into its derived sprite, and the
baked glyphs are hand-drawn pixel art, not a verbatim engine-font
render (baked R leg 1 px vs engine-font 2 px, E bars 4 px vs 5 px,
U 7 wide vs 6, N diagonal 1 px vs 2 px). The best runtime engine
font (FONTS style 2, spacing 1) only reaches IoU 0.626 against the
baked text. This font is the same answer as Moo2Menu: an
orion2re-owned replica font, bundled with the game, that matches the
baked letterforms.

### How it was made

Drawn by observation from the derived repo sprites
`data/base/sprites/return/{normal,pressed}.bin` and from the rendered
RETURN button -- never by byte-extracting pixels out of any LBX file,
and never by a fresh LBX extraction. The source of truth is
`tools/build_moo2button_font.py`, which holds the hand-drawn glyph
grids as literals and generates this JSON; the script's header
documents, per glyph and per state, exactly how each glyph was
obtained. Run it with `--check` to verify the grids against the
derived sprites, pixel by pixel.

### Glyphs

M, O, R, E, T, U, N. R and E are shared with the baked RETURN text;
T, U, N are also in RETURN and match it 1:1. M and O have no baked
reference at this scale (the colony sort-tab frames carry no baked
label, and the only baked M/O in the repo are Moo2Menu's at QUIT
scale), so they are designed to match: same 2 px stems, 1 px bars
and AA palette, proportions from Moo2Menu's QUIT-scale M and O.

The font carries one R, drawn from the first baked R. The two baked
Rs differ in a few AA pixels (the second R's leg is 1 px core with AA
where the first is 2 px core) and the second R's pressed form carries
an extra right-edge column, so the second R in a rendered RETURN
differs from the baked one: 12 differing positions in the normal
state, 39 in the pressed state (see `--check` output). Everything
outside the second-R cell matches exactly: the composed RETURN is
IoU 0.984 normal and 0.955 pressed against the baked bands, with
zero missing, extra or wrong-value pixels outside that cell.

### Format

Same top-level shape as Moo2Menu.json: `name`, `cap_height` (9),
`descender_rows` (2), `space_advance` (4), `provenance`, `glyphs`.
Each glyph has `advance` (the origin grid, shared by both states),
`width` (the normal state's cell width) and `bitmap` with `normal`
(9 rows) and `pressed` (11 rows). The pressed glyphs are a different
shape from the normal ones, not a recolour: they are 11 rows tall
(the cell is 11 rows: 9 cap + 2 descender), bolder, with a
bright-orange fill (palette 252) inside the strokes, a brown shadow
(112) on the stroke edges, and a single light-tan pixel (207) at the
first R's top-left corner. A pressed bitmap may be wider than
`width` (E's pressed top bar reaches one column into the next cell,
which is blank there for every real neighbour); the runtime uses each
state's actual row length. Both states are top-aligned at the label's
y, so normal glyphs ink rows 8..16 and pressed glyphs rows 8..18 of
the 26 px button, exactly like the baked text.

### Regeneration

```sh
python3 tools/build_moo2button_font.py [--output data/base/fonts/Moo2Button.json] [--check]
```

`--check` composes "RETURN" from the grids and compares it against
the derived repo sprites. It passes only when the composition
matches the baked bands outside the second-R cell (rel x 27..34),
whose diffs are the known single-R variance and are reported
separately. Do not edit the JSON by hand: edit the grids in the
script and regenerate.

### Runtime use

`src/game/ui_kit.cpp` (`Bitmap_Font`) loads this file at runtime
from base.zip via `vfs::fopen("fonts/Moo2Button.json")` and blits the
glyph bitmaps for the button labels, with the pressed-state bitmaps
used while a button is held. Loading is soft: a missing or malformed
file, a missing glyph, or the active (blue-face) state falls back to
the previous style-2 engine-font rendering, never a blank label.

## Moo2Display.json

The colony header-plate label font (cap 8). The colony summary
screen's header plates (NAME, FARMERS, WORKERS, SCIENTISTS, BUILDING)
and the SORT caption carry large display labels -- about 14 px per
character, 8 rows of ink -- but the kit had no such theme font: the
replication rendered them in the tab font (engine font style 2),
which inks the right 8 rows but only about half the width (real
FARMERS 97 px vs kit 53 px). Moo2Display closes that gap: the five
plate labels compose to their measured widths exactly (NAME 58,
FARMERS 97, WORKERS 98, SCIENTISTS 105, BUILDING 87).

### How it was made

Drawn by observation from the rendered colony summary screen
(header plates y 4..31, label ink rows 17..24, game coords). Each
pixel's RGB was mapped to the nearest menu-palette grey (MAINMENU.LBX
entry 2, offset 24; ramp 230-250); indices >= 240 are ink, the rest
transparent. No LBX file was read to produce this font and no sprite
pixels were byte-extracted into it: the grids are transcriptions of
the rendered screen. The source of truth is
`tools/build_moo2display_font.py`, which holds the glyph grids as
literals and generates this JSON.

### Glyphs

19 (A B C D E F G I K L M N O R S T U V W). Every glyph except V is
transcribed from the rendered colony screen; V is designed to match
(no baked V at this scale -- VALUE is a settings-screen plate,
rendered at runtime). The pressed state mirrors normal: header
plates have no pressed state, so the JSON carries the same 8 rows
for pressed (the runtime requires a pressed bitmap; it is never
rendered for plates).

Advances are solved so the five plate labels land on their measured
widths exactly (single advance per glyph; F=13 and D=14 sit 1 px
above their measured gap). The baked labels vary slightly in AA
values between instances of the same letter, so exact palette-index
match is unachievable and is not what the census measures -- the
--check screenshot comparison is binary ink with 1 px tolerance,
the same tolerance the colony census uses.

### Format

Same top-level shape as Moo2Menu.json: `name`, `cap_height` (8),
`descender_rows` (0), `space_advance` (4), `provenance`, `glyphs`.
Each glyph has `advance`, `width` and `bitmap` with `normal` (8 rows)
and `pressed` (8 rows, identical to normal).

### Regeneration

```sh
python3 tools/build_moo2display_font.py [--output data/base/fonts/Moo2Display.json] [--check] [--screenshot PATH]
```

`--check` composes the five plate labels and verifies their widths
against the measured widths. With `--screenshot PATH` (a colony
summary screengrab, 1280x1024 with the 640x480 game centred), it
additionally compares each composed label's ink shape against the
screenshot's label band (binary ink, 1 px tolerance). Do not edit the
JSON by hand: edit the grids in the script and regenerate.

### Runtime use

The theme carries the display font as `Header_Font()` (see
`src/game/ui_kit.h`); the settings screen's header plates
(`draw_header_plate`) and the colony replication render it through
the theme instead of the engine font. Loading is soft, like
Moo2Button: a missing or malformed file falls back to the engine
font, never a blank label.
