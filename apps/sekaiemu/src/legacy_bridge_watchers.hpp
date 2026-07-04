#pragma once

#include "memory_profile.hpp"
#include "profile_bridge.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct LegacyBridgeWatcherContext {
  const MemoryProfile& profile;
  const ProfileBridgeCallbacks& callbacks;
  std::vector<bool>& flag_check_snapshot;
  std::vector<bool>& oot_check_snapshot;
  std::function<std::vector<std::uint8_t>(const WatchRegion&)> read_profile_region;
  std::function<void(const std::string&)> append_log;
};

void TickLegacyBridgeWatchers(const LegacyBridgeWatcherContext& context);

}  // namespace sekaiemu::spike
