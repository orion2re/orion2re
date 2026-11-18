# Turn savegame oracle

The debug runner records `turn-000000.GAM` plus one save per turn, or replays an
existing `history.txt` directory using `-oracle-replay=<directory>`. Playback
loads turn zero before simulating; it does not independently regenerate a galaxy
from its seed.

## Save versions 7 and 8

Version 7 identifies planets by record position. Version 8 writes explicit IDs.
For an ordinary replay, old position **i must remain ID i**. Terraforming,
Stellar Converter destruction, and artificial-planet construction mutate existing
orbital records; they do not require reallocating planet identities. Creation,
deletion, slot reuse, and compaction during generation are a different boundary.
Two independently generated galaxies therefore must not be substituted for replay
from the same saved starting state.

The save comparator decodes both versions into a detached version-8 representation.
It does not load either save into the running game, change configuration or Lua
state, consume RNG values, or remap IDs by matching planet properties.

It compares planet IDs, all planet fields, and their serialized references in
colonies, stars, players, settlers, ships and events. Counts, allocator state,
RNG state, other actor records, settings, turn globals and extension bytes remain
checked. Version-7 allocator state is inferred as the planet record count.
Known encoding changes are decoded: packed freight words become plain settler
records, event planet targets become typed fields, and colony-ship weapon type
slots used as auto-colonization metadata become an optional target. Unused old
freight slots and obsolete control-slot encodings are not native gameplay state.

Diagnostics identify the first differing section/record; planet differences also
name the field. The original binary `Compare_Files` helper remains available for
byte-for-byte checks.
Same-version oracle comparisons additionally remain byte-exact, including fields
whose obsolete version-7 control encoding is ignored during cross-version checks.

## Focused checks

```sh
cmake --build --preset osx-debug-asan-tests --target test_turn_oracle_history
ctest --preset osx-debug-asan-tests -R '^test_turn_oracle_history$' --output-on-failure
```

An optional local-history test checks numbering across a recorded run and checks
each save through version-7-to-8 conversion and semantic comparison:

```sh
ORION2RE_ORACLE_HISTORY=/path/to/history \
  ctest --preset osx-debug-asan-tests -R '^test_turn_oracle_history$' --output-on-failure
```

The local-history check never changes the input directory. Conversion checks
verify representation equivalence, not the behavior of newly simulated turns.
A full game build and actual `-oracle-replay` run are still required to verify
the new implementation's gameplay equivalence.
