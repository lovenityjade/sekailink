#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sekaiemu::spike {

std::vector<std::uint8_t> ReadWholeFile(const std::filesystem::path& path);
std::string HexPreview(const std::uint8_t* data, std::size_t length);

}  // namespace sekaiemu::spike
