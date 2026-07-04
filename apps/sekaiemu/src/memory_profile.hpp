#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct WatchRegion {
  std::string memory_domain;
  std::uint32_t start = 0;
  std::uint32_t length = 0;
};

struct CheckEntry {
  std::uint32_t location_id = 0;
  std::uint32_t byte_index = 0;
  std::uint8_t bit_index = 0;
  std::string name;
};

struct OotCheckEntry {
  std::uint32_t location_id = 0;
  std::uint32_t scene = 0;
  std::uint8_t bit_index = 0;
  std::string name;
};

struct FlagCheckEntry {
  std::uint32_t location_id = 0;
  std::uint32_t flag_id = 0;
  std::string name;
};

struct ReceiveSlot {
  std::string kind;
  std::string memory_domain;
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
};

struct ItemAlias {
  std::string kind;
  std::uint32_t code = 0;
  std::string name;
};

struct MemoryProfile {
  std::string game;
  WatchRegion watch_region;
  std::vector<CheckEntry> checks;
  std::vector<OotCheckEntry> oot_chest_checks;
  std::vector<FlagCheckEntry> flag_checks;
  std::vector<ReceiveSlot> receive_slots;
  std::vector<ItemAlias> item_aliases;
};

bool LoadMemoryProfile(const std::filesystem::path& path,
                       MemoryProfile& out_profile,
                       std::string& out_error);

}  // namespace sekaiemu::spike
