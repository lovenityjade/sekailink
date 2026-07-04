#include "profile_bridge.hpp"

#include "legacy_bridge_commands.hpp"
#include "legacy_bridge_watchers.hpp"
#include "libretro_core_utils.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>

namespace sekaiemu::spike {

namespace {

bool ParseProfileLooseUnsigned(std::string_view text, std::uint32_t& out_value) {
  try {
    const auto lowered = Lowercase(text);
    std::size_t parsed = 0;
    unsigned long long value = 0;
    if (lowered.starts_with("0x")) {
      value = std::stoull(std::string(lowered), &parsed, 16);
    } else {
      value = std::stoull(std::string(text), &parsed, 10);
    }
    if (parsed != std::string(text).size() || value > 0xFFFFFFFFull) {
      return false;
    }
    out_value = static_cast<std::uint32_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

bool ProfileBridge::Initialize(const std::filesystem::path& profile_path,
                               const std::filesystem::path& save_directory,
                               std::string& error) {
  if (profile_path.empty()) {
    return true;
  }

  MemoryProfile loaded_profile;
  if (!LoadMemoryProfile(profile_path, loaded_profile, error)) {
    error = "Failed to load profile: " + error;
    return false;
  }

  const auto bridge_root = save_directory / "bridge";
  if (!bridge_ipc_.Initialize(bridge_root, error)) {
    return false;
  }

  profile_ = std::move(loaded_profile);
  AppendBridgeLog("bridge_started|game=" + profile_->game + "|profile=" + profile_path.string());
  return true;
}

void ProfileBridge::Tick(const ProfileBridgeCallbacks& callbacks) {
  if (!profile_) {
    return;
  }

  ProcessIncomingCommands(callbacks);

  if (Lowercase(profile_->game) == "ocarina of time") {
    TickLegacyBridgeWatchers(LegacyBridgeWatcherContext{
        .profile = *profile_,
        .callbacks = callbacks,
        .flag_check_snapshot = flag_check_snapshot_,
        .oot_check_snapshot = oot_check_snapshot_,
        .read_profile_region = [this, &callbacks](const WatchRegion& region) {
          return ReadProfileRegion(callbacks, region);
        },
        .append_log = [this](const std::string& line) { AppendBridgeLog(line); },
    });
    return;
  }

  if (watched_region_snapshot_.empty()) {
    watched_region_snapshot_ = ReadProfileRegion(callbacks, profile_->watch_region);
    if (watched_region_snapshot_.empty()) {
      return;
    }
    if (callbacks.trace) {
      callbacks.trace("[sekaiemu] profile watch region resolved for " + profile_->game +
                      " size=" + std::to_string(watched_region_snapshot_.size()));
    }
  }

  const auto current_snapshot = ReadProfileRegion(callbacks, profile_->watch_region);
  if (!current_snapshot.empty() && current_snapshot.size() == watched_region_snapshot_.size()) {
    for (const auto& check : profile_->checks) {
      if (check.byte_index >= current_snapshot.size()) {
        continue;
      }
      const std::uint8_t mask = static_cast<std::uint8_t>(1u << check.bit_index);
      const bool old_state = (watched_region_snapshot_[check.byte_index] & mask) != 0;
      const bool new_state = (current_snapshot[check.byte_index] & mask) != 0;
      if (!old_state && new_state) {
        std::ostringstream event;
        event << "location|0x" << std::hex << std::uppercase << check.location_id
              << std::dec << "|" << check.name
              << "|byte=" << check.byte_index
              << "|bit=" << static_cast<unsigned>(check.bit_index);
        AppendBridgeLog(event.str());
        if (callbacks.trace) {
          std::ostringstream trace;
          trace << "[sekaiemu] location triggered: " << check.name
                << " (0x" << std::hex << std::uppercase << check.location_id << std::dec << ")";
          callbacks.trace(trace.str());
        }
        if (callbacks.location_triggered) {
          callbacks.location_triggered(check.location_id, check.name);
        }
      }
    }
    watched_region_snapshot_ = current_snapshot;
  }

  TickLegacyBridgeWatchers(LegacyBridgeWatcherContext{
      .profile = *profile_,
      .callbacks = callbacks,
      .flag_check_snapshot = flag_check_snapshot_,
      .oot_check_snapshot = oot_check_snapshot_,
      .read_profile_region = [this, &callbacks](const WatchRegion& region) {
        return ReadProfileRegion(callbacks, region);
      },
      .append_log = [this](const std::string& line) { AppendBridgeLog(line); },
  });
}

bool ProfileBridge::Active() const {
  return profile_.has_value();
}

const MemoryProfile* ProfileBridge::Profile() const {
  return profile_ ? &*profile_ : nullptr;
}

const std::filesystem::path& ProfileBridge::SocketPath() const {
  return bridge_ipc_.SocketPath();
}

std::vector<std::uint8_t> ProfileBridge::ReadProfileRegion(const ProfileBridgeCallbacks& callbacks,
                                                           const WatchRegion& region) const {
  return callbacks.read_region ? callbacks.read_region(region) : std::vector<std::uint8_t>{};
}

const ReceiveSlot* ProfileBridge::FindReceiveSlot(std::string_view kind) const {
  if (!profile_) {
    return nullptr;
  }
  const auto wanted = Lowercase(kind);
  for (const auto& slot : profile_->receive_slots) {
    if (Lowercase(slot.kind) == wanted) {
      return &slot;
    }
  }
  return nullptr;
}

const ItemAlias* ProfileBridge::FindItemAlias(std::string_view kind,
                                              std::string_view alias_name) const {
  if (!profile_) {
    return nullptr;
  }
  const auto wanted_kind = Lowercase(kind);
  const auto wanted_name = Lowercase(alias_name);
  for (const auto& alias : profile_->item_aliases) {
    if (Lowercase(alias.kind) == wanted_kind && Lowercase(alias.name) == wanted_name) {
      return &alias;
    }
  }
  return nullptr;
}

bool ProfileBridge::ReadSlotBusy(const ProfileBridgeCallbacks& callbacks, const ReceiveSlot& slot) const {
  const WatchRegion region{slot.memory_domain, slot.offset, slot.size};
  const auto bytes = ReadProfileRegion(callbacks, region);
  if (bytes.empty()) {
    return true;
  }
  return std::any_of(bytes.begin(), bytes.end(), [](std::uint8_t value) { return value != 0; });
}

bool ProfileBridge::WriteSlotValue(const ProfileBridgeCallbacks& callbacks,
                                   const ReceiveSlot& slot,
                                   std::uint32_t value) const {
  auto* base = callbacks.resolve_mutable
                   ? callbacks.resolve_mutable(slot.memory_domain, slot.offset, slot.size)
                   : nullptr;
  if (!base) {
    return false;
  }
  for (std::uint32_t index = 0; index < slot.size; ++index) {
    base[index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFu);
  }
  return true;
}

std::optional<std::uint32_t> ProfileBridge::ReadSlotValue(const ProfileBridgeCallbacks& callbacks,
                                                          const ReceiveSlot& slot) const {
  const WatchRegion region{slot.memory_domain, slot.offset, slot.size};
  const auto bytes = ReadProfileRegion(callbacks, region);
  if (bytes.empty()) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < bytes.size() && index < sizeof(value); ++index) {
    value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
  }
  return value;
}

std::optional<std::uint32_t> ProfileBridge::ResolveInjectionCode(std::string_view kind,
                                                                 std::string_view raw_value) const {
  std::uint32_t numeric_value = 0;
  if (ParseProfileLooseUnsigned(raw_value, numeric_value)) {
    return numeric_value;
  }
  if (const auto* alias = FindItemAlias(kind, raw_value)) {
    return alias->code;
  }
  return std::nullopt;
}

void ProfileBridge::AppendBridgeLog(const std::string& line) {
  bridge_ipc_.PublishEvent(line);
}

void ProfileBridge::ProcessIncomingCommands(const ProfileBridgeCallbacks& callbacks) {
  if (!profile_) {
    return;
  }
  ProcessLegacyBridgeCommands(
      bridge_ipc_,
      LegacyBridgeCommandContext{
          .profile = *profile_,
          .find_receive_slot = [this](std::string_view kind) { return FindReceiveSlot(kind); },
          .read_region = [this, &callbacks](const WatchRegion& region) {
            return ReadProfileRegion(callbacks, region);
          },
          .resolve_mutable = [&callbacks](std::string_view domain, std::uint32_t start, std::uint32_t length) {
            return callbacks.resolve_mutable ? callbacks.resolve_mutable(domain, start, length) : nullptr;
          },
          .resolve_injection_code = [this](std::string_view kind, std::string_view raw_value) {
            return ResolveInjectionCode(kind, raw_value);
          },
          .read_slot_busy = [this, &callbacks](const ReceiveSlot& slot) {
            return ReadSlotBusy(callbacks, slot);
          },
          .write_slot_value = [this, &callbacks](const ReceiveSlot& slot, std::uint32_t value) {
            return WriteSlotValue(callbacks, slot, value);
          },
          .read_slot_value = [this, &callbacks](const ReceiveSlot& slot) {
            return ReadSlotValue(callbacks, slot);
          },
          .append_log = [this](const std::string& line) { AppendBridgeLog(line); },
          .trace = [&callbacks](const std::string& line) {
            if (callbacks.trace) {
              callbacks.trace(line);
            }
          },
      });
}

}  // namespace sekaiemu::spike
