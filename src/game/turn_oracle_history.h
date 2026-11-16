#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace turn_oracle {
    bool Read_File(const std::filesystem::path& path, std::vector<uint8_t>* bytes, std::string* error);
    bool Write_File(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string* error);
    bool Compare_Files(const std::filesystem::path& canonical, const std::filesystem::path& actual, std::string* error);
    std::filesystem::path Turn_Path(const std::filesystem::path& directory, int32_t turn);
} // namespace turn_oracle
