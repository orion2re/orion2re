#pragma once

#include <string>

namespace startup {
    bool Set_Game_Directory(std::string* directory, std::string* error);
    bool Resolve_Data_Directory(const std::string& game_directory, const char* installed_datadir, std::string* directory);
} // namespace startup
