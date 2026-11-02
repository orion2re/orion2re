# Outstanding acceptance gates

AI-assisted summary. Current status is in `plan.md`; this file retains unique
dependency/profiling/compatibility evidence, not an increment diary.

## P0-06: backend and platforms

`backend.json` pins libwebsockets v4.5.4 at
`c56adc3c62ee2545c77e0ba4cce67441be6955db`. The optional build uses project libuv
`840404ce8ba7cc0204be52389a6cfff9f2c90fb6`; default/router-only builds do not link
RPC/backend dependencies. The selected profile excludes TLS, compression, clients,
examples and backend JSON-RPC; it needs libc/platform networking and libuv, not
OpenSSL, zlib, mbedTLS, Brotli, SQLite or display dependencies. File operations
remain enabled because the pinned backend unconditionally compiles Windows file
operations that require the corresponding types.

Primary evidence:

- [Pinned license](https://github.com/warmcat/libwebsockets/blob/c56adc3c62ee2545c77e0ba4cce67441be6955db/LICENSE)
- [Pinned build/platform documentation](https://github.com/warmcat/libwebsockets/blob/c56adc3c62ee2545c77e0ba4cce67441be6955db/READMEs/README.build.md)

The main library is MIT; retain applicable BSD/SHA-1 and selected-component
notices. The broader upstream tree includes BSD, zlib, Apache, OFL and CC0
components; review the actual selected source/dependency set, not merely the main
license. Technical compatibility review is not legal/maintainer approval.

Local macOS ARM64 / AppleClang 17 builds, the foreign-loop backend probe, live
transport and ASAN/UBSAN have been exercised. `tools/check_rpc_backend.py` verifies
the existing checkout's pin/profile and writes configure/build/link logs under
ignored `out/`; it does not download/install dependencies. Backend shutdown
constraints are in [p1.md](p1.md). Upstream platform claims and configured CI jobs
are not observed project matrix results: supported Linux/Windows/MinGW/macOS
variants still require release evidence. P0-06 remains open.

## P0-08: lifecycle audit

[lifecycle.md](lifecycle.md) records pump classes, field replacement/restoration,
committed publication and exception-safe teardown. Appendix B of
[planning_baseline.md](planning_baseline.md) retains the native owner inventory.
Static audit is not dynamic route certification or permission for a generic
action pump. Actual owner adapters need initialized-lifetime and interruption tests.

## P0-09: profiling

Finite provisional bounds are in `contracts.json`, not approved performance SLOs.
Source capacities include 1,024 stars, 10,000 ships, 5,120 planets/colonies,
8 players, 67 leaders and 57 nebulas.

Historical initial-map synthetic measurement (macOS ARM64 Debug): 1,024 systems,
57 nebulas, maximum-width coordinates/CP850 names; 211,902 encoded bytes,
1,623 microseconds capture/identity setup, 1,158 microseconds encoding. This is
one run of the earlier map subset, not current full-catalog latency evidence.
Maximum-capacity native fixtures additionally exercise ship/colony/planet bounds
and explicit response overflow. They do not close maximum-game profiling.

Before finalizing budgets, measure projection/encoding time, copied/encoded bytes,
temporary and retained memory at cold and p50/p95/max across topic subsets,
maximum games/mods, no clients/readers/controllers/multiple subscribers and slow
readers. Include reconnect/baseline cost and hidden-only noninterference. Record
revision/platform/build/scenario with automated output, not a manual narrative.

## P0-10: interactive compatibility

`native_baseline.json` records synthetic native-function checks and fixed RNG
checkpoints, not interactive parity. Obtain legitimate local assets and private
save/interaction fixtures for new/load/turn, research, colony jobs/queues/buy/
demolish/transfers, fleets, diplomacy, combat/ground, film skip, hotseat and network
host/client/cancel paths. Never add proprietary assets/saves to the repository.

Compare matching native decisions, save copies and RNG state/call counts with RPC
off, enabled/no client, readers and controllers. Record mod/config digests and
seeds. Explain nondeterministic metadata without normalizing gameplay differences
away. Interactive saved-game/mod/RNG, multiplayer and supported-platform parity
remain open; synthetic/native-function and live transport tests cannot replace them.
