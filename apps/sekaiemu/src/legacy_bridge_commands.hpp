#pragma once

#include "bridge_adapters/adapter_context.hpp"
#include "bridge_ipc.hpp"
#include "memory_profile.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

struct LegacyBridgeCommandContext {
  const MemoryProfile& profile;
  std::function<const ReceiveSlot*(std::string_view)> find_receive_slot;
  std::function<std::vector<std::uint8_t>(const WatchRegion&)> read_region;
  std::function<std::uint8_t*(std::string_view, std::uint32_t, std::uint32_t)> resolve_mutable;
  std::function<std::optional<std::uint32_t>(std::string_view, std::string_view)>
      resolve_injection_code;
  std::function<bool(const ReceiveSlot&)> read_slot_busy;
  std::function<bool(const ReceiveSlot&, std::uint32_t)> write_slot_value;
  std::function<std::optional<std::uint32_t>(const ReceiveSlot&)> read_slot_value;
  std::function<void(const std::string&)> append_log;
  std::function<void(const std::string&)> trace;
};

void ProcessLegacyBridgeCommands(BridgeIpc& bridge_ipc,
                                 const LegacyBridgeCommandContext& context);

}  // namespace sekaiemu::spike
