#pragma once

#include <string>

namespace startup {
    bool Set_Game_Directory(std::string* directory, std::string* error);
    // Pick the directory the VFS reads runtime data from. A game directory
    // that ships its own packaged base data wins; otherwise the installed
    // data directory is used when it holds the packaged base data. When
    // neither holds it, the game directory is returned unchanged.
    bool Resolve_Data_Directory(const std::string& game_directory, const char* installed_datadir, std::string* directory);
} // namespace startup
