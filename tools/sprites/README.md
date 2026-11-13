# orion2re-owned derived sprites

The colony screens' separable sprites cannot be drawn on a menu
screen as-is: they are authored for the colony palette -- the palette
embedded in COLSUM.LBX entry 0, which the colony screen installs
before its sprites draw -- and some carry their label baked in.
(That palette is not FONTS.LBX entry 2: the two agree on the grey
ramp but differ completely in the blue range, where the colony
screen's blue field and labels live.) `../extract_sprite.py`
derives redeployable frames from them (palette remap, label blanking,
per-state overrides); its module docstring documents the full
pipeline. The derived frames ship in
`data/base/sprites/<sprite-id>/<state>.bin` and are part of
`base.zip`; the runtime loads them with
`sprite_art::Load_Sprite_Frame` and draws them with
`sprite_art::Draw_Sprite_Frame` /
`sprite_art::Draw_Sprite_Frame_Stretched` (flat middles),
`sprite_art::Draw_Sprite_Frame_Three_Slice` (fixed slices, one
tiled face column -- the sort tabs) or
`sprite_art::Draw_Sprite_Frame_Stretched_Resampled` (gradient
middles, which must span the width once, not tile).

Contents:

- `sprites.json` — the manifest: one entry per sprite, with the
  source LBX entry, the source/target palette ids, the expected size,
  the frame each state takes, and (when the label is baked in) the
  blank rectangle. This is the source of truth for the derivation.
  Adding a sprite means adding a manifest entry and re-running the
  tool; the game only needs the sprite id wired to
  `sprite_art::Load_Sprite_Frame`.

- `return` — COLSUM.LBX entry 1, the colony RETURN button, 85x26,
  colony palette -> menu palette. Frame 0 is the button at rest (grey
  bevel, dark "RETURN"); frame 1 is its pressed state (the text turns
  orange). No blanking: the baked label is the label the settings
  screen wants. The button's right edge is flat black as authored
  (it blends into the colony background there); the derived frame
  keeps it, and the settings screen's black background behind the
  button renders it the same way. The same frame also serves
  every metal button at any width, cut into three slices at
  runtime (sprite_art::Draw_Sprite_Frame_Three_Slice, via
  ui_kit::Button): the left dome (columns 0..16) and the right
  dome (columns 60..84) as-is, and a single text-free face column
  tiled across the middle -- column 58 for the rest frame, column
  59 for the pressed frame -- with the label rendered at runtime
  in the game font. Column 58 is the only column that is the flat
  face (238) for the whole text body (rows 8..16) in the rest
  frame; column 59 is the same for the pressed frame, and column
  58 in the pressed frame catches the orange text's fringe
  (221/223) and must not be used there. The caps are copied
  exactly, so a metal button's chrome is pixel-identical to the
  RETURN's; the middle differs from the RETURN's middle only
  where the RETURN has text (correctly omitted) and in the face's
  dithered bands (rows 1, 7, 17, 20 rest; row 17 pressed), where
  the tiled column holds one shade of the dither. Face-match of
  the tiled middle against the RETURN's non-text face pixels:
  622/897 (69.3%) rest, 710/731 (97.1%) pressed -- the rest
  mismatches are mostly text-antialiasing pixels that a blank
  button must not have.

- `sort-name`, `sort-population`, `sort-food` — the colony sort
  tabs (Name 50x24, Population 78x24, Food 42x24), grabbed from the
  rendered colony screen, not extracted from the LBX: the settings
  screen's three category tabs each draw their own tab's frame
  (`../derive_sort_tabs.py`; the screengrab is the source, so the
  frames are the colony's own pixels -- the Name frames it derives
  are byte-identical to the old LBX-derived `selector-frame`, which
  retired with them). The baked label is blanked per state so the
  settings screen can render its own labels with the game font:
  `normal` takes frame 0, blanked with a flat fill of the dark-navy
  interior (rows 4-19, columns 4 to the right edge); `active` takes
  frame 1 -- the selected sort column -- blanked by copying each
  pixel from the same column at label-free row 5, so the field
  continues behind the new label. The blue ramp has no close hue
  in the menu palette, so the active state overrides it onto the
  menu's own blues (50/84/103/138). Each frame carries a fixed
  ~20 px bright ramp on each side and a flat dark-navy middle, so
  the draw three-slices with 20 px fixed slices and one tiled
  middle column per tab (Name 26, Population 38, Food 21) -- the
  ramps stay 1:1 at any tab width; the selected frame's field is
  flat in the middle once the ramps are fixed slices, so nothing
  resamples. The gaps between tabs show the window band's
  recessed channel, measured from the same screengrabs (one
  palette index per row: 239/240, 234, 15, 84, 50 x16, 84, 94,
  29, 239 -- row 0 follows the strip-wide arch). The tabs sit
  1 px apart at y=446, like the colony's sort buttons, and their
  labels are font style 2: the baked colony labels measure as
  style 2, with a white face at rest and a blue-white face when
  active (see src/game/ui_kit.h, Colony_Theme).

- `sort-population-wide`, `sort-population-mid`, `sort-food-wide`,
  `value-box-wide` — the sort-tab and value-box frames stretched
  wider by repeating a flat middle column (`../stretch_sprite.py`;
  the tool asserts the repeat column is identical to its neighbour
  in every row, so the stretch is pixel-exact colonies art with no
  seam): Population 78 -> 160 px (column 40, x82) for the
  Internals tab and 78 -> 104 px (column 40, x26) for the Gameplay
  tab, Food 42 -> 80 px (column 21, x38) for the Mods tab, the
  value box 85 -> 104 px (column 32, x19) for the row dropdown/edit
  values. The tabs fill the bottom bar to MORE (Tom, 2026-11-13)
  and the widened value box fits the widest value, "overview"
  (74 px), which the 85 px frame truncated to "overvi...". MORE
  keeps the 85 px `value-box`: its zone is 99 px and the wide
  frame would overflow it.

- `colony-background` — COLSUM.LBX entry 0, the colony screen's
  baked 640x480 background, colony palette -> menu palette. It
  carries the frame, the header plates (NAME / FARMERS / WORKERS /
  SCIENTISTS / BUILDING, each drawn individually with its own
  per-row texture -- no repeating pattern reproduces them), the
  list panel, the info area, the SORT band (SORT label + icon,
  the seven sort tabs, RETURN) and the scrollbar track (a black
  channel at x 621..625). The colony render uses it as its base.
  The settings screen does NOT: it paints its own background
  with the kit (frame, panel, plates, scrollbar, info box,
  bottom band, all code-drawn), because painting over the baked
  background left seams -- a black gap beside the VALUE plate, a
  repeating pattern from a tiled plate face, speckled texture
  behind the description text, notches in the frame, wrong pixels
  around the scroll arrows. The .bin is NOT committed (extract
  from the LBX, do not copy bitmaps to git). The loader
  (`sprite_art::Load_Colony_Background`) and the manifest entry
  stay -- the loader is pinned by its test -- but nothing loads
  the background at run time now. The manifest entry is the
  source of truth for the derivation (entry 0, frame 0, embedded
  colony palette -> installed palette, index 0 preserved, fail
  over distance 64).

- `settings-background` — AUTHORED, not extracted from an LBX:
  the settings screen's 640x480 background, drawn from the
  colony frame verbatim (dark interior, the colony's 8px
  chiseled row dividers at the settings row boundaries, blank
  header plates, navy description band, and the colony SORT
  band's textured strip -- chiseled top edge, medium-grey
  textured face (the strip beside the tabs plate, not the tabs
  plate's dark navy face) -- across the bottom bar with the
  "Options" label baked in, the 3px chiseled vertical divider
  between the SETTING and VALUE columns (a transplant of the
  colonies divider itself, sampled via a piecewise-linear map
  anchored on the row-divider band centres, so it stays continuous
  through every crossing with the colonies' bright "+" bloom),
  and the VALUE plate's
  left edge straightened to x=506, with the vertical divider at
  x=502..504 (the VALUE column widened to 108 px so "overview"
  fits, Tom, 2026-11-13));
  the SETTING/VALUE text is rendered by code on the blank
  plates. No entry in `sprites.json` (that manifest is for
  LBX-extracted sprites only). Source PNG and converter live
  in `settings-background/` here; reproduce with
  `settings-background/png_to_bin.py settings-bg-v8.png
  MAINMENU.LBX data/base/sprites/settings-background/normal.bin`
  (settings-bg-v8.png is settings-bg-v7.png with the scrollbar
  transplanted verbatim from the colonies screen (pure-black
  track, divider, frame edge; row dividers stop at the track),
  every row-divider band repainted from one colonies profile
  (uniform across the width, no left/right step, no spurious
  "+" blob at x=599), the description-band top border repainted
  from one profile so it wraps continuously (the vertical divider
  stops at y=340), and the list area cleaned to flat face, by
  `build_bg_v9.py`, kept with the round screenshots)
  (remaps to the menu palette, excluding the transient navy
  ramp 160-223 and vivid blue 253 that Install_Background
  overwrites at runtime). The settings screen draws it at
  640x480 and falls back to its generated background at other
  resolutions or when the file is missing.

- `scroll-arrow-up`, `scroll-arrow-down` — COLSUM.LBX entries 2
  (10x20) and 3 (11x22), the scrollbar's up/down arrow buttons,
  colony palette -> menu palette. Only the rest state (frame 0)
  is derived: the pressed frame (frame 1) carries the vivid
  colony blue 38 (88, 84, 240), which has no close hue in the
  menu palette (nearest is 108 away, past the fail distance of
  64), so it cannot be remapped. The rest frames remap cleanly
  (the blues 58/59/60 onto the menu's 84/111/138; the brightest
  colony blue 59 lands on 111 at distance 35.3, muted against
  the real (28, 28, 96)). The arrows are the colony screen's
  own buttons, drawn by the field system at (619, 15) and
  (619, 316) -- above and below the scroll track (y 40..311),
  not at the kit Scroll_Bar's track ends -- so the colony render
  draws them at those positions; the settings screen draws its own arrows with the kit
  (Scroll_Bar::Draw_Arrows, code-drawn triangles in the theme's colours)
  at the same positions and does not use these sprites.

## What cannot be derived from game art

- Border, list panel, header plates, description box: the
  colony's are baked into the 640x480 colony background art
  (COLSUM.LBX entry 0), so no separable sprite exists for
  them -- and the settings screen no longer renders on that
  background (see `colony-background` above). They now come
  from the authored `settings-background` bitmap instead (see
  above); the toggle squares and the scrollbar thumb stay
  kit-drawn. The thumb is dynamic (it moves with the scroll
  position), so it stays kit-drawn.
- Dropdown popup: no popup-window sprite exists; the game's own
  modal lists are engine-drawn. The resolution popup stays
  engine-drawn.
- Star buy buttons (COLSUM.LBX entries 11-20): purchase actions
  with two frames (not bought / bought), not on/off toggles. Not
  used; the settings toggles stay the engine-drawn squares.
- Scrollbar arrow pressed states (COLSUM.LBX entries 2/3, frame
  1): the vivid colony blue 38 has no close hue in the menu
  palette (see `scroll-arrow-up` / `scroll-arrow-down` above).
  Only the rest states are derived.

Reproduce everything with `tools/extract_sprite.py --lbx COLSUM.LBX
--fonts-lbx FONTS.LBX --mainmenu-lbx MAINMENU.LBX`, and verify with
the same command plus `--check`. The tool needs the user's original
game data; the committed .bin files are the checked-in result. The
settings background reproduces with
`settings-background/png_to_bin.py` from the PNG beside it (see
`settings-background` above). The
sort tabs reproduce with `tools/derive_sort_tabs.py --game <game
data> --grabs <colony screenshots> --out data/base/sprites`, which
needs the colony-screen screengrabs (not committed) and the same
game data. The widened tabs and value box reproduce with
`tools/stretch_sprite.py --in <source .bin> --column <c> --repeat
<n> --out <out .bin>` (parameters in `sprites.json`); verify with
the same command plus `--check`.

- `dropdown-closed` — the collapsed dropdown's own button, built
  from scratch by `build_dropdown_closed.py` (104x26, menu
  palette), not a reuse of the value box: the value-box face and
  border language, so it matches its sibling edit buttons, with
  a darker triangle zone at its right end (the rightmost 14 px,
  palette 233) holding a 7x4 header-white (palette 250)
  down-triangle, right edge 4 px inside the frame, vertically
  centred. White, not black: a black triangle sat on the frame's
  dark right-edge bevel and was invisible, and the widest value
  ("overview") leaves no room for a dark triangle on the light
  face. It has to live in the button art -- the field system
  redraws button fields after the screen's auto-function, so a
  triangle drawn at runtime is covered by the button (a solid
  blue rect over a button showed only 111 of 2704 px). Verify
  with `build_dropdown_closed.py --check` (rebuilds and
  byte-compares). `extract_sprite.py` skips it (source
  "screen", like the other screen-derived sprites). The settings
  screen uses it for dropdown rows only; edit rows keep the
  plain wide box.

- `dropdown-border-h` / `dropdown-border-v` -- the expanded dropdown
  window's border, a BITMAP like the one drawn on the background all
  around the screen (the outer frame: riveted, textured, chiseled),
  just thinner (Tom, 2026-11-13). The screen frame is ~8 px thick;
  these strips are 5 px, derived by compressing the frame's own 8 px
  edge profile (sampling rows/cols [0, 2, 4, 5, 7] in outer-to-inner
  order), so the border reads as the same border family, only
  thinner -- not a code-drawn chisel. Built by
  `build_dropdown_border.py` from the authored background
  (`settings-background/settings-bg-v8.png`): the horizontal sprite
  (256x10) carries the top strip (rows 0..4, frame rows 0..7) and the
  bottom strip (rows 5..9, frame rows 472..479), the vertical sprite
  (10x256) the left strip (cols 0..4, frame cols 0..7) and the right
  strip (cols 5..9, frame cols 632..639), each 256 px long from the
  middle of its edge (x/y 64..319), away from the corners, so it
  carries the frame's natural rivet texture. RGB is remapped to
  menu-palette indices with the png_to_bin rule (MAINMENU.LBX entry
  2, excluding the transient navy ramp and vivid blue), so the strips
  are pixel-identical to the frame they came from. The draw code tiles
  the strips if a window ever exceeds 256 px. Verify with
  `build_dropdown_border.py --check --lbx <mainmenu.lbx>` (rebuilds
  and byte-compares).
