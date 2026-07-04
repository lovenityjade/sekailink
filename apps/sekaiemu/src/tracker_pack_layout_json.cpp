#include "tracker_pack_layout_json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace sekaiemu::spike {

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_pack_layout_open_failed:" + path.string());
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::string StripJsonComments(std::string text) {
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.erase(0, 3);
  }

  std::string out;
  out.reserve(text.size());
  bool in_string = false;
  bool escaping = false;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char ch = text[index];
    const char next = index + 1 < text.size() ? text[index + 1] : '\0';
    if (in_string) {
      out.push_back(ch);
      if (escaping) {
        escaping = false;
      } else if (ch == '\\') {
        escaping = true;
      } else if (ch == '"') {
        in_string = false;
      }
      continue;
    }
    if (ch == '"') {
      in_string = true;
      out.push_back(ch);
      continue;
    }
    if (ch == '/' && next == '/') {
      while (index < text.size() && text[index] != '\n') {
        ++index;
      }
      if (index < text.size()) {
        out.push_back(text[index]);
      }
      continue;
    }
    out.push_back(ch);
  }
  return out;
}

nlohmann::json ParseJsonWithComments(const std::filesystem::path& path) {
  return nlohmann::json::parse(StripJsonComments(ReadTextFile(path)));
}

std::string CanonicalToken(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isalnum(ch) != 0) {
      out.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return out;
}

std::vector<std::string> ParseCodeList(const nlohmann::json& raw, const char* key) {
  std::vector<std::string> codes;
  if (!raw.contains(key)) {
    return codes;
  }
  const auto& value = raw[key];
  auto append_code = [&](std::string code) {
    code.erase(std::remove_if(code.begin(), code.end(), [](unsigned char ch) {
                 return std::isspace(ch) != 0;
               }),
               code.end());
    if (!code.empty()) {
      codes.push_back(code);
    }
  };
  if (value.is_string()) {
    std::stringstream stream(value.get<std::string>());
    std::string token;
    while (std::getline(stream, token, ',')) {
      append_code(token);
    }
  } else if (value.is_array()) {
    for (const auto& entry : value) {
      if (entry.is_string()) {
        append_code(entry.get<std::string>());
      }
    }
  }
  return codes;
}

void AppendUniqueCode(std::vector<std::string>& codes, std::string code) {
  code.erase(std::remove_if(code.begin(), code.end(), [](unsigned char ch) {
               return std::isspace(ch) != 0;
             }),
             code.end());
  if (code.empty() || std::find(codes.begin(), codes.end(), code) != codes.end()) {
    return;
  }
  codes.push_back(std::move(code));
}

void AppendCodeList(std::vector<std::string>& codes, const std::vector<std::string>& incoming) {
  for (const auto& code : incoming) {
    AppendUniqueCode(codes, code);
  }
}

std::string JsonStringFlexible(const nlohmann::json& root, const char* key) {
  if (!root.is_object()) {
    return {};
  }
  const auto it = root.find(key);
  if (it == root.end()) {
    return {};
  }
  if (it->is_string()) {
    return it->get<std::string>();
  }
  if (it->is_number_integer()) {
    return std::to_string(it->get<std::int64_t>());
  }
  if (it->is_number_unsigned()) {
    return std::to_string(it->get<std::uint64_t>());
  }
  return {};
}

int JsonIntFlexible(const nlohmann::json& root, const char* key, int fallback) {
  if (!root.is_object()) {
    return fallback;
  }
  const auto it = root.find(key);
  if (it == root.end()) {
    return fallback;
  }
  if (it->is_number_integer()) {
    return it->get<int>();
  }
  if (it->is_number_unsigned()) {
    return static_cast<int>(it->get<std::uint64_t>());
  }
  if (it->is_string()) {
    try {
      return std::stoi(it->get<std::string>());
    } catch (const std::exception&) {
    }
  }
  return fallback;
}

bool JsonBoolFlexible(const nlohmann::json& root, const char* key, bool fallback) {
  if (!root.is_object()) {
    return fallback;
  }
  const auto it = root.find(key);
  if (it == root.end()) {
    return fallback;
  }
  if (it->is_boolean()) {
    return it->get<bool>();
  }
  if (it->is_number_integer()) {
    return it->get<std::int64_t>() != 0;
  }
  if (it->is_string()) {
    auto value = it->get<std::string>();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
      return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
      return false;
    }
  }
  return fallback;
}

std::string NormalizePackAssetPath(std::string path) {
  if (path.empty()) {
    return {};
  }
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  if (path.rfind("images/", 0) == 0 || path.rfind("maps/", 0) == 0) {
    return "poptracker-adapted/" + path;
  }
  if (path.rfind("poptracker-adapted/", 0) == 0) {
    return path;
  }
  return "poptracker-adapted/" + path;
}

}  // namespace sekaiemu::spike
