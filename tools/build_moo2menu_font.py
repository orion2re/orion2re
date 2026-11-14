#!/usr/bin/env python3
"""Rebuild the Moo2Menu bitmap font from the original game's MAINMENU.LBX.

The baked main-menu button labels (MAINMENU.LBX entries 3-20 and 25-42)
carry the original 1990 menu-button label font, painted into the button
frames. This tool recovers that font as a JSON bitmap font
(data/base/fonts/Moo2Menu.json) that tools/render_button_frames.py blits
verbatim, so re-rendered buttons match the originals.

How each glyph is obtained (see the JSON's provenance field):
- Q, U, I, T, G, A, M, E: byte-exact replicas of the QUIT GAME frames
  (entries 18/19/20). No resampling, no voting: composing "QUIT GAME"
  from the font reproduces the original Quit frames pixel-for-pixel.
- C, D, F, H, L, N, O, P, R, W, Y: majority-consensus of the baked
  labels, resampled to QUIT scale (cap height 11). Each sample is
  extracted as indexed palette values per state, resampled per ink
  class (LANCZOS), and voted pixel-by-pixel; the value is the modal
  ink among samples that have ink there.
- S, Z: designed to match (no S or Z exists in any baked label). Drawn
  at cap height 11 with the same stroke weight and emboss conventions
  as the recovered letters; hover/disabled follow the dominant
  per-state conventions measured from the QUIT letters.

Bitmaps are lists of palette indices (null = background) so the rare
stray values inside glyphs (e.g. 82, 113, 145) survive losslessly.
Glyph cells are 12 rows: 11 cap rows plus 1 descender row (only Q uses
it). Advances are the median measured advance at QUIT scale; the space
advance is 7 px.

Usage: tools/build_moo2menu_font.py MAINMENU.LBX [--output PATH]
Requires Pillow (pip install pillow).
"""

import argparse
import json
import statistics
import struct
import sys
from collections import Counter, deque
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("build_moo2menu_font: Pillow is required (pip install pillow)",
          file=sys.stderr)
    raise SystemExit(2)

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / "data" / "base" / "fonts" / "Moo2Menu.json"

# Baked labels: (MAINMENU.LBX entry, label). Entries are triples
# (normal, hover, disabled); entries 25-42 are the same buttons at the
# smaller size used on other screens.
BUTTONS = [(3, "CONTINUE"), (6, "LOAD GAME"), (9, "NEW GAME"),
           (12, "MULTI PLAYER"), (15, "HALL OF FAME"), (18, "QUIT GAME"),
           (25, "CONTINUE"), (28, "LOAD GAME"), (31, "NEW GAME"),
           (37, "HALL OF FAME"), (40, "QUIT GAME")]
# Face ink values (the glyph silhouette), used to find the label band.
FACE = {19, 22, 37}
# Per-label horizontal scale relative to QUIT, measured from shared-letter
# widths (each label was fit to the 128 px box with its own scale).
HSCALE = {3: 1.11, 6: 0.92, 9: 1.0, 12: 0.80, 15: 0.77, 18: 1.0,
          25: 1.11, 28: 0.92, 31: 1.0, 37: 0.77, 40: 0.935}
# Prior glyph widths at QUIT scale (px), used only to split merged
# components; the font's own widths are measured from the segmentation.
PRI = {"A": 17, "B": 13, "C": 12, "D": 15, "E": 13, "F": 12, "G": 15,
       "H": 15, "I": 6, "J": 10, "K": 14, "L": 11, "M": 19, "N": 15,
       "O": 15, "P": 12, "Q": 15, "R": 13, "T": 13, "U": 15, "V": 14,
       "W": 22, "Y": 12, "Z": 12}
QUIT_LETTERS = set("QUITGAME")
SPACE = 7
# Ink-class priority when resampled classes overlap (face first).
PRIO = [19, 159, 22, 37, 58, 6, 209]
EXTRA = [145, 152, 68, 97, 113, 32, 43, 82, 122, 147, 148, 151, 149, 28, 29]
PRIO_ALL = PRIO + EXTRA
STATES = ("normal", "hover", "disabled")


def load_lbx(path):
    """Load an LBX file; return (data, offsets)."""
    data = Path(path).read_bytes()
    entry_count, magic = struct.unpack_from("<HH", data, 0)
    if magic != 0xFEAD:
        raise ValueError(f"{path}: not an LBX file")
    if entry_count < 43:
        raise ValueError(f"{path}: only {entry_count} entries, need 43")
    return data, struct.unpack_from("<510I", data, 8)


def decode_sprite(data, offsets, index):
    """Decode one LBX sprite entry to (width, height, palette-index bytes).

    The sprite stream is RLE: after the frame offset, start_y, then
    (pixel_count, skip) pairs; pixel_count 0 advances y by skip.
    A raw bitmap read of the entry is wrong; this is the correct decode.
    """
    payload = data[offsets[index]:offsets[index + 1]]
    width, height = struct.unpack_from("<hh", payload, 0)
    frame_offset = struct.unpack_from("<I", payload, 12)[0]
    stream = payload[frame_offset:]
    image = bytearray(width * height)
    start_y = struct.unpack_from("<H", stream, 2)[0]
    pos, y = 4, start_y
    while y < height:
        pixel_count, skip = struct.unpack_from("<hh", stream, pos)
        pos += 4
        if pixel_count == 0:
            y += skip
        else:
            image[y * width + skip:y * width + skip + pixel_count] = \
                stream[pos:pos + pixel_count]
            pos += pixel_count
            if pos & 1:
                pos += 1
    return width, height, bytes(image)


def label_band(width, height, px):
    """Label rows: first/last row containing face ink in x 13..140."""
    rows = [y for y in range(2, height - 2)
            if any(px[y * width + x] in FACE for x in range(13, 141))]
    return min(rows) - 1, max(rows) + 1


def components(width, height, px, y0, y1, bg, x0=13, x1=140):
    """4-connected components of non-panel pixels in the label band."""
    seen = set()
    comps = []
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if px[y * width + x] == bg or (x, y) in seen:
                continue
            queue = deque([(x, y)])
            seen.add((x, y))
            pts = []
            while queue:
                cx, cy = queue.popleft()
                pts.append((cx, cy))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if (nx, ny) in seen:
                        continue
                    if not (x0 <= nx <= x1 and y0 <= ny <= y1):
                        continue
                    if px[ny * width + nx] == bg:
                        continue
                    seen.add((nx, ny))
                    queue.append((nx, ny))
            comps.append(pts)
    comps.sort(key=lambda p: min(x for x, _ in p))
    return comps


def cbox(pts):
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return min(xs), max(xs), min(ys), max(ys)


def col_counts(width, height, px, y0, y1, x0, x1, bg):
    return [sum(1 for y in range(y0, y1 + 1) if px[y * width + x] != bg)
            for x in range(x0, x1 + 1)]


def split_cols(counts, x0, letters, expected):
    """Split an x-range into len(letters) glyphs by DP on span width."""
    n = len(letters)
    total = len(counts)
    inf = float("inf")
    dp = [[inf] * (total + 1) for _ in range(n + 1)]
    par = [[None] * (total + 1) for _ in range(n + 1)]
    dp[0][0] = 0
    for i in range(n):
        for j in range(total + 1):
            if dp[i][j] == inf:
                continue
            for k in range(j + 1, total + 1):
                if i < n - 1 and k >= total:
                    continue
                if abs((k - j) - expected[i]) > 3:
                    continue
                cost = dp[i][j] + abs((k - j) - expected[i])
                if cost < dp[i + 1][k]:
                    dp[i + 1][k] = cost
                    par[i + 1][k] = j
    if dp[n][total] == inf:
        return None
    bounds = []
    j = total
    for i in range(n, 0, -1):
        pj = par[i][j]
        bounds.append((x0 + pj, x0 + j - 1))
        j = pj
    bounds.reverse()
    trimmed = []
    for a, b in bounds:
        cols = [x for x in range(a, b + 1) if counts[x - x0] > 0]
        if not cols:
            return None
        trimmed.append((min(cols), max(cols)))
    return trimmed


def segment_labels(frames):
    """Segment every baked label into glyph boxes.

    Returns {letter: [sample, ...]} where a sample is a dict with the
    button entry, label, box (x0, x1, y0, y1), advance and hscale.
    Components are exact; merged components are split by DP, split
    glyphs are unioned, and the MULTI PLAYER T+I pair (which shares
    T's top bar) is handled explicitly: T keeps the whole component
    box with the I stem excluded, and the MULTI I sample is discarded
    (clean I samples exist elsewhere).
    """
    out = {}
    for entry, label in BUTTONS:
        width, height, px = frames[entry]
        bg = Counter(px).most_common(1)[0][0]
        y0, y1 = label_band(width, height, px)
        comps = components(width, height, px, y0, y1, bg)
        letters = [c for c in label if c != " "]
        hs = HSCALE[entry]
        boxes = []
        ci = li = 0
        cboxes = [cbox(p) for p in comps]
        while li < len(letters) and ci < len(comps):
            ch = letters[li]
            x0, x1, _, _ = cboxes[ci]
            cw = x1 - x0 + 1
            exp = round(PRI[ch] * hs)
            if entry == 12 and ch == "T" and x0 == 50:
                boxes.append((ch, x0, x1, {"exclude": (61, 64, y0 + 2, y1)}))
                boxes.append(("I", 61, 64, "DISCARD"))
                ci += 1
                li += 2
                continue
            if ci + 1 < len(comps):
                nx0, nx1, _, _ = cboxes[ci + 1]
                if nx0 - x1 - 1 <= 1 and nx1 - x0 + 1 <= exp * 1.35 + 2:
                    j = ci + 1
                    while j + 1 < len(comps):
                        fx0, fx1, _, _ = cboxes[j + 1]
                        if fx0 - cboxes[j][1] - 1 <= 1 and \
                                fx1 - x0 + 1 <= exp * 1.35 + 2:
                            j += 1
                        else:
                            break
                    boxes.append((ch, x0, cboxes[j][1]))
                    ci = j + 1
                    li += 1
                    continue
            if cw > exp * 1.35:
                best = None
                for k in range(2, len(letters) - li + 1):
                    tot = sum(round(PRI[letters[li + t]] * hs)
                              for t in range(k)) + (k - 1)
                    cost = abs(tot - cw)
                    if best is None or cost < best[0]:
                        best = (cost, k)
                if best and best[0] <= 5:
                    k = best[1]
                    expw = [round(PRI[letters[li + t]] * hs)
                            for t in range(k)]
                    cc = col_counts(width, height, px, y0, y1, x0, x1, bg)
                    parts = split_cols(cc, x0, letters[li:li + k], expw)
                    if parts:
                        for t, (a, b) in enumerate(parts):
                            boxes.append((letters[li + t], a, b))
                        ci += 1
                        li += k
                        continue
                raise ValueError(
                    f"entry {entry}: merged component x{x0}..{x1} "
                    f"unresolved (width {cw}, expected {exp})")
            boxes.append((ch, x0, x1))
            ci += 1
            li += 1
        if li != len(letters) or ci != len(comps):
            raise ValueError(
                f"entry {entry} {label}: segmentation leftover "
                f"li={li}/{len(letters)} ci={ci}/{len(comps)}")
        clean = [b for b in boxes
                 if not (len(b) > 3 and b[3] == "DISCARD")]
        for k, (ch, a, b, *extra) in enumerate(clean):
            adv = (clean[k + 1][1] - a) if k + 1 < len(clean) else (b - a + 1)
            rec = {"button": entry, "label": label, "x0": a, "x1": b,
                   "y0": y0, "y1": y1, "advance": adv, "hscale": hs}
            if extra and isinstance(extra[0], dict):
                rec.update(extra[0])
            out.setdefault(ch, []).append(rec)
    return out


def state_mask(state, px, npx=None):
    """Ink mask + values for one state. The disabled frame's glyph 159s
    are indistinguishable from its 159 panel by value, so the normal
    frame disambiguates; values are kept verbatim (the disabled frame
    is a separate rendering, not a remap of normal)."""
    if state == "normal":
        return [px[i] != 209 for i in range(len(px))], px
    if state == "hover":
        return [px[i] not in (139, 140, 141) for i in range(len(px))], px
    mask = [False] * len(px)
    for i in range(len(px)):
        if px[i] != 159:
            mask[i] = True
        elif npx is not None and npx[i] != 209:
            mask[i] = True
    return mask, px


def extract_sample(frames, rec):
    """Per-state indexed bitmaps (rows of palette index or None)."""
    entry = rec["button"]
    x0, x1, y0, y1 = rec["x0"], rec["x1"], rec["y0"], rec["y1"]
    excl = rec.get("exclude")
    out = {}
    for si, state in enumerate(STATES):
        width, height, px = frames[entry + si]
        _, _, npx = frames[entry]
        mask, values = state_mask(state, px, npx)
        rows = []
        for y in range(y0, y1 + 1):
            row = []
            for x in range(x0, x1 + 1):
                if excl and excl[0] <= x <= excl[1] and \
                        excl[2] <= y <= excl[3]:
                    row.append(None)
                    continue
                i = y * width + x
                row.append(values[i] if mask[i] else None)
            rows.append(row)
        out[state] = rows
    return out


def glyph_bottom(rows):
    for y in range(len(rows) - 1, -1, -1):
        if any(c is not None for c in rows[y]):
            return y
    return -1


def trim_rows(rows):
    """Trim leading/trailing all-blank rows (align each sample to its
    own ink bbox vertically; the band y0 is shared per label)."""
    keep = [i for i, r in enumerate(rows) if any(c is not None for c in r)]
    if not keep:
        return rows
    return rows[min(keep):max(keep) + 1]


def resample_rows(rows, tw, th):
    """Resample an indexed bitmap to tw x th, per ink class (LANCZOS,
    threshold 0.5), composing by PRIO_ALL priority."""
    if th == 0:
        return []
    if not rows:
        return [[None] * tw for _ in range(th)]
    sh = len(rows)
    sw = len(rows[0])
    composed = [[None] * tw for _ in range(th)]
    vals = sorted({c for r in rows for c in r if c is not None},
                  key=lambda v: PRIO_ALL.index(v) if v in PRIO_ALL else 99)
    for v in vals:
        img = Image.new("F", (sw, sh))
        pix = img.load()
        for y in range(sh):
            for x in range(sw):
                pix[x, y] = 1.0 if rows[y][x] == v else 0.0
        resized = img.resize((tw, th), Image.LANCZOS)
        rp = resized.load()
        for y in range(th):
            for x in range(tw):
                if composed[y][x] is None and rp[x, y] >= 0.5:
                    composed[y][x] = v
    return composed


# --- Designed glyphs (S, Z): no original exists in the baked labels. ---
# Drawn at cap height 11 with the recovered letters' stroke weight
# (4-5 px stems, 2-row bars) and emboss conventions (159 on the top
# edge row and outer corners, 19 face, 22/37 on curve AA). '#' = 19,
# '+' = 22, '.' = 37, '=' = 159. Hover/disabled are derived below
# from the dominant per-state conventions measured from QUIT.
_CHAR2IDX = {' ': None, '#': 19, '+': 22, '.': 37, '=': 159,
             '6': 6, '8': 58, 'o': 209}
_S_ART = ["   =======   ",
          "  =########= ",
          "  =###=  =##= ",
          "  =###=  =##= ",
          "   =#######= ",
          "    =#####=  ",
          "      =#####=",
          "        =###=",
          "        =###=",
          " =#########= ",
          " ==========="]
_Z_ART = [" =========== ",
          " ###########=",
          "        =###=",
          "       =###= ",
          "      =###=  ",
          "     =###=   ",
          "    =###=    ",
          "   =###=     ",
          "  =###=      ",
          " =##########=",
          " ==========="]


def _designed_bitmap(art):
    rows = [[_CHAR2IDX.get(ch) for ch in row.ljust(13)[:13]] for row in art]
    return rows + [[None] * 13]


def _hover_from_normal(bitmap):
    out = [row[:] for row in bitmap]
    out[0] = [209 if v == 159 else v for v in out[0]]
    out[10] = [159 if v == 19 else v for v in out[10]]
    return out


def _disabled_from_normal(bitmap):
    out = [row[:] for row in bitmap]
    out[10] = [19 if v == 159 else v for v in out[10]]
    return out


def build_font(frames, boxes):
    """Build the font dict from segmented boxes."""
    # Per-label baselines (mode of per-glyph bottom rows, Q excluded):
    # labels were rendered at different vertical scales (cap 10-12
    # rows), so each sample's cap is split at its own label's baseline.
    baselines = {}
    for entry in sorted({s["button"] for ss in boxes.values() for s in ss}):
        bottoms = []
        for ch2, s2 in boxes.items():
            if ch2 == "Q":
                continue
            for r2 in s2:
                if r2["button"] != entry:
                    continue
                bottoms.append(r2["y0"] + glyph_bottom(
                    extract_sample(frames, r2)["normal"]))
        baselines[entry] = Counter(bottoms).most_common(1)[0][0]

    # Per-button ordered letters, for word-boundary-aware advances.
    order = {}
    for entry in sorted({s["button"] for ss in boxes.values() for s in ss}):
        label = [s["label"] for ss in boxes.values() for s in ss
                 if s["button"] == entry][0]
        by_letter = {}
        for ch, ss in boxes.items():
            for r in ss:
                if r["button"] == entry:
                    by_letter.setdefault(ch, []).append(r["x0"])
        for ch in by_letter:
            by_letter[ch].sort()
        if entry == 12:
            by_letter.setdefault("I", []).append(61)
        seq = []
        ptr = {}
        for i, c in enumerate(label):
            if c == " ":
                continue
            x0 = by_letter[c][ptr.get(c, 0)]
            ptr[c] = ptr.get(c, 0) + 1
            width = [r for r in boxes[c] if r["button"] == entry]
            wdt = (width[0]["x1"] - width[0]["x0"] + 1) \
                if width and not (c == "I" and entry == 12) else 4
            seq.append([c, x0, i + 1 >= len(label) or label[i + 1] == " ",
                        wdt])
        order[entry] = seq
    adv = {}
    for entry, seq in order.items():
        hs = [s["hscale"] for ss in boxes.values() for s in ss
              if s["button"] == entry][0]
        for j, (c, x0, wf, width) in enumerate(seq):
            if j + 1 < len(seq):
                a = (seq[j + 1][1] - x0) / hs
                if wf:
                    a -= SPACE
            else:
                a = width / hs
            adv.setdefault(c, []).append((a, j + 1 >= len(seq)))

    target_w = {}
    font_adv = {}
    for ch, samples in boxes.items():
        if ch in QUIT_LETTERS:
            r18 = [s for s in samples if s["button"] == 18][0]
            target_w[ch] = r18["x1"] - r18["x0"] + 1
            seq = order[18]
            for j, (c, x0, wf, width) in enumerate(seq):
                if c == ch:
                    if j + 1 < len(seq):
                        a = seq[j + 1][1] - x0
                        if wf:
                            a -= SPACE
                    else:
                        a = width + 1
                    font_adv[ch] = int(round(a))
                    break
        else:
            ws = [round((s["x1"] - s["x0"] + 1) / s["hscale"])
                  for s in samples]
            target_w[ch] = int(statistics.median(ws))
            a_med = statistics.median([a for a, _ in adv[ch]])
            if all(last for _, last in adv[ch]):
                a_med = target_w[ch] + 1
            elif a_med < target_w[ch] + 1:
                a_med = target_w[ch] + 1
            font_adv[ch] = int(round(a_med))

    font = {}
    for ch, samples in boxes.items():
        tw = target_w[ch]
        glyph = {"advance": font_adv[ch], "width": tw, "bitmap": {}}
        for state in STATES:
            if ch in QUIT_LETTERS:
                rec = [s for s in samples if s["button"] == 18][0]
                rows = extract_sample(frames, rec)[state]
                bi = baselines[18] - rec["y0"]
                cap, desc = rows[:bi + 1], rows[bi + 1:]
                grid = [(g + [None] * (tw - len(g)))[:tw] for g in cap]
                while len(grid) < 11:
                    grid.append([None] * tw)
                grid = grid[:11]
                drow = (desc[0] + [None] * tw)[:tw] if desc else [None] * tw
                glyph["bitmap"][state] = grid + [drow]
            else:
                counts = [[Counter() for _ in range(tw)] for _ in range(12)]
                n = 0
                for rec in samples:
                    rows = extract_sample(frames, rec)[state]
                    bi = baselines[rec["button"]] - rec["y0"]
                    cap = trim_rows(rows[:bi + 1])
                    desc = trim_rows(rows[bi + 1:])
                    full = resample_rows(cap, tw, 11) + \
                        resample_rows(desc, tw, 1)
                    for y in range(12):
                        for x in range(tw):
                            counts[y][x][full[y][x]] += 1
                    n += 1
                grid = []
                for y in range(12):
                    row = []
                    for x in range(tw):
                        c = counts[y][x]
                        ink = {k: v for k, v in c.items() if k is not None}
                        if ink and sum(ink.values()) * 2 > n:
                            best = max(
                                ink.items(),
                                key=lambda kv: (kv[1],
                                                -PRIO_ALL.index(kv[0])
                                                if kv[0] in PRIO_ALL
                                                else -99))[0]
                            row.append(best)
                        else:
                            row.append(None)
                    grid.append(row)
                glyph["bitmap"][state] = grid
        font[ch] = glyph

    for ch, art in (("S", _S_ART), ("Z", _Z_ART)):
        normal = _designed_bitmap(art)
        font[ch] = {"advance": 14, "width": 13, "designed": True,
                    "bitmap": {"normal": normal,
                               "hover": _hover_from_normal(normal),
                               "disabled": _disabled_from_normal(normal)}}
    return font


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Rebuild the Moo2Menu bitmap font from MAINMENU.LBX.")
    parser.add_argument("lbx", type=Path, help="path to MAINMENU.LBX")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT,
                        help="where to write Moo2Menu.json")
    args = parser.parse_args(argv)
    data, offsets = load_lbx(args.lbx)
    frames = {e: decode_sprite(data, offsets, e) for e in range(3, 43)}
    boxes = segment_labels(frames)
    font = build_font(frames, boxes)
    payload = {
        "name": "Moo2Menu",
        "cap_height": 11,
        "descender_rows": 1,
        "space_advance": SPACE,
        "provenance":
            "Recreated from the baked MAINMENU.LBX button labels of the "
            "1990 game Master of Orion 2. Q,U,I,T,G,A,M,E are byte-exact "
            "replicas of the QUIT GAME frames (entries 18/19/20). Other "
            "letters are majority-consensus resamples of the baked labels "
            "at QUIT scale. S and Z are designed to match (no original S "
            "or Z exists in the baked labels); their hover/disabled "
            "states follow the dominant per-state conventions (hover: "
            "top edge 209, bottom edge 159; disabled: bottom edge "
            "159->19).",
        "glyphs": font,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=1) + "\n",
                           encoding="utf-8")
    verbatim = sorted(c for c in font if c in QUIT_LETTERS)
    designed = sorted(c for c, g in font.items() if g.get("designed"))
    consensus = sorted(set(font) - set(verbatim) - set(designed))
    print(f"{args.output}: {len(font)} glyphs")
    print(f"  byte-exact (QUIT GAME): {' '.join(verbatim)}")
    print(f"  consensus resamples: {' '.join(consensus)}")
    print(f"  designed to match: {' '.join(designed)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
