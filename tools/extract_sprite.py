#!/usr/bin/env python3
"""Extract orion2re-owned sprite frames from the original game art.

The colony screens' art is baked: most panels, plates and boxes live
inside a 640x480 background sprite, and the separable sprites carry
their labels painted in. A settings screen cannot reuse that art
as-is. This tool is the systemic answer: it derives orion2re-owned
sprite frames from the original LBX sprites, remapping them from the
palette they were authored for to the palette of the screen that will
draw them, and ships them as native bitmap frames in base.zip.

Pipeline (per sprite, per state):

1.  Decode. The tool reads one frame of one sprite entry from a
    source LBX. A sprite entry is a 12-byte animation header, a frame
    offset table (frame_count + 1 uint32 offsets), then one RLE stream
    per frame: runs of (pixel_count, skip) with pixel_count == 0
    skipping rows, the same layout the engine's animated-sprite draw
    uses. The decoded grid is palette indices in the source palette.
    Frame 0 is the button at rest; frame 1 is its pressed/selected
    state (RETURN's text turns orange; the sort button's frame 1
    restyles the whole button: blue gradient field, blue bevel,
    blue-white label).

2.  Blank. A sprite whose label is baked in cannot take a new label
    until the label is gone. The manifest names a blank rectangle per
    state (interior rows, from a left column to the right edge); the
    tool asserts the rectangle holds label pixels before blanking, so
    a sprite that does not match fails loudly instead of producing a
    subtly wrong frame. Two modes: "fill" fills the rectangle with one
    index (the rest frame, whose field is flat); "column" copies each
    blanked pixel from the same column at a label-free reference row,
    which continues a horizontal gradient behind the new label
    instead of punching a flat hole in it (the selected frame's field
    is a blue gradient, brightest at the edges, dark in the middle).
    The RETURN sprite needs no blanking: its baked label is the label
    the settings screen wants.

3.  Remap. Each used source index is translated to the nearest RGB
    match in the target palette (Euclidean distance in 8-bit RGB).
    Index 0 is preserved as 0: it is the transparency anchor in
    BITMAP draws, and black in both palettes. A distance over 32 is
    reported as a warning (the target palette has no close hue; the
    nearest match is still used), over 64 fails the run. A state may
    also carry per-index overrides (source index -> target index),
    which replace the nearest match: the selector frame's active state
    is frame 1, whose blue ramp has no close hue in the menu palette
    (the bright blues map at distance 50-125), so the state overrides
    the ramp onto the menu's own blues (50/84/103/138, face 250) and
    the dark-navy field (57) onto the menu dark navy (50). The tool
    prints the mapping table with distances, so every match is visible
    at review time.

4.  Output. Each frame is written as a native orion2re bitmap: the
    s_animation_header layout produced by bitmap::Create_Bitmap_Header
    (12-byte header, one frame offset of 16, width*height palette-index
    bytes, padded to a multiple of 4, draw mode BITMAP). The game loads
    these with sprite_art::Load_Sprite_Frame, which validates the
    header before use; see src/game/sprite_art.h. Files land in
    data/base/sprites/<sprite-id>/<state>.bin and ship in base.zip.

Palettes: the colony screens are authored for the palette embedded
in their own background art, COLSUM.LBX entry 0 (a 256-entry palette
sitting after the 12-byte header and the frame table; the background
draw installs it with animate::Draw_Palette before any colony sprite
is drawn). fonts::Load_Palette(1) does load FONTS.LBX entry 2 first,
but the background's embedded palette replaces it wholesale, so
FONTS.LBX entry 2 is NOT what the colony screen renders with. The
two agree on the grey ramp (indices 80-104) but differ completely
in 43-62: FONTS.LBX entry 2 has black there, the embedded palette
has the blue ramp that makes the selected sort tab blue (43 is the
bright blue-white label face, 50-56/60/62 the field gradient, 57
the dark-navy field, 58 the dark blue-grey inner line). The tool
therefore reads the colony palette from COLSUM.LBX entry 0's
embedded palette (manifest "embedded"). Each palette entry is one
s_palette_entry record {changed, r, g, b} in 6-bit values. The menu
screens do NOT render with FONTS.LBX entry 1: the main-menu
background (MAINMENU.LBX entry 2) carries its own embedded palette,
which the background draw installs, and that palette stays in effect
for the settings screen. The tool therefore reads the menu palette
from MAINMENU.LBX entry 2 at offset 24 (after the 12-byte header and
the frame table). The proof is in the RGB values: index 138 is the
Auto Build blue (52, 48, 136) and 230-249 the blue-grey ramp there,
while FONTS.LBX entry 1 has brown at 138 and pure green at 230-249.
The colony grey ramp (indices 80-104) maps to the menu palette's own
grey family (indices 10-17, 230-248) at distance <= 11, so the
remapped buttons keep their grey look and harmonize with the
settings chrome, which already uses 230-249.

Adding a sprite: add an entry to tools/sprites/sprites.json with the
sprite id, the source LBX entry, the source/target palette ids, the
expected size, the frames each state takes, and (when the label is
baked in) the blank rectangle; then run this tool. No game-code
change is needed beyond wiring the sprite id to
sprite_art::Load_Sprite_Frame.

Reproducing: `tools/extract_sprite.py --lbx COLSUM.LBX --fonts-lbx
FONTS.LBX --mainmenu-lbx MAINMENU.LBX` regenerates every frame.
`--check` regenerates to a temporary directory and byte-compares
against the committed files, so drift in the source art or the remap
is caught instead of silently shipped. The tool needs the user's
original game data (COLSUM.LBX, FONTS.LBX, MAINMENU.LBX); the
committed .bin files are the checked-in result of that extraction.
"""

import argparse
import json
import struct
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TOOL_DIR = REPO_ROOT / "tools" / "sprites"
MANIFEST_PATH = TOOL_DIR / "sprites.json"
OUTPUT_ROOT = REPO_ROOT / "data" / "base" / "sprites"

BITMAP_DRAW_MODE = 1
BITMAP_FRAME_DATA_OFFSET = 16
REMAP_WARN_DISTANCE = 32.0
REMAP_FAIL_DISTANCE = 64.0

# A palette is 256 s_palette_entry records {changed, r, g, b} in
# 6-bit values: 1024 bytes.
PALETTE_SIZE = 1024


def load_lbx(path):
    """Return (data, entry offsets) for an LBX file."""
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


def frame_offsets(payload):
    """Frame start offsets from a sprite entry's offset table."""
    frame_count = struct.unpack_from("<h", payload, 6)[0]
    if frame_count <= 0:
        raise ValueError("sprite entry has no frames")
    return struct.unpack_from("<%dI" % (frame_count + 1), payload, 12)


def decode_frame(payload, width, height, start):
    """Decode one RLE frame to a grid of palette indices."""
    stream = payload[start:]
    start_y = struct.unpack_from("<H", stream, 2)[0]
    image = [[0] * width for _ in range(height)]
    pos, y = 4, start_y
    while y < height:
        pixel_count, skip = struct.unpack_from("<hh", stream, pos)
        pos += 4
        if pixel_count == 0:
            y += skip
        else:
            image[y][skip:skip + pixel_count] = stream[pos:pos + pixel_count]
            pos += pixel_count
            if pos & 1:
                pos += 1
    return image


def load_palette(lbx_files, spec):
    """Read a palette from an LBX entry (offset 0 unless specified)."""
    path = lbx_files[spec["lbx"]]
    data, offsets = load_lbx(path)
    payload = lbx_entry(data, offsets, spec["entry"])
    if spec.get("embedded"):
        # Palette embedded in a sprite entry, after the 12-byte header
        # and the frame table: a start/count header, then count
        # s_palette_entry records. (This is how a screen background
        # carries its palette; animate::Set_Animation_Palette installs
        # it over whatever Load_Palette loaded.)
        frame_count = struct.unpack_from("<h", payload, 6)[0]
        header_at = 12 + (frame_count + 1) * 4
        start, count = struct.unpack_from("<hh", payload, header_at)
        raw = payload[header_at + 4:header_at + 4 + count * 4]
        if len(raw) < count * 4:
            raise ValueError(
                f"{path} entry {spec['entry']}: embedded palette is "
                f"{len(raw)} bytes, need {count * 4}")
        palette = [(0, 0, 0)] * 256
        for index in range(count):
            palette[start + index] = (raw[4 * index + 1] * 4,
                                      raw[4 * index + 2] * 4,
                                      raw[4 * index + 3] * 4)
        return palette
    offset = spec.get("offset", 0)
    raw = payload[offset:offset + PALETTE_SIZE]
    if len(raw) < PALETTE_SIZE:
        raise ValueError(
            f"{path} entry {spec['entry']}: palette at offset {offset} "
            f"is {len(raw)} bytes, need {PALETTE_SIZE}")
    return [(raw[4 * index + 1] * 4,
             raw[4 * index + 2] * 4,
             raw[4 * index + 3] * 4) for index in range(256)]


def nearest_index(rgb, palette):
    """Nearest palette index to rgb (Euclidean distance in 8-bit RGB)."""
    best, best_dist_sq = 0, None
    for index, candidate in enumerate(palette):
        dist_sq = sum((a - b) ** 2 for a, b in zip(rgb, candidate))
        if best_dist_sq is None or dist_sq < best_dist_sq:
            best, best_dist_sq = index, dist_sq
    return best, best_dist_sq ** 0.5


def remap_grid(grid, source_palette, target_palette, overrides):
    """Translate a grid of source-palette indices to target indices.

    Returns (remapped grid, mapping table {source: (target, distance)}).
    Index 0 is preserved as 0 (transparency anchor). Overrides replace
    the nearest match for the listed source indices.
    """
    used = sorted({value for row in grid for value in row})
    mapping = {}
    for source in used:
        if source == 0:
            mapping[source] = (0, 0.0)
        elif str(source) in overrides:
            mapping[source] = (overrides[str(source)], 0.0)
        else:
            target, distance = nearest_index(source_palette[source],
                                             target_palette)
            if distance > REMAP_FAIL_DISTANCE:
                raise ValueError(
                    f"index {source} {source_palette[source]} maps to "
                    f"{target} {target_palette[target]} at distance "
                    f"{distance:.1f} (> {REMAP_FAIL_DISTANCE}); the "
                    f"palettes do not cover this sprite")
            mapping[source] = (target, distance)
    remapped = [[mapping[value][0] for value in row] for row in grid]
    return remapped, mapping


def apply_blank(grid, blank, width, height, sprite_id, state):
    """Remove a baked label from a decoded frame, in place.

    Modes: "fill" (default) fills the rectangle with one index;
    "column" copies each blanked pixel from the same column at a
    label-free reference row ("src_row"), which continues a
    horizontal gradient behind the new label instead of punching a
    flat hole in it. Both modes fail loudly when the rectangle holds
    no label pixels, so a sprite that does not match the manifest
    fails instead of producing a subtly wrong frame. (For "column"
    the check is weaker by construction -- a gradient field already
    differs row to row -- but a flat field with no label still fails.)
    """
    mode = blank.get("mode", "fill")
    x0, y0 = blank["x0"], blank["y0"]
    x1 = blank.get("x1", width - 1)
    y1 = blank["y1"]
    if not (0 <= x0 <= x1 < width and 0 <= y0 <= y1 < height):
        raise ValueError(
            f"sprite {sprite_id!r}: blank rect out of bounds")
    if mode == "fill":
        fill = blank["fill"]
        label_pixels = sum(
            1 for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)
            if grid[y][x] != fill)
        if label_pixels == 0:
            raise ValueError(
                f"sprite {sprite_id!r}: blank rect holds no label "
                f"pixels; the sprite does not match the manifest")
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                grid[y][x] = fill
    elif mode == "column":
        src_row = blank["src_row"]
        if y0 <= src_row <= y1:
            raise ValueError(
                f"sprite {sprite_id!r}: blank src_row {src_row} is "
                f"inside the blank rect {y0}..{y1}")
        if not 0 <= src_row < height:
            raise ValueError(
                f"sprite {sprite_id!r}: blank src_row {src_row} out "
                f"of bounds")
        changed = 0
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if grid[y][x] != grid[src_row][x]:
                    changed += 1
                grid[y][x] = grid[src_row][x]
        if changed == 0:
            raise ValueError(
                f"sprite {sprite_id!r}: blank rect holds no label "
                f"pixels; the sprite does not match the manifest")
    else:
        raise ValueError(
            f"sprite {sprite_id!r}: unknown blank mode {mode!r}")


def frame_bytes(frame):
    """Encode an index grid as a native orion2re bitmap frame."""
    height = len(frame)
    width = len(frame[0])
    pixel_count = width * height
    size = (pixel_count + BITMAP_FRAME_DATA_OFFSET + 3) // 4 * 4
    header = struct.pack(
        "<hhhhBBBbI", width, height, 0, 1, 0, 0, 0, BITMAP_DRAW_MODE,
        BITMAP_FRAME_DATA_OFFSET)
    pixels = bytes(value for row in frame for value in row)
    return header + pixels + bytes(size - len(header) - len(pixels))


def load_manifest():
    with open(MANIFEST_PATH, encoding="utf-8") as handle:
        manifest = json.load(handle)
    sprites = manifest.get("sprites", [])
    if not sprites:
        raise ValueError(f"{MANIFEST_PATH}: no sprites defined")
    return manifest, sprites


def extract_all(lbx_path, lbx_files, output_root):
    """Derive every frame in the manifest. Returns the written paths."""
    manifest, sprites = load_manifest()
    lbx_data, lbx_offsets = load_lbx(lbx_path)
    palettes = {}

    def palette(palette_id):
        if palette_id not in palettes:
            palettes[palette_id] = load_palette(
                lbx_files, manifest["palettes"][palette_id])
        return palettes[palette_id]

    written = []
    for sprite in sprites:
        sprite_id = sprite["id"]
        # Screen-grabbed sprites (source "screen") are derived by their
        # own tool (derive_sort_tabs.py), not by this LBX extractor.
        if sprite.get("source") == "screen":
            continue
        payload = lbx_entry(lbx_data, lbx_offsets, sprite["entry"])
        width, height = struct.unpack_from("<hh", payload, 0)
        expected = sprite.get("size")
        if expected is not None and [width, height] != expected:
            raise ValueError(
                f"sprite {sprite_id!r}: entry {sprite['entry']} is "
                f"{width}x{height}, manifest expects {expected}")
        offsets = frame_offsets(payload)

        blank = sprite.get("blank")
        source_palette = palette(sprite["source_palette"])
        target_palette = palette(sprite["target_palette"])
        for state, state_spec in sprite["states"].items():
            if isinstance(state_spec, int):
                frame_index, overrides = state_spec, {}
                state_blank = blank
            else:
                frame_index = state_spec["frame"]
                overrides = state_spec.get("overrides", {})
                state_blank = state_spec.get("blank", blank)
            grid = decode_frame(payload, width, height, offsets[frame_index])
            if state_blank is not None:
                apply_blank(grid, state_blank, width, height, sprite_id,
                            state)
            remapped, mapping = remap_grid(
                grid, source_palette, target_palette, overrides)
            print(f"{sprite_id}/{state}: {width}x{height} from "
                  f"{manifest['source_lbx']} entry {sprite['entry']} "
                  f"frame {frame_index}, palette "
                  f"{sprite['source_palette']} -> "
                  f"{sprite['target_palette']}")
            for source in sorted(mapping):
                target, distance = mapping[source]
                if str(source) in overrides:
                    note = "  <-- override"
                elif distance > REMAP_WARN_DISTANCE:
                    note = "  <-- no close hue in target palette"
                else:
                    note = ""
                print(f"    {source:3d} {source_palette[source]} -> "
                      f"{target:3d} {target_palette[target]} "
                      f"dist {distance:5.1f}{note}")
            path = output_root / sprite_id / f"{state}.bin"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(frame_bytes(remapped))
            written.append(path)
    return written


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Extract orion2re-owned sprite frames from game art.")
    parser.add_argument("--lbx", required=True,
                        help="source LBX with the sprites (e.g. COLSUM.LBX)")
    parser.add_argument("--fonts-lbx", default=None,
                        help="FONTS.LBX (retained for compatibility; "
                             "the colony palette now comes from the "
                             "source LBX's embedded palette)")
    parser.add_argument("--mainmenu-lbx", required=True,
                        help="MAINMENU.LBX, for the menu palette")
    parser.add_argument("--check", action="store_true",
                        help="regenerate to a temp dir and byte-compare "
                             "against the committed files")
    args = parser.parse_args(argv)
    lbx_files = {"MAINMENU.LBX": args.mainmenu_lbx,
                 Path(args.lbx).name.upper(): args.lbx}
    if args.fonts_lbx is not None:
        lbx_files["FONTS.LBX"] = args.fonts_lbx

    if args.check:
        with tempfile.TemporaryDirectory(prefix="orion2re-sprites-") as tmp:
            written = extract_all(args.lbx, lbx_files, Path(tmp))
            manifest, _ = load_manifest()
            runtime = {s["id"] for s in manifest["sprites"]
                       if s.get("runtime")}
            mismatches = []
            for path in written:
                relative = path.relative_to(tmp)
                if relative.parts[0] in runtime:
                    continue
                committed = OUTPUT_ROOT / relative
                if not committed.exists():
                    mismatches.append(f"{relative}: missing committed file")
                elif committed.read_bytes() != path.read_bytes():
                    mismatches.append(f"{relative}: differs from committed file")
            if mismatches:
                print("sprite check failed:", file=sys.stderr)
                for mismatch in mismatches:
                    print(f"  {mismatch}", file=sys.stderr)
                raise SystemExit(1)
            print(f"sprite check passed: {len(written)} frames match")
            return 0

    written = extract_all(args.lbx, lbx_files, OUTPUT_ROOT)
    for path in written:
        print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
