# Graphics modernization

The first milestone establishes a reproducible visual baseline before changing artwork or interface rendering. Compare the existing presentation scalers on identical frames, then evaluate optional high-resolution artwork separately from text and gameplay.

## Capture logical frames

Build the native game and comparison tool:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target orion2re scaler-compare
```

Run from an installation containing original game data. Set an absolute capture directory and press F12 on a settled screen:

```sh
cd /path/to/game
ORION2RE_CAPTURE_DIR=/absolute/path/to/captures ./orion2re
```

The opt-in shortcut saves lossless BMPs of the logical frame before presentation scaling, including the software cursor. Without the environment variable, F12 retains its normal behavior. Rename captures descriptively, for example `galaxy.bmp` or `research.bmp`. Use a new directory for each session because numbering restarts when the process restarts.

Shift+F12 arms a sequence of the next 120 published frames. Press it again to
cancel. Cursor-only refreshes and window redraws do not consume sequence frames;
palette-only publications do, so fades are included. A failed save stops the
sequence. Capture an idle diplomacy audience and a tactical movement or firing
sequence into separate directories. Disk writes perturb playback timing: use
these frames to assess visual consistency, and collect timings in a separate
run with capture disabled. BMPs contain the original logical artwork, so optional
portrait and TrueType overlays are excluded.

## Measure the presentation path

Set an absolute CSV path whose parent directory already exists:

```sh
ORION2RE_RENDERER=vulkan \
ORION2RE_FRAME_TIMINGS=/absolute/path/to/diplomacy-vulkan.csv ./orion2re
```

The presenter retains up to 30,000 samples in memory and writes the CSV on normal
shutdown. Repeat the same scene, window size, scaler, and vsync setting with
`ORION2RE_RENDERER=sdl`. Avoid captures, debugger pauses, and debug overlays during
measurement. Close the game normally before summarizing:

```sh
python3 tools/frame_timings.py /absolute/path/to/diplomacy-vulkan.csv
```

The summary excludes the first two seconds, sequence-capture samples, and the
interval immediately following a captured sample. It groups results by renderer
and scaler, reporting mean, median, p95, p99, and maximum. `present_ms` includes
the published-frame copy, palette conversion, cursor composition, scaling,
texture upload, optional overlays, and `SDL_RenderPresent`. `interval_ms` measures
time between successful presentation returns, including event processing,
frame pacing, and time waiting for new game frames. `published` distinguishes
game publications from presentation-only refreshes. Scaler changes start a new
interval group. Idle gaps are retained; do not interpret their reciprocal as FPS.

These are CPU wall-clock measurements of the complete presentation path. They
do not measure gameplay computation before publication, asynchronous GPU
completion, input-to-display latency, or scanout. Hardware Vulkan measurements
require a real display backend; Xvfb cannot provide its DRI3 presentation path.
The statistics helper has regression checks in `test_frame_timings` when Python
is available to CTest.

## Generate comparisons

Run from the repository root. Python requires Pillow; the C++ tool uses the production scaler pipeline.

```sh
python3 tools/visual_gallery.py /absolute/path/to/captures \
    out/visual-milestone/gallery \
    --scaler-tool out/build/Linux/linux-debug/scaler-compare
```

Open `out/visual-milestone/gallery/index.html` locally. Compare pixelart, bilinear, HQ2x/3x/4x, and MMPX2/4. Select native output to inspect actual scaler pixels, or 1080p/1440p for previews of a 4:3 viewport. Preview resampling approximates display presentation; it does not reproduce SDL's pixel-art sampler exactly.

Each screen includes CSV timings. Full-frame measurements invalidate the entire frame, while idle measurements exercise unchanged-frame detection. Results average 30 iterations after five warmups. GPU upload, display sampling, gameplay, and frame pacing are excluded.

## Vulkan presentation

Native builds automatically enable Vulkan presentation when a Vulkan SDK and `glslangValidator` are available. The build translates the canonical HQx and MMPX integer kernels into GLSL, targets Vulkan 1.0 / SPIR-V 1.0, and embeds the shaders in the executable. Device creation uses SDL's minimal Vulkan feature set; no additional device extensions are required by the scalers. Unsupported systems fall back to the existing SDL presentation path.

Linux prefers Wayland, with X11 fallback when Wayland is unavailable. The default window is 1920×1080, with the original 4:3 logical frame letterboxed inside it. `SDL_VIDEODRIVER=x11` overrides the desktop backend; `ORION2RE_WINDOW_SIZE=1280x960` overrides the window size without changing gameplay coordinates.

Windowed mode requests the desktop's normal frame, title bar, and window controls. GNOME Wayland requires SDL's libdecor support: install the `libdecor-devel` build package and retain the libdecor runtime and decoration plugin. The native Linux build uses dynamically loaded libdecor; it does not draw a replacement title bar inside the game.

The Vulkan backend uploads the logical indexed frame, a 256-color palette, and a color-keyed 24×24 cursor. A compute shader converts the palette and composes the cursor before scaling. MMPX4 uses two 2x scaler passes. Pixelart and bilinear use the converted 1x texture directly, avoiding an extra scaler pass; SDL's GPU presentation sampler handles display scaling. All seven modes bypass CPU palette conversion and scaling on this backend. Logical-frame captures convert on the CPU only when requested. The SDL fallback retains its CPU conversion and scaler pipeline. Window presentation, input mapping, and the debug overlay use SDL's GPU renderer.

```sh
cmake --build --preset linux-debug --target orion2re vulkan-scaler-check
cd /path/to/game
ORION2RE_RENDERER=vulkan ./orion2re
```

The default is automatic selection. `ORION2RE_RENDERER=vulkan` requires Vulkan and reports a startup failure when unavailable; `ORION2RE_RENDERER=sdl` selects the previous backend. Set `ORION2RE_VULKAN_VALIDATION=1` for GPU validation. `ORION2RE_VULKAN_ALLOW_SOFTWARE=1` permits software Vulkan for isolated display tests. Configure `-DORION2RE_ENABLE_VULKAN=OFF` to omit the Vulkan backend entirely.

Validation enables the Khronos validation layer. SDL's additional GPU bounds assertions are disabled because they reject some scissor rectangles that are valid in Vulkan during window and overlay resizing. Vulkan's scissor rules require nonnegative offsets and no integer overflow, rather than containment within the render target; see the [Vulkan scissor specification](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetScissor.html).

Run `out/build/Linux/linux-debug/vulkan-scaler-check` for GPU/CPU parity checks covering small and odd dimensions, padded strides, transparency, and invalid inputs. It can also validate an actual capture:

```sh
out/build/Linux/linux-debug/vulkan-scaler-check captures/research.bmp out/gpu/research
```

Add `--gpu-tool out/build/Linux/linux-debug/vulkan-scaler-check` to the gallery command to include validated shader outputs and timings. GPU timings include upload, submission, execution, and a completion wait, averaged over 10 iterations after five warmups. These differ from the CPU timing scope and exclude window presentation.

The parity tool additionally checks indexed uploads, all 256 palette entries
(including alpha), palette-only updates, cursor color key zero, and clipped
cursors against CPU-composed frames for all seven modes. The game uses opaque
palette colors, matching its existing SDL conversion. Transfer buffers and
textures are cycled rather than waiting for GPU idle every frame; full idle
waits are confined to reconfiguration and shutdown.

On a real display, run the source-owned presentation fixture:

```sh
ORION2RE_VULKAN_VALIDATION=1 \
    out/build/Linux/linux-debug/vulkan-scaler-check --present
```

It presents 32 frames per scaler with changing palettes and moving, clipped
cursors, resizes the window, and checks both final compute output and a pixel
sampled by the renderer against the CPU result. The frame loop has no explicit
GPU idle waits or readbacks; synchronization is confined to final checks and
scaler reconfiguration.
This checks SDL texture interoperation and resource cycling as well as shader
fidelity. The local Intel hardware run passed all seven modes with validation
enabled. The expanded trait text also passed a separate isolated software
Vulkan game run, popup fallback, logical-frame capture, and window resize review.

## GPU timestamp profiling

SDL's public GPU API does not expose timestamp queries or native command-buffer
handles. The separate `vulkan-scaler-profile` tool therefore uses a minimal
Vulkan 1.0 device and the same embedded scaler and palette shaders. It compares
MMPX2 with full-color upload against indexed upload followed by GPU palette
conversion. It verifies both outputs against the CPU scaler before reporting
results; captures must contain no more than 256 distinct colors.

```sh
cmake --build --preset linux-debug --target vulkan-scaler-profile
out/build/Linux/linux-debug/vulkan-scaler-profile > out/gpu-profile.csv
out/build/Linux/linux-debug/vulkan-scaler-profile captures/diplomacy.bmp
```

Without a BMP argument it generates a source-owned 640×480 test pattern.
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` enables Vulkan validation for
this standalone tool. Software devices are excluded unless
`ORION2RE_VULKAN_ALLOW_SOFTWARE` is set. Missing timestamp support fails the tool
with a diagnostic and does not affect the game.

The CSV averages 30 iterations after five warmups. `host_pack_ms` measures CPU
staging-buffer preparation. GPU timestamps separate upload, palette conversion,
MMPX2 scaling, and readback, including the barriers between those stages.
`submit_wait_ms` measures CPU submission and fence wait for the entire workload.
The ARGB path has no palette work, so that column records timestamp overhead.
The tool uses one queue, synchronous readback, and GENERAL image layouts;
it does not reproduce SDL's resource cycling, layout policy, queued-frame
overlap, or window presentation. Compare its two paths to investigate costs;
do not treat it as a measurement of live game frame latency.

On the local Intel Graphics LNL validation run, the indexed path uploaded
308,800 bytes versus 1,228,800 bytes for ARGB. CPU buffer packing averaged
approximately 0.01 ms versus 0.04 ms on both diplomacy and scout-combat captures.
The additional GPU palette pass averaged approximately 0.03–0.04 ms. These are
machine-specific samples with validation enabled, not a general frame-rate
improvement claim. Palette conversion and cursor composition now execute on the
GPU in the game, while dense text continues to be reviewed separately.

Both shaders validate with `spirv-val --target-env vulkan1.0`. The compatibility
checks used modern Intel and software Vulkan drivers with a Vulkan 1.0 instance
and minimal requested features; older physical GPUs still require testing.

## Initial baseline

The Linux baseline captured the menu, race selection portrait, galaxy map, research chooser, and colony screen. Full-frame MMPX2 processing measured approximately 3–7 ms across those captures; HQ2x measured 9–12 ms, and HQ4x measured 24–27 ms. These measurements are specific to this machine and workload.

MMPX2 is a candidate for a balanced optional presentation profile. Retain the existing default until animation, real frame pacing, and text readability are reviewed. Portrait smoothing alone does not establish a better default for dense interfaces.

The Vulkan baseline on Intel Graphics LNL matched CPU output exactly for all seven modes on all five captured screens. MMPX2 submission and completion measured approximately 0.5–0.6 ms, and HQ4x approximately 1.7–2.3 ms, including full-frame upload and a GPU idle wait. Native Wayland presentation and isolated window resize tests passed with validation enabled. These results establish shader fidelity and initial performance, rather than an end-to-end frame-rate claim.

## Remaining milestones

1. Extend the captured diplomacy and scout-movement baseline with weapon effects,
   missiles, explosions, and battles with multiple ships. Collect controlled
   diplomacy/combat presentation timings on real display hardware and review text
   and small icons at actual display size before changing the default scaler.
2. Compare offline neural enhancement and restrained image-generation studies on representative artwork. Approve identity, silhouettes, colors, and animation consistency before integrating assets.
3. Add high-resolution texture replacements keyed by LBX entry and frame, preserving logical dimensions, clipping, anchors, palette effects, input coordinates, and fallback assets. Start with one diplomacy screen.
4. Render text and selected interface elements at display resolution; verify wrapping, localization, UI scaling, and hitboxes.
5. Expand the optional visual pack, then adapt widescreen layouts screen by screen.

Keep original data, captures, derived artwork, and experimental visual packs outside tracked source and distributable build packages. Local gallery output under `out/` is ignored by Git. This workflow and the initial portrait concept were developed with AI assistance; generated artwork requires human review and provenance assessment before distribution.

## Extended baseline validation

The October 2026 validation added race relations, a human diplomacy audience,
and tactical scout combat to the local comparison gallery. Diplomacy and scout
movement each have 120-frame sequences (31 and 50 distinct pixel frames,
respectively). They came from an isolated copy of a local early-game save:
diplomatic contacts were enabled and an opposing scout placed in the player's
orbit with a war treaty to make the scenes available. These are presentation
fixtures, not tests of naturally reached diplomatic or AI behavior. The original
installation and saves were retained unchanged.

All seven Vulkan scaler modes matched CPU output exactly on the new diplomacy
and tactical captures, with Khronos validation enabled. Hardware X11 Vulkan and
SDL/OpenGL presentation runs at 1920×1080 both saved timing logs and exited
normally. Those short intro runs validate instrumentation and the backends;
they are not controlled diplomacy/combat performance comparisons. Captured runs
with debugger pauses are likewise unsuitable for performance conclusions.
The Vulkan-disabled game build and all 103 registered tests passed on Linux.

The local still gallery is `out/visual-milestone/extended-gallery/index.html`.
Animation galleries are `out/visual-milestone/diplomacy-animation/index.html`
and `out/visual-milestone/combat-animation/index.html`. To generate an animation
gallery from its numbered BMP directory:

```sh
python3 tools/visual_gallery.py /absolute/path/to/sequence \
    out/visual-milestone/animation-review --sequence \
    --scaler-tool out/build/Linux/linux-debug/scaler-compare
```

Sequence mode retains native scaler PNGs, enables paired frame playback, and
uses one CPU pass without warmups. Its timing columns are diagnostic only.
Playback rate is selectable and does not represent the original frame cadence.

## Optional high-resolution race portraits

Set `ORION2RE_GRAPHICS_DIR` to a local portrait pack directory to enable static,
high-resolution artwork on the stock race selection screen:

```sh
ORION2RE_GRAPHICS_DIR=/absolute/path/to/out/alien-races-v1 ./orion2re
```

Leave the variable unset or empty to use the original graphics. The directory
contains `alkari.png`, `bulrathi.png`, `darlok.png`, `elerian.png`, `gnolam.png`,
`human.png`, `klackon.png`, `meklar.png`, `mrrshan.png`, `psilon.png`, `sakkra.png`,
`silicoid.png`, and `trilarian.png`. PNGs must be at least 290×322 and no more
than 4096 pixels per side; use the original 290:322 aspect ratio. Textures are
loaded once per race and rendered at display resolution within the original
portrait bounds. Restart after changing the pack.

Missing or invalid portraits fall back individually to the original artwork.
Custom races, dialogs that cover the portrait, fades, and other game screens
retain the original graphics. The UI frame and cursor remain visible above the
replacement. This option works with both SDL and Vulkan presentation.
Keep locally generated restorations under ignored `out/`; do not commit
proprietary source art or derived portrait packs. Implementation and the local
restorations were created with substantive AI assistance.

## Optional TrueType label backend

Set `ORION2RE_TTF_FONT` to an absolute path to a trusted local `.ttf` file:

```sh
ORION2RE_TTF_FONT=/absolute/path/to/vendors/imgui/misc/fonts/Roboto-Medium.ttf \
ORION2RE_GRAPHICS_DIR=/absolute/path/to/out/alien-races-v1 ./orion2re
```

The flags are independent. Unset or empty `ORION2RE_TTF_FONT` keeps the original
bitmap captions. A missing or unsupported font falls back without preventing
play; restart to change fonts. The first integration replaces the 14 race button
captions using ASCII game race names and “Custom”, plus the wrapped race-trait
descriptions. Screen titles and other screens still use bitmap fonts. Non-ASCII
labels and trait lines fall back individually.

The backend uses the `stb_truetype` implementation already pinned in the Dear
ImGui submodule (`vendors/imgui/imstb_truetype.h`, upstream dual MIT/public-domain
license), with no new package dependency. It accepts standalone TrueType files
with glyph outlines, not font collections or CFF OpenType files. Table bounds
are checked before loading, but this rasterizer is intended for trusted font
files. No font or proprietary artwork is copied into runtime packages.

Game-thread capture records button interiors and text; publication validates
that the captured pixels and palette still match the finished frame. Palette-only
fades and modified regions use bitmap captions. The presentation thread owns the
font, glyph rasterization, and texture cache. It rasterizes at the display scale,
centers/fits names inside the original button bounds, and preserves hitboxes and
button borders. Because the original captions are baked into sprites, the trial
reconstructs the caption background from clear side strips. This is a prototype
of the label appearance, not a replacement for general text layout: original
bitmap measurement, wrapping, and gameplay behavior remain in place.

Trait descriptions reuse the original paragraph wrapper. Each rendered line
captures its real background before bitmap drawing, then records the finished
region for publication checks. The native-resolution overlay restores that
background and draws the same line at its original location, size, and palette
color. It supports up to eight ASCII lines within the existing bounds. Covered
regions, palette fades, missing glyphs, and invalid geometry retain bitmap text.
There is no change to paragraph wrapping, localization encodings, or hitboxes.

Validation: `test_ttf_labels` checks frame eligibility, malformed font table
bounds, ASCII fallback, cached rendering at 1×/3×, highlighted text, and missing
fonts. `test_fields` covers normal and pressed radio-button capture. Set
`ORION2RE_TTF_TEST_PREVIEW` to a BMP path when running `test_ttf_labels` to export
the rendering fixture. A real race-selection capture is kept locally at
`out/ttf-race-selection.png`.
