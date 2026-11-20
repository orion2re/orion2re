# Savegame compatibility fixtures

These are synthetic serialization fixtures, not playable campaigns. The
original `savegame_v0.sav` uses patterned bytes to exercise the classic
layout. Its planet links and orbit indices, star links, event status bytes,
and message queue bounds are normalized on disk so it can pass load
validation without the test patching the file in memory.

`savegame_v7.sav` was written by the game from that loaded v0 state, using
default configuration and no mods. It uses valid nebula types and nonzero
values for the later Antaran, freight, jump-gate, and building-scrap fields.
The v1–v6 fixtures encode the same state in the older layouts:

| Version | Fixture layout |
| --- | --- |
| 0 | Classic header, fixed record counts, and classic field widths. |
| 1 | Versioned header and record layout, with pre-doubling planet food values. |
| 2 | Planet food values stored on the new scale. |
| 3 | Antaran runtime state, expanded freight records, and an unframed config snapshot. |
| 4 | Adds per-ship jump-gate redirect state before the config snapshot. |
| 5 | Replaces the unframed snapshot with framed config and Lua sections. |
| 6 | Same record layout as v5 with a higher ship limit; the boundary is tested separately by `test_savegame`. |
| 7 | Adds a scrap-count byte to each colony record. |

`test_savegame_compatibility` reads each file unchanged through the game's
save loader, including the production config and Lua readers. When bumping
the save version, add a fixture for the new version and keep older fixtures
unchanged so the test continues to catch changes to historical layouts.
