# orion2re

This project is a restoration effort for the Master of Orion II game engine. Check our [documentation portal](https://moo2mod.com/redone/doc/) too.

The game is fully playable and includes all bug fixes and optional community improvements up to the latest version of the unofficial 1.50 patch.

All rights to Master of Orion II, its assets, trademarks, and related intellectual property belong to their corresponding owners.

This repository does not include the original game data. To play, you must own a legitimate copy of Master of Orion II and provide the original game files from a supported release such as GOG or Steam.

## Download and play

To play, download a prebuilt ZIP. You do not need CMake, Ninja, or a compiler.

1. Install your copy of Master of Orion II from GOG or Steam.
2. Open [GitHub Releases](https://github.com/orion2re/orion2re/releases) and expand
   Assets for the release you want. Download the game ZIP for your platform:
   `orion2re-win-win32-<tag>.zip` (Windows 32-bit),
   `orion2re-win-win64-<tag>.zip` (Windows 64-bit), or
   `orion2re-osx-arm64-<tag>.zip` (macOS on Apple Silicon).
   The Source code (zip) and Source code (tar.gz) downloads are for building the
   game yourself, not for installing it.
3. If you're updating, back up `data/orion2re.cfg` first. Release ZIPs include
   the default configuration, which will replace your custom settings.
4. Extract the ZIP's contents into your existing Master of Orion II game folder,
   alongside the original game files, merging the included `data/` folder.
   Packages include runtime data but no commercial game files.
5. Launch `orion2re.exe` on Windows or `Orion2 Redone.app` on macOS
   from that folder, rather than the original game's launcher.

On macOS, use Archive Utility or `ditto` to extract the ZIP, and keep the app,
executable, and `data/` folder together so the app can find the game. The app is
not signed or notarized by Apple, so you may need to allow it in
System Settings / Privacy & Security.

Linux is supported, but we don't currently provide prebuilt Linux packages.
To run the game natively on Linux, you'll need to [build it from source](#build).
The ZIP instructions above are for Windows and macOS.

### Development builds

To try a development build, select a successful `main` run under
[Actions / CI and packages](https://github.com/orion2re/orion2re/actions/workflows/ci.yml)
and download the artifact for your platform. GitHub requires you to sign in to
download Actions artifacts, even from public repositories. Release downloads
don't require sign-in. Development downloads are kept for 30 days.

The artifact is a ready-to-use ZIP, such as `orion2re-win-win32-main.zip`,
with the executable and `data/` folder directly inside it.
The workflow run shows which commit it was built from.

Install these ZIPs in the same way as releases. Development (`main`) packages omit
`data/orion2re.cfg`, so your configuration survives intermediate updates.

On all platforms, the game uses the executable's directory for data, configuration,
and save files, regardless of the launcher's working directory. Linux desktop
launchers do not need a `Path=` entry pointing to the installation directory.

## License

Original orion2re code and documentation are free software licensed under
the GNU General Public License, version 3 or any later version. See
[`LICENSE`](LICENSE) for the full license text.

This license does not apply to Master of Orion II, its assets, trademarks,
or other intellectual property. Third-party components retain their own
licenses and copyright notices in their source files and vendor directories.

## AI-assisted development

This project was built with assistance from AI agents operating under human direction. Human contributors wrote code, reviewed, tested, and approved AI-assisted changes, and remain responsible for the final work.

## Build

These steps build the game from source. The examples use native Linux, with
Windows and macOS alternatives below. To play, you'll also need the original
Master of Orion II game files from your GOG or Steam installation.

### 1. Install build dependencies

You need Git, CMake 3.21 or newer for the presets used below, Ninja, and a C/C++
compiler with C++20 support. These are separate tools, not included in the game
download. Python 3 is also used for the build-tool and packaging regression tests.

#### Linux (Ubuntu, Xubuntu, or Debian)

Open a terminal and install the tools and development libraries used by our Linux CI:

```sh
sudo apt update
sudo apt install git build-essential cmake ninja-build pkg-config python3 \
  libasound2-dev libpulse-dev libaudio-dev libjack-dev libsndio-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libwayland-dev libdecor-0-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
  libudev-dev libdbus-1-dev libibus-1.0-dev libpipewire-0.3-dev
```

`build-essential` includes GCC and G++. Check `cmake --version` before continuing;
if your distribution provides an older version, install a newer one from
[CMake](https://cmake.org/download/).

#### macOS (Apple Silicon)

Install Apple's command-line tools for the Clang compiler and macOS SDK:

```sh
xcode-select --install
```

Then, with [Homebrew](https://brew.sh/) installed, install the remaining tools:

```sh
brew install git cmake ninja python
```

#### Windows (Visual Studio)

Install Visual Studio 2022 or its Build Tools with the Desktop development with
C++ workload, including MSVC, a Windows SDK, and C++ CMake tools for Windows
(CMake and Ninja). Also install [Git for Windows](https://git-scm.com/downloads/win)
and [Python 3](https://www.python.org/downloads/windows/) and add them to `PATH`.
For the sanitizer tests below, include the MSVC AddressSanitizer component.

Run the following steps in an x64 Native Tools Command Prompt for VS 2022 when
using `win64-*` presets. For `win32-*`, use an x86 Native Tools Command Prompt
instead. The selected prompt sets up the compiler for the matching architecture.

### 2. Clone the source and vendored submodules

The project uses Git submodules for third-party libraries such as SDL. Clone
them along with the project:

```sh
git clone --recurse-submodules https://github.com/orion2re/orion2re.git
cd orion2re
```

If you already cloned the repository, open a terminal in its directory and run:

```sh
git submodule update --init --recursive
```

Run all remaining commands from this `orion2re` directory, which contains
`CMakeLists.txt` and `CMakePresets.json`. GitHub's source-code ZIP does not include
the submodules, so use a Git clone for these instructions.

### 3. Build release or debug

For a Linux release build, replace the path below with the absolute path to your
existing Master of Orion II installation. Choose a folder you can write to.

```sh
cmake --preset linux-release -DORION2RE_GAME_DIR="/absolute/path/to/Master of Orion II"
cmake --build --preset linux-release --parallel
```

The first command configures the build. The second compiles the game and copies
the executable and runtime data into your game folder. Existing
`data/orion2re.cfg` files are preserved; a default is copied only if one is missing.
Start the Linux game from that folder:

```sh
cd "/absolute/path/to/Master of Orion II"
./orion2re
```

For a debug build, return to the source directory and use:

```sh
cmake --preset linux-debug -DORION2RE_GAME_DIR="/absolute/path/to/Master of Orion II"
cmake --build --preset linux-debug --parallel
```

Release builds are optimized for playing; debug builds are useful for diagnosing
problems. They have separate build directories under `out/build/Linux/`, but
building either with the same game directory replaces the executable there.

For other platforms, replace the preset in both the configure and build commands:

| Platform | Release preset | Debug preset |
| --- | --- | --- |
| Linux | `linux-release` | `linux-debug` |
| macOS | `osx-release` | `osx-debug` |
| Windows x64 | `win64-release` | `win64-debug` |
| Windows x86 | `win32-release` | `win32-debug` |

Use a game directory appropriate for that platform, such as
`-DORION2RE_GAME_DIR="C:/Games/Master of Orion II"` on Windows. Launch
`orion2re.exe` on Windows or `Orion2 Redone.app` on macOS after a release build.
macOS debug builds use the `orion2re` executable directly.

For additional profiles, including sanitizer and router-only builds, see
[`CMakePresets.json`](CMakePresets.json). The `linux-router` preset builds only
`orion2re-router`, not the game.

#### Runtime data and mods

Normal game builds package `data/base/` as `data/base.zip` and each supported mod
under `data/mods/` as a ZIP, then copy them into `ORION2RE_GAME_DIR` when it is set.
Without that option, the executable and packaged data stay in the build directory,
such as `out/build/Linux/linux-release/`.

To build only the mod archives, or copy them into the configured game directory:

```sh
cmake --build --preset linux-release --target package-supported-mods
cmake --build --preset linux-release --target deploy-supported-mods
```

The second command requires `ORION2RE_GAME_DIR` to be set. Mod archives are written
under the build directory at `data/mods/<package>.zip`.

Mods are not enabled automatically. Put personal settings and enabled mods in
`data/user.cfg`, which is loaded after `data/orion2re.cfg`. For example, add
`enable v150;` for standard 1.50, `enable v150i;` for the single-player 1.50
Improved profile, or `enable v150i-mp;` for its multiplayer overrides.
See [Configuration](docs/content/manual/legacy-modding/configuration.md).

### 4. Run tests

Tests are opt-in and are not built by the normal release and debug presets.
From the source directory on Linux or macOS, configure, build, and run the unit
tests with:

```sh
tools/run-tests.sh
```

This uses a separate debug build in `out/build/test-debug/` and reports failures
through CTest, CMake's test runner.

To run the sanitizer-enabled tests used by CI on Linux:

```sh
cmake --preset linux-debug-asan-tests
cmake --build --preset linux-debug-asan-tests --parallel
ctest --preset linux-debug-asan-tests
```

On macOS, use `osx-debug-asan-tests` in all three commands. On Windows, use
`win64-debug-asan-tests` from the x64 Native Tools Command Prompt. Linux and
macOS run AddressSanitizer and UndefinedBehaviorSanitizer; Windows runs
AddressSanitizer only.

The Python build-tool and packaging regression tests run separately:

```sh
python3 -m unittest discover -s tests/build_tools -v
python3 -m unittest discover -s tests/packaging -v
```

On Windows, use `python` instead of `python3`.
