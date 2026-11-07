#pragma once

#include <string>
#include <vector>

#include "config/cfg_parser.h"

namespace config {

    // Minimal, refuse-rather-than-corrupt writer for the `enable` lines of an
    // orion2re.cfg file (mod picker, Phase 3.4 of the mod-picker plan).
    //
    // Contract:
    // - Only `enable <id>;` lines are added, removed or reordered, in the
    //   order the caller passes them (highest-priority first). Dependency
    //   order is the caller's responsibility; the writer never resolves
    //   dependencies.
    // - Everything else is preserved byte-for-byte where possible: comments,
    //   other settings, `include` lines, blank lines and the file's existing
    //   formatting. The rewrite is surgical: it uses the parser's byte
    //   offsets and replaces only the byte ranges of the enable statements.
    //   It never re-serializes the whole document.
    // - The writer operates on the path the caller passes (the picker passes
    //   the user XDG config, `~/.config/orion2re/orion2re.cfg`). It never
    //   hardcodes a path and must not be pointed at the system datadir.
    //
    // Refuse rather than corrupt: on any failure the functions return false,
    // fill `diagnostic` with the problem and the line, and leave the file
    // untouched. Refusals:
    // - the file fails to parse (the parser's diagnostic is reported);
    // - an `enable` statement is not a single-line `enable <id>;` statement
    //   (a comment or line break inside the statement would be lost by a
    //   surgical replacement, so the writer refuses instead of dropping it);
    // - a desired mod ID is not a valid identifier, or is duplicated;
    // - a live `include` target cannot be read, fails to parse, or itself
    //   enables a mod (Write_Enabled_Mods only; the in-memory Rewrite_
    //   API has no filesystem access and cannot check includes): the writer
    //   only updates enable lines in the file it was given, so an included
    //   enable would make the written set differ from the effective set.
    //   Includes are resolved relative to the config file's directory,
    //   mirroring the loader;
    // - a live `enable` statement sits after a `stop` statement: the loader
    //   stops reading at `stop`, so only enable statements before it are
    //   live and managed; anything after it is left byte-for-byte. If the
    //   file has a `stop` but no live enable lines, appending is refused:
    //   new lines at the end of the file would sit after `stop` and never
    //   take effect. Likewise, if `stop` shares the line of the last live
    //   enable line and new lines would have to be appended, appending is
    //   refused: the appended lines would land after `stop` on that line
    //   and never take effect.
    //
    // Limits (mirroring the loader): a config larger than 16 MiB is
    // refused, and include nesting is capped at 64 files including the
    // entry file (63 nested includes).
    //
    // Atomicity: the new content is built fully in memory and written to a
    // temporary file in the same directory, then renamed over the original.
    // A failure at any point leaves the original untouched, and a file whose
    // desired set already matches is not rewritten at all. If the config
    // path is a symlink, the link's target is written through and the link
    // itself is preserved; a target that cannot be resolved is refused.
    //
    // Limitations: the rename is atomic but the write is not fsynced, so
    // an OS crash may lose the write (the file itself is never left
    // half-written). Includes are resolved against the real filesystem,
    // not the game's VFS, so an include target visible only through the
    // VFS is refused as unresolvable.

    bool Rewrite_Enabled_Mods(const char* source_name, const std::string& source,
        const std::vector<std::string>& enabled_mods, std::string* rewritten,
        s_cfg_diagnostic* diagnostic);

    bool Write_Enabled_Mods(const std::string& path,
        const std::vector<std::string>& enabled_mods, s_cfg_diagnostic* diagnostic);

} // namespace config
