# orion2re

This project is a restoration effort for the Master of Orion II game engine. Check our [documentation portal](https://moo2mod.com/redone/doc/) too.

All rights to Master of Orion II, its assets, trademarks, and related intellectual property belong to their corresponding owners.

This repository does not include the original game data. To play, you must own a legitimate copy of Master of Orion II and provide the original game files from a supported release such as GOG or Steam.

## License

Original orion2re code and documentation are free software licensed under
the GNU General Public License, version 3 or any later version. See
[`LICENSE`](LICENSE) for the full license text.

This license does not apply to Master of Orion II, its assets, trademarks,
or other intellectual property. Third-party components retain their own
licenses and copyright notices in their source files and vendor directories.

## AI-assisted development

This project was built with assistance from AI agents operating under human direction. Human contributors wrote code, reviewed, tested, and approved AI-assisted changes, and remain responsible for the final work.

## Requirements

- CMake 3.20 or newer
- Ninja
- Either:
  - macOS build environment with an arm64-capable Clang toolchain
  - Windows build environment with an MSVC toolchain
  - Linux build environment with optional MinGW-w64 cross-compilers
- Original Master of Orion II game files from GOG or Steam

Supported build paths:

- Native macOS arm64 builds
- Native Linux builds
- Native Windows builds with MSVC for x86 and x64
- Linux-hosted MinGW-w64 cross-builds for Win32 and Win64

macOS builds use the native host architecture.

## Vendored submodules

The project uses vendored submodules for third-party dependencies.

Clone the repository with submodules:

```sh
git clone --recurse-submodules <repo-url>
```

If you already cloned the repository without submodules, initialize and update them with:

```sh
git submodule update --init --recursive
```

## Build

Configure and build with a preset:

```sh
cmake --preset win32-debug
cmake --build --preset win32-debug
```

Native macOS example:

```sh
cmake --preset osx-debug
cmake --build --preset osx-debug
```

Router-only Linux example:

```sh
cmake --preset linux-router
cmake --build --preset linux-router
```

This skips the `orion2re` game target and is the preferred path when you only need `orion2re-router`, especially on a headless Linux machine. Prefer `-DORION2RE_BUILD_GAME=OFF` for router-only work.

Tests are opt-in and are not built by default. To configure, build, and run the current unit tests:

```sh
tools/run-tests.sh
```

For the full preset list and per-platform details, see `CMakePresets.json`.

Native builds with a Vulkan 1.4 SDK and `glslangValidator` use GPU shaders for
presentation scaling on compatible hardware, with automatic SDL fallback.
Linux defaults to Wayland and a 1920×1080 window, letterboxing the original
4:3 game view. X11 remains available as a fallback.
For window borders and title-bar controls on GNOME Wayland, install
`libdecor-devel` before configuring SDL.
Set `ORION2RE_RENDERER=vulkan` to require Vulkan or `ORION2RE_RENDERER=sdl` to
select the previous backend. See the [graphics development guide](docs/content/development/graphics-modernization.md)
for captures, comparisons, validation, and build options.

Supported source-owned mods are stored under `data/mods/`. Normal game builds package `data/base/` as `data/base.zip`, package each source-owned mod as `data/mods/<package>.zip`, and deploy all archives when `ORION2RE_GAME_DIR` is configured.

Build only the distributable mod ZIP archives with:

```sh
cmake --build --preset win32-debug --target package-supported-mods
```

Archives are written under the build tree at `data/mods/<package>.zip`. To deploy only the mods to a configured target game directory, use:

```sh
cmake --build --preset win32-debug --target deploy-supported-mods
```

The game does not enable deployed mods automatically. Add `enable <package>;` to
`data/orion2re.cfg`, for example `enable v150;` for standard 1.50, `enable v150i;` for the
single-player 1.50 Improved profile, or `enable v150i-mp;` for its multiplayer overrides.

Random-sequence mismatches log every recorded original caller and captured local caller, including absolute addresses and executable-relative RVAs for symbol lookup.

Example release build:

```sh
cmake --preset win32-release
cmake --build --preset win32-release
```

## Cross-compiling Windows builds from Linux

Install MinGW-w64 cross-compilers and Ninja first. On Debian or Ubuntu, the required packages are typically:

```sh
sudo apt install gcc-mingw-w64 g++-mingw-w64 binutils-mingw-w64 ninja-build
```

Then configure and build one of the MinGW presets:

```sh
cmake --preset mingw-win32-debug
cmake --build --preset mingw-win32-debug
```

or:

```sh
cmake --preset mingw-win64-release
cmake --build --preset mingw-win64-release
```

See `CMakePresets.json` for the available MinGW presets.
