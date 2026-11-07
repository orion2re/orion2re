#pragma once

#include <string>

namespace startup {
    bool Set_Game_Directory(std::string* directory, std::string* error);
    // Pick the directory the VFS reads runtime data from. A game directory
    // that ships its own packaged base data wins; otherwise the user's data
    // directory is used when it holds the packaged base data, then the
    // installed data directory. When none holds it, the game directory is
    // returned unchanged.
    bool Resolve_Data_Directory(const std::string& game_directory, const char* user_datadir, const char* installed_datadir, std::string* directory);
} // namespace startup
