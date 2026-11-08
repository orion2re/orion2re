#pragma once

#include <string>
#include <vector>

#include "config/cfg_parser.h"
#include "config/mod_manifest.h"

namespace config {

    enum e_mod_source {
        MOD_SOURCE_BUNDLED,
        MOD_SOURCE_USER,
    };

    struct s_mod_catalogue_entry {
        std::string id;
        std::string name;
        std::string description;
        std::string version_text;
        std::string engine_range;
        std::vector<s_mod_dependency> dependencies;
        std::vector<std::string> conflicts;
        // True when the id is in the enabled set the catalogue was
        // built with (the config file's `enable` lines).
        bool enabled = false;
        e_mod_source source = MOD_SOURCE_BUNDLED;
        // True when the manifest is missing or fails the loader's
        // validation, or the manifest's config file is missing from
        // the package directory. A broken mod is listed, never
        // silently dropped, and cannot be enabled.
        bool broken = false;
        // The loader's diagnostic (or the missing-file reason) when
        // broken; empty otherwise.
        std::string reason;
    };

    // Enumerates every installed mod: subdirectories and `.zip`
    // files of each bundled mods root (`data/mods/` under a VFS data
    // search path) and of the user mods root
    // (`~/.local/share/orion2re/mods/`). The shipped game carries its
    // mods as ZIPs; the loader supports both forms, so the catalogue
    // reads the manifest out of the ZIP. A package with both forms in
    // one root is listed broken, as the loader refuses it. A user mod
    // with the same id replaces the bundled one. Each manifest is
    // validated with exactly the checks Load_Mod_Manifest performs
    // (via Load_Mod_Manifest_From_Source), plus a check that the
    // manifest's config file exists in the package (directory or ZIP).
    // No `data/` payload check: config-only mods (for example `ice-x`)
    // ship no `data/` directory and are valid.
    //
    // Entries are sorted by id. `enabled_ids` is the set of ids to
    // mark enabled, in file order.
    std::vector<s_mod_catalogue_entry> Build_Mod_Catalogue(
        const std::vector<std::string>& bundled_mods_roots,
        const std::string& user_mods_root,
        const std::vector<std::string>& enabled_ids);

    // Same, with the roots the game uses: the VFS base path's
    // `data/mods/` as the bundled root, and the user data directory's
    // `mods/` as the user root.
    std::vector<s_mod_catalogue_entry> Build_Mod_Catalogue(
        const std::vector<std::string>& enabled_ids);

    // Live `enable` ids from a config file, in file order: the lines
    // before any `stop`, from the file itself. Includes are not
    // followed: the enable writer refuses a file whose includes enable
    // mods, so an included enable would make the written set differ
    // from the effective set. False when the file cannot be read or
    // fails to parse.
    bool Read_Enabled_Mods(const std::string& path,
        std::vector<std::string>* enabled_ids, s_cfg_diagnostic* diagnostic);

} // namespace config
