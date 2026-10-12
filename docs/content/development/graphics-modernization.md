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

Native builds automatically enable Vulkan presentation when a Vulkan 1.4 SDK and `glslangValidator` are available. The build translates the canonical HQx and MMPX integer kernels into GLSL, compiles SPIR-V for Vulkan 1.4, and embeds the shader in the executable. Runtime requires a Vulkan 1.4 loader and hardware device. Unsupported systems fall back to the existing SDL presentation path.

Linux prefers Wayland, with X11 fallback when Wayland is unavailable. The default window is 1920×1080, with the original 4:3 logical frame letterboxed inside it. `SDL_VIDEODRIVER=x11` overrides the desktop backend; `ORION2RE_WINDOW_SIZE=1280x960` overrides the window size without changing gameplay coordinates.

Windowed mode requests the desktop's normal frame, title bar, and window controls. GNOME Wayland requires SDL's libdecor support: install the `libdecor-devel` build package and retain the libdecor runtime and decoration plugin. The native Linux build uses dynamically loaded libdecor; it does not draw a replacement title bar inside the game.

The Vulkan backend uploads the logical ARGB frame and runs compute shaders for the selected scaler. MMPX4 uses two 2x passes. Pixelart and bilinear preserve their 1x logical texture and use SDL's GPU presentation sampler for display scaling. All seven modes bypass CPU scaling on this backend. Palette conversion and software cursor composition remain on the CPU; window presentation, input mapping, and the debug overlay use SDL's GPU renderer.

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
captions using ASCII game race names and “Custom”. Screen titles, trait text,
and other screens still use bitmap fonts. Non-ASCII labels fall back individually.

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

Validation: `test_ttf_labels` checks frame eligibility, malformed font table
bounds, ASCII fallback, cached rendering at 1×/3×, highlighted text, and missing
fonts. `test_fields` covers normal and pressed radio-button capture. Set
`ORION2RE_TTF_TEST_PREVIEW` to a BMP path when running `test_ttf_labels` to export
the rendering fixture. A real race-selection capture is kept locally at
`out/ttf-race-selection.png`.
