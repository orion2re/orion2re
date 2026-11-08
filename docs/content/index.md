# Orion2 Redone

**A community-built native restoration of Master of Orion II engine.**

Orion2 Redone, aka 2.00, restores the classic turn-based strategy game as a native application for Windows, macOS, and Linux. It preserves the original game rules while incorporating audited fixes and useful extensions from the unofficial 1.50 patch.

Build an empire in the familiar game, now with updated display support, larger galaxies, cross-platform multiplayer, extensive configuration, and packaged mods. Whether you want to play, customize the rules, create a mod, or understand the restored engine, this documentation is the place to start.

!!! important "Original game data required"

    Orion2 Redone does not include the original Master of Orion II assets. You must own a supported copy of version 1.31 from GOG, Steam, or an updated CD installation. See the [introduction](manual/overview/introduction.md) for details.

## What version 2.00 offers

- **Cross-platform play:** Native, resizable-window and fullscreen support on Windows, macOS, and Linux, with selectable presentation scalers and display options.
- **Restored and corrected gameplay:** Original rules plus fixes for crashes, stalls, interface errors, tactical combat, and network synchronization. Behavior has been [oracle-tested](https://en.wikipedia.org/wiki/Test_oracle) against the original `orion95.exe` over many thousands of turns.
- **Larger galaxies:** Expanded map support with overview markers and influence views for navigating bigger games.
- **Modern multiplayer:** Local network, Internet, and Hotseat play, with compatibility checks for builds, configuration, mods, and scripts.
- **Configuration and modding:** Plain-text settings and versioned packages that can change rules, replace assets, and add Lua behavior.

[See the version 2.00 highlights](manual/overview/highlights.md)

## Choose your path

| I want to... | Start here |
| --- | --- |
| See what changed since the last release | [Changelog](changelog.md) |
| Set up the game and understand version 2.00 | [Introduction](manual/overview/introduction.md) and [compatibility](manual/overview/compatibility.md) |
| Learn the interface, rules, and game mechanics | [Player manual](manual/index.md) |
| Look up a setting and its default value | [Configuration reference](generated/configuration/index.md) |
| Install, enable, or troubleshoot a mod | [Using mods](manual/legacy-modding/index.md) |
| Create or migrate a mod package | [Authoring mods](authoring-mods/index.md) |
| Host or join a multiplayer game | [Multiplayer guide](manual/multiplayer.md) |
| Work on the restored engine | [Development notes](development/index.md) |

## Explore the documentation

### Player manual

Start with the [manual](manual/index.md) for setup, player-visible changes, game mechanics, keyboard shortcuts, and reference material. The manual builds on the historical 1.50.26 manual, with current information checked against version 2.00.

### Configuration reference

The generated [configuration reference](generated/configuration/index.md) lists supported settings, accepted values, and built-in defaults directly from the runtime schema.

### Mods

Players can use [Using mods](manual/legacy-modding/index.md) to install and select completed packages. Mod creators can use [Authoring mods](authoring-mods/index.md) for manifests, configuration overrides, asset replacement, Lua APIs, persistence, and migration from 1.50.

### Development

The [development notes](development/index.md) describe selected restored engine subsystems for source contributors. These implementation guides are separate from the stable interfaces exposed to mod packages.

## Project and provenance

Orion2 Redone is an independent restoration effort. Master of Orion II and its original assets remain the property of their respective owners. See the [acknowledgements](manual/reference/acknowledgements.md) and [third-party software](manual/reference/third-party-software.md) pages for project history and dependency credits.