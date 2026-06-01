#include "memory_profile.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

namespace {

std::string Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }

  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }

  return std::string(text.substr(start, end - start));
}

std::vector<std::string> Split(std::string_view text, char delimiter) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto position = text.find(delimiter, start);
    if (position == std::string_view::npos) {
      parts.push_back(Trim(text.substr(start)));
      break;
    }
    parts.push_back(Trim(text.substr(start, position - start)));
    start = position + 1;
  }
  return parts;
}

bool ParseUnsigned(std::string_view text, std::uint32_t& out_value) {
  try {
    const std::string cleaned = Trim(text);
    std::size_t parsed = 0;
    unsigned long long value = 0;
    if (cleaned.starts_with("0x") || cleaned.starts_with("0X")) {
      value = std::stoull(cleaned, &parsed, 16);
    } else {
      value = std::stoull(cleaned, &parsed, 10);
    }
    if (parsed != cleaned.size() || value > 0xFFFFFFFFull) {
      return false;
    }
    out_value = static_cast<std::uint32_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

bool LoadMemoryProfile(const std::filesystem::path& path,
                       MemoryProfile& out_profile,
                       std::string& out_error) {
  std::ifstream stream(path);
  if (!stream) {
    out_error = "Could not open profile file.";
    return false;
  }

  MemoryProfile profile;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(stream, line)) {
    ++line_number;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed.starts_with('#')) {
      continue;
    }

    const auto fields = Split(trimmed, '|');
    if (fields.empty()) {
      continue;
    }

    const auto& kind = fields[0];
    if (kind == "game") {
      if (fields.size() != 2) {
        out_error = "Invalid game line at " + std::to_string(line_number);
        return false;
      }
      profile.game = fields[1];
    } else if (kind == "watch_region") {
      if (fields.size() != 4) {
        out_error = "Invalid watch_region line at " + std::to_string(line_number);
        return false;
      }
      profile.watch_region.memory_domain = fields[1];
      if (!ParseUnsigned(fields[2], profile.watch_region.start) ||
          !ParseUnsigned(fields[3], profile.watch_region.length)) {
        out_error = "Invalid watch_region numeric value at " + std::to_string(line_number);
        return false;
      }
    } else if (kind == "check") {
      if (fields.size() != 5) {
        out_error = "Invalid check line at " + std::to_string(line_number);
        return false;
      }
      CheckEntry entry;
      std::uint32_t bit_index = 0;
      if (!ParseUnsigned(fields[1], entry.location_id) ||
          !ParseUnsigned(fields[2], entry.byte_index) ||
          !ParseUnsigned(fields[3], bit_index) ||
          bit_index > 7) {
        out_error = "Invalid check numeric value at " + std::to_string(line_number);
        return false;
      }
      entry.bit_index = static_cast<std::uint8_t>(bit_index);
      entry.name = fields[4];
      profile.checks.push_back(std::move(entry));
    } else if (kind == "oot_chest_check") {
      if (fields.size() != 5) {
        out_error = "Invalid oot_chest_check line at " + std::to_string(line_number);
        return false;
      }
      OotCheckEntry entry;
      std::uint32_t bit_index = 0;
      if (!ParseUnsigned(fields[1], entry.location_id) ||
          !ParseUnsigned(fields[2], entry.scene) ||
          !ParseUnsigned(fields[3], bit_index) ||
          bit_index > 31) {
        out_error = "Invalid oot_chest_check numeric value at " + std::to_string(line_number);
        return false;
      }
      entry.bit_index = static_cast<std::uint8_t>(bit_index);
      entry.name = fields[4];
      profile.oot_chest_checks.push_back(std::move(entry));
    } else if (kind == "flag_check") {
      if (fields.size() != 4) {
        out_error = "Invalid flag_check line at " + std::to_string(line_number);
        return false;
      }
      FlagCheckEntry entry;
      if (!ParseUnsigned(fields[1], entry.location_id) ||
          !ParseUnsigned(fields[2], entry.flag_id)) {
        out_error = "Invalid flag_check numeric value at " + std::to_string(line_number);
        return false;
      }
      entry.name = fields[3];
      profile.flag_checks.push_back(std::move(entry));
    } else if (kind == "receive_slot") {
      if (fields.size() != 5) {
        out_error = "Invalid receive_slot line at " + std::to_string(line_number);
        return false;
      }
      ReceiveSlot slot;
      slot.kind = fields[1];
      slot.memory_domain = fields[2];
      if (!ParseUnsigned(fields[3], slot.offset) ||
          !ParseUnsigned(fields[4], slot.size) ||
          slot.size == 0 || slot.size > 8) {
        out_error = "Invalid receive_slot numeric value at " + std::to_string(line_number);
        return false;
      }
      profile.receive_slots.push_back(std::move(slot));
    } else if (kind == "item_alias") {
      if (fields.size() != 4) {
        out_error = "Invalid item_alias line at " + std::to_string(line_number);
        return false;
      }
      ItemAlias alias;
      alias.kind = fields[1];
      if (!ParseUnsigned(fields[2], alias.code)) {
        out_error = "Invalid item_alias numeric value at " + std::to_string(line_number);
        return false;
      }
      alias.name = fields[3];
      profile.item_aliases.push_back(std::move(alias));
    } else {
      out_error = "Unknown profile directive '" + kind + "' at line " + std::to_string(line_number);
      return false;
    }
  }

  if (profile.game.empty()) {
    out_error = "Profile is missing a game entry.";
    return false;
  }
  if (profile.watch_region.memory_domain.empty() || profile.watch_region.length == 0) {
    out_error = "Profile is missing a valid watch_region entry.";
    return false;
  }

  out_profile = std::move(profile);
  return true;
}

}  // namespace sekaiemu::spike
