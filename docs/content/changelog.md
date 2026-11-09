# Changelog

## 2.0.1

<!-- History reviewed: v2.0.0..e08db1a. -->

### Interface and startup

- Added settings for the initial window size and starting in fullscreen mode.
- Added startup options to choose a configuration file, skip the intro, or load a saved-game slot directly.
- Disabled the thank-you dialog on normal exit by default, with an option to restore it; error messages still appear.
- Added an option to put serious Turn Summary messages, such as starvation and lost colonies, before routine reports without hiding any messages.
- Added a classic display option for Maximum galaxies at the farthest zoom level, restoring star names, normal star artwork, black-hole animation, and manual influence-overlay controls.
- Updated Tech Review ordering, including the placement of Galactic Currency Exchange and several ship systems.

### Fixes

- Fixed colony-ship and outpost-ship dispatch controls on the Planets screen, including button click areas and selecting the available ship type.
- Fixed recalling ships from the Planets screen so recall affects the dispatched ship rather than an idle alternative, and arrival estimates update immediately.
- Fixed Planets-screen ship icons and click areas after scrolling, sorting, or filtering, including when no idle colony or outpost ships remain.
- Fixed combat-screen crashes and invalid displays when no ship is selected, including shield arcs, movement highlights, status panels, and ship details.
- Fixed retreat animations when a ship is immobilized or captured while turning to escape.
- Made multiplayer combat messages safer when they refer to a ship or player that is no longer valid.
- Fixed ground-combat handling when one side runs out of troops, and corrected surviving-troop graphics when combat is fast-forwarded.
- Fixed fleet movement orders at the ship limit and strategic-combat target selection in very large battles.
- Fixed AI interception checks for ships already in transit.
- Fixed AI colonies considering lower-tier orbital defenses when a Star Fortress is already present.
- Prevented industrial accidents and other non-player attacks from writing into the wrong war-loss records.
- Fixed planet names in GNN reports for industrial accidents, mineral depletion, and new mineral deposits.
- Fixed the star system shown in leader-offer popups and safely handled empires without a capital.
- Fixed galaxy-map positioning when zooming all the way out or centering the view near map edges.
- Made Antaran and monster spawn positions independent of the player's map zoom, keeping subsequent random outcomes consistent too.
- Fixed corrupted colors and lingering popup frames after cinematics.
- Restored normal timing for timed dialogs even when their background animation is inactive.
- Safely recovered invalid remembered player counts when opening New Game or importing setup preferences from a save.
- Fixed a crash when formatting fractional colony values.
- Improved reliability when reading game and save data, drawing animations, opening research and leader screens, calculating scores, and running Lua mods, especially on platforms with stricter memory requirements.
- Fixed refreshing mod graphics, text, fonts, and sound effects when loading saves, reloading configuration, or returning to the main menu, so resources from the previous setup are not left behind.
- Corrected localized ship-scrapping prompts when base text overrides are active.

### Modding and configuration

- Added plain-text JSON5 replacements for individual game strings, so mods can change supported messages and labels without editing whole binary resources; see [JSON5 string overrides](authoring-mods/string-overrides.md).
- Made text replacements from multiple mods combine by string ID while respecting mod priority and complete resource replacements.
- Preserved original character and message-control bytes in text replacements, including localized strings and event messages.
- Expanded each customizable Tech Review category to 256 entries, giving mods more room to organize technologies.
- Added separate structural-hit-point settings for stationary Crystals, Hydras, and Dragons and for event Crystals and Dragons, while retaining the shared settings.
- Added an optional Displacement Device defense against Stellar Converter shots; it is off by default.
- Added a configurable Phase Shifter travel-speed multiplier; normal travel speed remains the default and combat speed is unchanged.
- Added a configurable galaxy-wide satellite-trimming threshold for maps with up to 72 stars, including an option to disable trimming; gas giants now count alongside planets and asteroid belts, matching 1.50.
- Renamed the leader-offer setting `leaders reject_declined_offers` to `leaders rejected_offer_keeps_timer`; existing configurations using the old name must be updated.
- Renamed the weapon firing-arc value `rear` to `back` and clarified the difference between firing directions and arc cost/space categories; mod configurations using `rear` must be updated.

### Bundled mods

These changes apply when the named mod is enabled, not to every game profile.

- **1.50 (Basic):** Now puts serious turn reports first, disables VSync, uses faster screen fades, and leaves history graphs unsmoothed; several of these preferences were previously set only by Improved.
- **1.50 (Basic):** Ship Design's Design button now preserves the chosen shield and computer, while Clear updates them to the best available models.
- **1.50 (Basic):** Now selects classic Maximum-galaxy display and zoom behavior, also inherited by mods based on Basic unless overridden.
- **1.50 (Basic):** Set the earliest comet, supernova, and time-space-anomaly events to turn 150.
- **1.50 (Basic):** Converted the bundled English text replacements to editable JSON5 files and refreshed event, interface, and help wording.
- **Improved:** Revised Elerian and Trilarian AI race choices on Impossible difficulty.
- **Improved:** Delayed the earliest Antaran attacks to turn 150 in Pre-Warp games and turn 120 in Average-start games, and reduced attack frequency.
- **Improved:** Maximum galaxies now use 90 stars; disabled satellite trimming on smaller maps and revised climate-generation chances in young and average-age galaxies.
- **Improved:** Aquatic races now treat Tundra as Swamp rather than Terran, with matching help text.
- **Improved:** Increased stationary Dragon structure to 600 and event Dragon structure to 3,000, and removed their Class I shields.
- **Improved:** Adjusted the Colony Tech Review order for Recyclotron and Robotic Factory.
- **ICE:** Removed custom main-menu version-label placement to use the shared layout.
- Shortened the displayed name of Only Home Systems Mirrored to Home Systems Mirrored; its behavior is unchanged.
- Updated bundled galaxy-size and enemy-presence filter help, and refreshed Improved's race-creation wording.
- Removed the standalone No Diplomacy Reports mod; Improved multiplayer and VDC GM3 retain that restriction in their own settings, while single-player Improved leaves reports available.

### Installation and documentation

- The game now finds its files relative to the executable on all platforms, so launching from a shortcut or another working directory uses the correct installation.
- Added Linux system-wide installation support and optional per-user locations for saves, settings, mods, and logs; portable installations remain supported and files are not moved automatically. See [File locations](manual/overview/file-locations.md).
- Improved missing-original-game-data errors to identify the required file and the directories searched.
- Added automated Windows and macOS game packages, plus offline HTML documentation and a standalone PDF, to development downloads and published releases.
- Update packages leave the user's configuration file intact, and release filenames identify the version they contain.
- Normal release builds no longer create diagnostic log files during ordinary play.

## 2.0.0

- Initial official release and the starting point for this changelog. See [Orion2 Redone highlights](manual/overview/highlights.md) for a general overview of the project.
