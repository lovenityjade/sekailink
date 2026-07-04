#pragma once

#include "../memory_profile.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike::bridge_adapters {

struct AdapterContext {
  std::function<const ReceiveSlot*(std::string_view)> find_receive_slot;
  std::function<std::vector<std::uint8_t>(const WatchRegion&)> read_region;
  std::function<std::uint8_t*(std::string_view, std::uint32_t, std::uint32_t)> resolve_mutable;
  std::function<bool(const ReceiveSlot&)> read_slot_busy;
  std::function<bool(const ReceiveSlot&, std::uint32_t)> write_slot_value;
  std::function<std::optional<std::uint32_t>(const ReceiveSlot&)> read_slot_value;
  std::function<void(const std::string&)> append_log;
  std::function<void(const std::string&)> trace;
};

}  // namespace sekaiemu::spike::bridge_adapters
