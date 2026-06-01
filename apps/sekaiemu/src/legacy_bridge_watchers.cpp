#include "legacy_bridge_watchers.hpp"

#include "libretro_core_utils.hpp"

#include <optional>
#include <sstream>

namespace sekaiemu::spike {

namespace {

void TickOotChecks(const LegacyBridgeWatcherContext& context) {
  if (context.profile.oot_chest_checks.empty()) {
    return;
  }

  auto read_region = [&](std::uint32_t address, std::uint32_t length) -> std::vector<std::uint8_t> {
    for (const char* domain : {"n64_system_bus", "system_ram"}) {
      const WatchRegion region{domain, address, length};
      const auto bytes = context.read_profile_region(region);
      if (bytes.size() == length) {
        return bytes;
      }
    }
    return {};
  };

  auto read_be_u32 = [&](std::uint32_t address, std::uint32_t& out_value) -> bool {
    const auto bytes = read_region(address, 4);
    if (bytes.size() != 4) {
      return false;
    }
    out_value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                (static_cast<std::uint32_t>(bytes[1]) << 16) |
                (static_cast<std::uint32_t>(bytes[2]) << 8) |
                static_cast<std::uint32_t>(bytes[3]);
    return true;
  };

  const auto temp_context = read_region(0x40002Cu, 4);

  auto chest_checked = [&](const OotCheckEntry& check) -> std::optional<bool> {
    constexpr std::uint32_t kSaveContextOffset = 0x11A5D0u;
    constexpr std::uint32_t kSceneFlagsOffset = kSaveContextOffset + 0xD4u;
    const std::uint32_t scene_address = kSceneFlagsOffset + (0x1Cu * check.scene);
    std::uint32_t permanent_flags = 0;
    if (!read_be_u32(scene_address, permanent_flags)) {
      return std::nullopt;
    }
    const bool permanent = (permanent_flags & (1u << check.bit_index)) != 0;
    bool temporary = false;
    if (temp_context.size() == 4) {
      temporary = temp_context[0] == static_cast<std::uint8_t>(check.scene & 0xFFu) &&
                  temp_context[1] == 0x01u &&
                  temp_context[3] == check.bit_index;
    }
    return permanent || temporary;
  };

  if (context.oot_check_snapshot.size() != context.profile.oot_chest_checks.size()) {
    context.oot_check_snapshot.assign(context.profile.oot_chest_checks.size(), false);
    bool all_readable = true;
    for (std::size_t i = 0; i < context.profile.oot_chest_checks.size(); ++i) {
      const auto state = chest_checked(context.profile.oot_chest_checks[i]);
      if (!state.has_value()) {
        all_readable = false;
        break;
      }
      context.oot_check_snapshot[i] = *state;
    }
    if (!all_readable) {
      context.oot_check_snapshot.clear();
    }
    return;
  }

  for (std::size_t i = 0; i < context.profile.oot_chest_checks.size(); ++i) {
    const auto& check = context.profile.oot_chest_checks[i];
    const auto state = chest_checked(check);
    if (!state.has_value()) {
      continue;
    }
    const bool old_state = context.oot_check_snapshot[i];
    const bool new_state = *state;
    if (!old_state && new_state) {
      std::ostringstream event;
      event << "location|0x" << std::hex << std::uppercase << check.location_id
            << std::dec << "|" << check.name
            << "|scene=0x" << std::hex << std::uppercase << check.scene
            << std::dec << "|bit=" << static_cast<unsigned>(check.bit_index);
      context.append_log(event.str());
      if (context.callbacks.trace) {
        std::ostringstream trace;
        trace << "[sekaiemu] OOT location triggered: " << check.name
              << " (0x" << std::hex << std::uppercase << check.location_id << std::dec << ")";
        context.callbacks.trace(trace.str());
      }
    }
    context.oot_check_snapshot[i] = new_state;
  }
}

void TickFireRedDynamicFlagChecks(const LegacyBridgeWatcherContext& context) {
  if (context.profile.flag_checks.empty()) {
    return;
  }
  if (Lowercase(context.profile.game) != "pokemon firered") {
    return;
  }

  const WatchRegion main_state_region{"gba_system_bus", 0x03003078, 1};
  const auto overworld_state = context.read_profile_region(main_state_region);
  if (overworld_state.empty() || overworld_state[0] != 1) {
    return;
  }

  const WatchRegion sb1_ptr_region{"gba_system_bus", 0x03004F58, 4};
  const auto sb1_ptr_bytes = context.read_profile_region(sb1_ptr_region);
  if (sb1_ptr_bytes.size() != 4) {
    return;
  }

  const std::uint32_t sb1_address =
      static_cast<std::uint32_t>(sb1_ptr_bytes[0]) |
      (static_cast<std::uint32_t>(sb1_ptr_bytes[1]) << 8) |
      (static_cast<std::uint32_t>(sb1_ptr_bytes[2]) << 16) |
      (static_cast<std::uint32_t>(sb1_ptr_bytes[3]) << 24);
  if (sb1_address < 0x02000000u || sb1_address >= 0x02040000u) {
    return;
  }

  const WatchRegion flag_region{"gba_system_bus", sb1_address + 0x1130u, 0x120u};
  const auto flag_bytes = context.read_profile_region(flag_region);
  if (flag_bytes.size() != flag_region.length) {
    return;
  }

  if (context.flag_check_snapshot.size() != context.profile.flag_checks.size()) {
    context.flag_check_snapshot.assign(context.profile.flag_checks.size(), false);
    for (std::size_t i = 0; i < context.profile.flag_checks.size(); ++i) {
      const auto& check = context.profile.flag_checks[i];
      const std::size_t byte_index = check.flag_id / 8u;
      const std::uint8_t mask = static_cast<std::uint8_t>(1u << (check.flag_id % 8u));
      if (byte_index < flag_bytes.size()) {
        context.flag_check_snapshot[i] = (flag_bytes[byte_index] & mask) != 0;
      }
    }
    return;
  }

  for (std::size_t i = 0; i < context.profile.flag_checks.size(); ++i) {
    const auto& check = context.profile.flag_checks[i];
    const std::size_t byte_index = check.flag_id / 8u;
    const std::uint8_t bit_index = static_cast<std::uint8_t>(check.flag_id % 8u);
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit_index);
    if (byte_index >= flag_bytes.size()) {
      continue;
    }
    const bool new_state = (flag_bytes[byte_index] & mask) != 0;
    const bool old_state = context.flag_check_snapshot[i];
    if (!old_state && new_state) {
      std::ostringstream event;
      event << "location|0x" << std::hex << std::uppercase << check.location_id
            << std::dec << "|" << check.name
            << "|flag=" << check.flag_id
            << "|byte=" << byte_index
            << "|bit=" << static_cast<unsigned>(bit_index);
      context.append_log(event.str());
      if (context.callbacks.trace) {
        std::ostringstream trace;
        trace << "[sekaiemu] location triggered: " << check.name
              << " (0x" << std::hex << std::uppercase << check.location_id << std::dec << ")";
        context.callbacks.trace(trace.str());
      }
    }
    context.flag_check_snapshot[i] = new_state;
  }
}

}  // namespace

void TickLegacyBridgeWatchers(const LegacyBridgeWatcherContext& context) {
  if (Lowercase(context.profile.game) == "ocarina of time") {
    TickOotChecks(context);
    return;
  }

  TickFireRedDynamicFlagChecks(context);
}

}  // namespace sekaiemu::spike
