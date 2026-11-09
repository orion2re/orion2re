# Contributing to orion2re

This project restores and maintains the Master of Orion II engine. Contributions may include bug fixes, restoration work, tests, documentation, portability improvements, and other focused changes.

## Preserve game behavior

Treat compatibility and established game behavior as project requirements. They take priority over cleanup, redesign, balance changes, and personal preference.

Before changing gameplay, saved games, multiplayer behavior, network protocols, deterministic execution, data formats, mod compatibility, or compatibility with original game data:

- Open an issue or discussion. Describe the intended behavior and compatibility impact.
- Distinguish original game behavior from a defect in this project.
- Provide evidence for the proposed behavior where practical.
- Keep compatibility changes separate from unrelated refactoring.
- Include focused tests or a reproducible manual test plan.

Discuss and obtain approval before implementing a change that could cause crashes, data loss, save corruption, incompatible multiplayer sessions, or irreversible format changes. Include a migration or rollback plan where applicable.

Maintainers may decline a technically sound change when its compatibility risk, maintenance cost, or departure from the restoration goals is too high. Discuss and obtain approval for any required compatibility break before implementation.

## Take responsibility for submitted work

Take responsibility for every change submitted under your name. This applies to work written by hand, produced by a script or generator, adapted from another source, or created with AI assistance.

For every submitted change:

- Understand and be able to explain the change.
- Review generated or assisted output as carefully as handwritten work.
- Verify correctness, security, compatibility, provenance, and licensing.
- Run appropriate tests and report the results accurately.
- Address review feedback and defects introduced by the change.

Do not submit speculative bulk changes or generated output that you have not personally reviewed. Disclose generation or AI assistance when it produced or materially rewrote non-trivial content included in the contribution. Code completion, spelling, and grammar suggestions do not need separate disclosure when they do not produce or rewrite non-trivial content.

## Prepare reviewable changes

Keep changes focused, readable, tested, and consistent with the surrounding code. Follow the [project style guide](STYLEGUIDE.md). Avoid unrelated cleanup. Explain non-obvious design decisions.

Maintainer time is limited. Review and acceptance are not guaranteed. Response times vary. Keep the same quality and scope while waiting for review. Small, self-contained changes with clear motivation and evidence are easier to review.

Maintainers may ask for a change to be split, revised, or withdrawn when it is too broad to review safely. Inactive or out-of-scope submissions may be closed. Do not repeatedly request a review. Add a concise update when new information is available.

## Copyright and licensing

### Do not submit proprietary or unlicensed material

Do not add material copied or extracted from Master of Orion II or another proprietary source. This includes executables, art, music, sound, video, fonts, substantial text, game assets, proprietary source code, and data extracted from the original game.

Where the material was obtained does not change this restriction. Do not submit such material from an official release, unofficial distribution, modification, or online source. Submit original assets or third-party assets intended for this project only after maintainers agree to include them. Document their authorship, provenance, and compatible license.

This repository does not distribute the original game data. Users must provide files from a legitimate copy of the game. Code that interoperates with user-supplied game data should keep that data outside the repository and build artifacts.

### Check licenses and provenance

Submit only work that you have the legal right to contribute. The work must be your own, or its source and terms must allow you to provide it under the applicable project license.

Original project code and documentation use the GNU General Public License, version 3 or any later version. See [LICENSE](LICENSE). Contributions to project-owned files must be distributable under that license.

Files under `vendors/` retain their existing licenses and notices. Other identified third-party components retain their own licenses and notices. Changes to third-party files must follow the upstream license and should normally be submitted upstream first.

When adapting third-party material or adding a dependency:

- Identify its source, author, version, and license.
- Confirm that the license is compatible with this repository and the intended distribution.
- Preserve required copyright and license notices.
- Review transitive dependencies and target-platform constraints.
- Explain why the dependency is necessary.

Do not copy code merely because it is publicly visible. Code without a clear compatible license is not acceptable. Do not remove or alter existing license notices unless the change is legally justified and approved.

Stop and ask before submitting material with uncertain provenance or licensing.

## Preparing a change

1. Search existing issues and pull requests for related work.
2. Discuss large, risky, compatibility-sensitive, or dependency-adding changes before investing substantial effort.
3. Keep each change narrowly scoped. Separate behavior changes from refactoring and formatting.
4. Follow existing architecture and the [project style guide](STYLEGUIDE.md).
5. Add or update tests when behavior changes or a defect is fixed.
6. Regenerate generated files from their canonical inputs; do not edit generated output by hand.
7. Build the affected targets and run the relevant tests.

Initialize vendored submodules before building:

```sh
git submodule update --init --recursive
```

The standard unit-test command is:

```sh
tools/run-tests.sh
```

Test relevant platform presets or configurations when a change affects platform-specific code. See [README.md](README.md) and `CMakePresets.json` for available presets and build instructions.

CI runs the C++ tests with ASan + UBSan on Linux x64 and macOS ARM64, and with
ASan on Windows x64 using native MSVC (which does not support UBSan).
It uses `linux-debug-asan-tests`, `osx-debug-asan-tests`, and
`win64-debug-asan-tests`, respectively, for configure, build, and CTest.
The Windows preset requires an x64 Visual Studio developer environment with
the MSVC AddressSanitizer component, CMake, Ninja, and Python installed.
All three test jobs must pass before CI builds release packages.

## Pull requests

Include:

- The problem and why the change is appropriate for a restoration project.
- A concise description of the implementation.
- Any relevant impact on gameplay, saves, multiplayer, protocols, determinism, mods, or platform compatibility.
- Tests performed, including commands and manual checks.
- Known limitations or follow-up work.
- Provenance and license information for adapted code, dependencies, and other third-party material.

Before submitting, confirm that:

- The change is focused and contains no unrelated formatting or cleanup.
- Tests pass, or each test limitation is explained.
- No proprietary game assets or source material are included.
- All contributed material has clear, compatible licensing and provenance.
