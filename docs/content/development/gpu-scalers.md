# Vulkan GPU scaling

The optional Vulkan backend moves presentation scaling, indexed-palette conversion,
and software-cursor composition onto the GPU. The game continues to draw its
original indexed framebuffer, bitmap text, and artwork. Gameplay, logical screen
coordinates, saves, and the configured scaler remain unchanged.

## Build and select the backend

Native game builds detect the Vulkan SDK and `glslangValidator`. Python generates
GLSL from the existing HQx and MMPX kernels; CMake compiles and embeds SPIR-V 1.0
shaders targeting Vulkan 1.0. No shader compiler is needed at runtime. The game
uses SDL's GPU API and dynamically loaded Vulkan backend; it does not link directly
to the Vulkan loader. The standalone timestamp profiler does link to Vulkan.

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target orion2re vulkan-scaler-check vulkan-scaler-profile
```

If the SDK or shader compiler is unavailable, the build retains the existing SDL
presentation path. Cross-compilation also retains that path. Configure with
`-DORION2RE_ENABLE_VULKAN=OFF` to explicitly build without the new backend.

Run from an installation containing legitimate original game data:

```sh
cd /path/to/game
ORION2RE_RENDERER=vulkan ./orion2re
```

With no renderer override, the game attempts Vulkan and falls back to SDL if
initialization fails. `ORION2RE_RENDERER=vulkan` requires Vulkan and reports startup
failure if unavailable; `ORION2RE_RENDERER=sdl` selects the existing backend.
`ORION2RE_VULKAN_VALIDATION=1` requests Vulkan validation for the SDL GPU device.
Software Vulkan devices are excluded unless `ORION2RE_VULKAN_ALLOW_SOFTWARE` is set.

The existing `interface scaler` setting selects pixelart, bilinear, HQ2x, HQ3x,
HQ4x, MMPX2, or MMPX4. GPU HQx and MMPX output preserves the CPU algorithms;
MMPX4 uses two 2× passes. Pixelart and bilinear present the palette-converted
frame directly, using nearest and linear sampling respectively.

## Indexed uploads and synchronization

The game uploads the indexed frame, its 256-color palette, and a 24×24 cursor.
A compute shader converts them into RGBA before scaling. Palette colors are
opaque as in the original SDL path; cursor index zero remains transparent.
Clipped and fully offscreen cursors retain the existing composition behavior.
For a 640×480 frame, this uploads 308,800 bytes rather than 1,228,800 bytes of ARGB.

Transfer buffers and textures cycle to preserve in-flight frames. There are no
per-frame GPU idle waits or readbacks in the game loop; full idle waits are
confined to scaler reconfiguration and shutdown. SDL GPU textures are shared
with its renderer for presentation, including the existing debug overlay.
The renderer refreshes its backbuffer before drawing after a window resize.

## Validate scaler parity and presentation

```sh
out/build/Linux/linux-debug/vulkan-scaler-check
ORION2RE_VULKAN_VALIDATION=1 out/build/Linux/linux-debug/vulkan-scaler-check --present
```

The parity tool compares all seven modes against the CPU implementation on
source-owned patterns, tiny and odd dimensions, padded strides, all palette
entries including alpha, palette-only updates, and clipped/offscreen cursors.
It also checks invalid inputs. An optional local BMP can be compared and exported:

```sh
out/build/Linux/linux-debug/vulkan-scaler-check /path/to/frame.bmp /tmp/scaler-results
```

The presentation fixture displays 32 frames per scaler with changing palettes,
moving cursors, and window resizes. It checks the final compute output and a
pixel sampled by the renderer against the CPU result. Its frame loop has no
explicit GPU idle waits or readbacks; synchronization is confined to the final
checks and scaler reconfiguration. Run it on a real display for hardware testing.

Validate generated binaries when SPIR-V Tools is installed:

```sh
spirv-val --target-env vulkan1.0 out/build/Linux/linux-debug/generated/vulkan/scaler.spv
spirv-val --target-env vulkan1.0 out/build/Linux/linux-debug/generated/vulkan/palette.spv
```

## Profile GPU work

SDL's public GPU API does not expose timestamp queries. `vulkan-scaler-profile`
uses a minimal Vulkan 1.0 device and the same embedded shaders to compare ARGB
upload with indexed upload and GPU palette conversion, followed by MMPX2.
Both paths must match the CPU scaler before results are reported.

```sh
out/build/Linux/linux-debug/vulkan-scaler-profile > /tmp/gpu-profile.csv
out/build/Linux/linux-debug/vulkan-scaler-profile /path/to/frame.bmp
```

Without a BMP, it generates a source-owned 640×480 pattern. Input BMPs must fit
within 640×480 and contain at most 256 distinct colors. Set
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` for standalone validation.
Missing timestamp support fails the tool without affecting the game.

CSV results average 30 iterations after five warmups. `host_pack_ms` measures CPU
staging preparation; GPU timestamps separate upload, palette conversion, scaling,
and readback, including intervening barriers. `submit_wait_ms` includes CPU
submission and the completion fence wait. The ARGB path's palette column records
timestamp overhead. This fixture uses one queue, synchronous readback, and GENERAL
image layouts. It does not reproduce SDL resource cycling, queued-frame overlap,
or window presentation, and does not measure live game frame latency.

Parity and presentation have been validated locally on Intel Graphics LNL.
Shaders also passed Vulkan 1.0 validation. Older physical GPUs and other platforms
still require testing; the shader target alone does not establish compatibility
with every Vulkan 1.0 device. Keep proprietary captures outside tracked source
and distributable packages. This implementation was developed with substantive
AI assistance; generated kernels derive from the project's existing scaler code.
