#include "core_option_manager.hpp"

#include <cctype>
#include <system_error>

namespace sekaiemu::spike {

std::string CoreOptionManager::NormalizeCoreKey(std::string_view name) {
  std::string output;
  output.reserve(name.size());
  for (const unsigned char value : name) {
    if (std::isalnum(value)) {
      output.push_back(static_cast<char>(std::tolower(value)));
    } else {
      output.push_back('_');
    }
  }
  while (!output.empty() && output.back() == '_') {
    output.pop_back();
  }
  return output.empty() ? "core" : output;
}

std::string CoreOptionManager::Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }

  std::size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }

  return std::string(text.substr(start, end - start));
}

std::optional<std::filesystem::file_time_type> CoreOptionManager::ReadTimestamp(
    const std::filesystem::path& path) {
  std::error_code ec;
  const auto time = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return std::nullopt;
  }
  return time;
}

}  // namespace sekaiemu::spike
