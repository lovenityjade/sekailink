#include "host_io_utils.hpp"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace sekaiemu::spike {

std::vector<std::uint8_t> ReadWholeFile(const std::filesystem::path& path) {
  std::vector<std::uint8_t> bytes;
  std::error_code ec;
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size == 0) {
    std::cerr << "[sekaiemu-libretro-spike] ReadWholeFile failed size check for "
              << path << " ec=" << ec.message() << " size=" << file_size << "\n";
    return bytes;
  }

  bytes.resize(static_cast<std::size_t>(file_size));
  FILE* file = std::fopen(path.c_str(), "rb");
  if (!file) {
    std::cerr << "[sekaiemu-libretro-spike] fopen failed for " << path << "\n";
    bytes.clear();
    return bytes;
  }
  const auto read_bytes = std::fread(bytes.data(), 1, bytes.size(), file);
  std::fclose(file);
  if (read_bytes != bytes.size()) {
    std::cerr << "[sekaiemu-libretro-spike] fread short read for " << path
              << " expected=" << bytes.size() << " got=" << read_bytes << "\n";
    bytes.clear();
  }
  return bytes;
}

std::string HexPreview(const std::uint8_t* data, std::size_t length) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < length; ++index) {
    if (index > 0) {
      stream << ' ';
    }
    stream << std::setw(2) << static_cast<unsigned>(data[index]);
  }
  return stream.str();
}

}  // namespace sekaiemu::spike
