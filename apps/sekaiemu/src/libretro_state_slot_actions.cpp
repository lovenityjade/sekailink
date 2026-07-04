#include "libretro_state_slot_actions.hpp"

#include "tracker_overlay_render_state.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace sekaiemu::spike {
namespace {

const nlohmann::json* JsonAt(const nlohmann::json& root, std::string_view key) {
  if (!root.is_object()) {
    return nullptr;
  }
  const auto found = root.find(std::string(key));
  return found == root.end() ? nullptr : &*found;
}

std::string JsonStringAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr) {
    return {};
  }
  if (value->is_string()) {
    return value->get<std::string>();
  }
  if (value->is_number_integer()) {
    return std::to_string(value->get<long long>());
  }
  if (value->is_number_unsigned()) {
    return std::to_string(value->get<unsigned long long>());
  }
  if (value->is_boolean()) {
    return value->get<bool>() ? "yes" : "no";
  }
  return {};
}

unsigned JsonUnsignedAt(const nlohmann::json& root, std::string_view key, unsigned fallback) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr) {
    return fallback;
  }
  if (value->is_number_unsigned()) {
    return static_cast<unsigned>(value->get<unsigned long long>());
  }
  if (value->is_number_integer()) {
    return static_cast<unsigned>(std::max<long long>(0, value->get<long long>()));
  }
  return fallback;
}

std::string SlotLabel(int slot) {
  return slot <= 0 ? "Autosave" : "Slot " + std::to_string(slot);
}

std::string LocalTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &time);
#else
  localtime_r(&time, &local_time);
#endif
  std::ostringstream stream;
  stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

nlohmann::json BuildStateSlotMetadata(const TrackerRuntime& tracker_runtime,
                                      std::string_view game_name,
                                      std::uint64_t frame_counter,
                                      int slot) {
  const auto& state = tracker_runtime.AuthoritativeState();
  const auto& snapshot = state.snapshot;
  unsigned checked = static_cast<unsigned>(state.checked_locations.size());
  unsigned total = checked + static_cast<unsigned>(state.missing_locations.size());
  unsigned received = static_cast<unsigned>(state.received_items.size());
  if (const auto* summary = JsonAt(snapshot, "summary"); summary != nullptr && summary->is_object()) {
    checked = std::max(checked, JsonUnsignedAt(*summary, "checked", 0));
    total = std::max(total, JsonUnsignedAt(*summary, "total", 0));
    received = std::max(received, JsonUnsignedAt(*summary, "received", 0));
  }
  const unsigned percent = total == 0 ? 0 : std::min(100u, checked * 100u / total);
  return {
      {"format_version", "1"},
      {"slot_index", slot},
      {"slot_kind", slot <= 0 ? "autosave" : "manual"},
      {"slot_label", SlotLabel(slot)},
      {"created_at_local", LocalTimestamp()},
      {"frame", frame_counter},
      {"game", std::string(game_name)},
      {"sync",
       {
           {"seed_id", state.seed_id},
           {"seed_name", JsonStringAt(snapshot, "seed_name")},
           {"slot_id", state.slot_id},
           {"slot_name", JsonStringAt(snapshot, "slot_name")},
           {"username", SnapshotStringAt(tracker_runtime, "username").empty()
                            ? SnapshotStringAt(tracker_runtime, "room_metadata.username")
                            : SnapshotStringAt(tracker_runtime, "username")},
           {"player_alias", SnapshotStringAt(tracker_runtime, "player_alias").empty()
                                ? SnapshotStringAt(tracker_runtime, "room_metadata.player_alias")
                                : SnapshotStringAt(tracker_runtime, "player_alias")},
           {"player_display_name", SnapshotDisplayPlayerName(tracker_runtime)},
           {"room_name", JsonStringAt(snapshot, "room_name")},
           {"game", JsonStringAt(snapshot, "game")},
           {"checked_locations", checked},
           {"total_locations", total},
           {"completion_percent", percent},
           {"received_items", received},
       }},
  };
}

}  // namespace

bool SaveStateSlotNow(SaveStateManager& save_state_manager,
                      CoreApi& core,
                      const TrackerRuntime& tracker_runtime,
                      std::string_view game_name,
                      std::uint64_t frame_counter,
                      int slot,
                      std::optional<int>& pending_screenshot_slot,
                      nlohmann::json& pending_metadata) {
  std::string error;
  if (!save_state_manager.SaveState(core, error, slot)) {
    std::cerr << "[sekaiemu] state save failed: " << error << "\n";
    return false;
  }
  pending_screenshot_slot = slot;
  pending_metadata = BuildStateSlotMetadata(tracker_runtime, game_name, frame_counter, slot);
  std::cerr << "[sekaiemu] state saved: " << save_state_manager.StatePath(slot) << "\n";
  return true;
}

bool LoadStateSlotNow(SaveStateManager& save_state_manager, CoreApi& core, int slot) {
  std::string error;
  if (!save_state_manager.LoadState(core, error, slot)) {
    std::cerr << "[sekaiemu] state load failed: " << error << "\n";
    return false;
  }
  save_state_manager.RefreshBatteryTracking(core);
  std::cerr << "[sekaiemu] state loaded: " << save_state_manager.StatePath(slot) << "\n";
  return true;
}

}  // namespace sekaiemu::spike
