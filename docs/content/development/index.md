# Development

This section documents restored engine subsystems whose behavior is useful when changing the C++ source. It describes the current implementation rather than the interfaces exposed to players or mod packages.

The restoration deliberately preserves some original runtime models where replacing them would obscure game behavior or require a broad rewrite. Other platform-specific parts have been replaced with explicit cross-platform boundaries. The pages in this section call out both cases.

## Contents

- [Fields](fields.md) describes the global field list used for buttons, hit regions, text input, lists, help, and keyboard shortcuts.
- [Networking](networking.md) describes the router-based TCP transport, LAN discovery, session model, and game-owned disconnect handling.
- [Threading and SDL](threading.md) describes the two-thread arrangement inherited from `orion95.exe` and the frame, input, cursor, and exit handoffs used by the SDL port.
- [GPU scaling](gpu-scalers.md) describes the optional Vulkan presentation backend, shader validation, and profiling.

These are implementation guides, not stable extension APIs. Mods should use the interfaces documented under [Authoring Mods](../authoring-mods/index.md) instead of depending on native structures or thread behavior.
