#include "runtime_menu_sync.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <string_view>

namespace sekaiemu::spike {
namespace {

std::string Truncate(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
}

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
  if (value->is_boolean()) {
    return value->get<bool>() ? "YES" : "NO";
  }
  if (value->is_number_integer()) {
    return std::to_string(value->get<long long>());
  }
  if (value->is_number_unsigned()) {
    return std::to_string(value->get<unsigned long long>());
  }
  return {};
}

std::size_t JsonSizeAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  return value != nullptr && value->is_array() ? value->size() : 0;
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
  if (value->is_string()) {
    try {
      return static_cast<unsigned>(std::stoul(value->get<std::string>()));
    } catch (const std::exception&) {
      return fallback;
    }
  }
  return fallback;
}

struct CompletionInfo {
  unsigned checked = 0;
  unsigned total = 0;
  unsigned received = 0;
};

CompletionInfo BuildCompletionInfo(const TrackerRuntime* tracker_runtime) {
  CompletionInfo info;
  if (tracker_runtime == nullptr) {
    return info;
  }
  const auto& state = tracker_runtime->AuthoritativeState();
  const auto& snapshot = state.snapshot;
  if (const auto* summary = JsonAt(snapshot, "summary"); summary != nullptr && summary->is_object()) {
    info.checked = JsonUnsignedAt(*summary, "checked", 0);
    info.total = JsonUnsignedAt(*summary, "total", 0);
    info.received = JsonUnsignedAt(*summary, "received", 0);
  }
  info.checked = std::max<unsigned>(info.checked, static_cast<unsigned>(state.checked_locations.size()));
  const unsigned missing = std::max<unsigned>(static_cast<unsigned>(state.missing_locations.size()),
                                             static_cast<unsigned>(JsonSizeAt(snapshot, "missing_locations")));
  info.total = std::max<unsigned>(info.total, info.checked + missing);
  info.received = std::max<unsigned>(info.received, static_cast<unsigned>(state.received_items.size()));
  return info;
}

std::string SnapshotOrMeta(const nlohmann::json& snapshot, std::string_view key) {
  auto value = JsonStringAt(snapshot, key);
  if (!value.empty()) {
    return value;
  }
  if (const auto* metadata = JsonAt(snapshot, "room_metadata"); metadata != nullptr && metadata->is_object()) {
    value = JsonStringAt(*metadata, key);
  }
  return value;
}

void DrawField(OverlayCanvas& canvas,
               int x,
               int y,
               int width,
               std::string_view label,
               std::string_view value,
               UiColor value_color) {
  canvas.FillRect(x, y, width, 18, UiColor{24, 30, 42, 190});
  canvas.DrawText(x + 8, y + 5, Truncate(label, 20), UiColor{255, 245, 225, 255}, 1);
  canvas.DrawText(x + 174,
                  y + 5,
                  Truncate(value.empty() ? "-" : value, static_cast<std::size_t>(std::max(12, (width - 190) / 6))),
                  value_color,
                  1);
}

}  // namespace

void DrawRuntimeMenuCompletionHeader(OverlayCanvas& canvas,
                                     int x,
                                     int y,
                                     int width,
                                     const TrackerRuntime* tracker_runtime) {
  const auto info = BuildCompletionInfo(tracker_runtime);
  const unsigned percent = info.total == 0 ? 0 : std::min(100u, (info.checked * 100u) / info.total);
  canvas.FillRect(x, y, width, 26, UiColor{10, 14, 22, 180});
  canvas.DrawRect(x, y, width, 26, UiColor{65, 85, 120, 255});
  canvas.DrawText(x + 8,
                  y + 8,
                  "SYNC COMPLETION",
                  UiColor{255, 245, 225, 255},
                  1);
  const int bar_x = x + 150;
  const int bar_y = y + 7;
  const int bar_width = std::max(40, width - 330);
  canvas.FillRect(bar_x, bar_y, bar_width, 10, UiColor{22, 28, 40, 255});
  canvas.FillRect(bar_x, bar_y, (bar_width * static_cast<int>(percent)) / 100, 10, UiColor{90, 210, 140, 255});
  canvas.DrawRect(bar_x, bar_y, bar_width, 10, UiColor{92, 122, 196, 255});
  canvas.DrawText(x + width - 166,
                  y + 8,
                  std::to_string(info.checked) + "/" + std::to_string(info.total) + "  " +
                      std::to_string(percent) + "%",
                  UiColor{180, 205, 255, 255},
                  1);
}

void DrawRuntimeMenuSyncInfoPage(OverlayCanvas& canvas,
                                 int x,
                                 int y,
                                 int width,
                                 int bottom,
                                 const BridgeRuntimeStatus& bridge_status,
                                 const TrackerRuntime* tracker_runtime) {
  const nlohmann::json snapshot = tracker_runtime != nullptr
                                      ? tracker_runtime->AuthoritativeState().snapshot
                                      : nlohmann::json::object();
  const bool connected = SnapshotOrMeta(snapshot, "connected") == "YES" ||
                         SnapshotOrMeta(snapshot, "connected") == "1" ||
                         SnapshotOrMeta(snapshot, "ap_connected") == "YES";
  const UiColor ok{170, 230, 180, 255};
  const UiColor neutral{180, 205, 255, 255};
  const UiColor warning{255, 215, 150, 255};
  const int row_height = 20;
  int row = 0;
  const auto draw = [&](std::string_view label, std::string value, UiColor color) {
    const int field_y = y + row * row_height;
    if (field_y + row_height <= bottom) {
      DrawField(canvas, x, field_y, width, label, value, color);
    }
    ++row;
  };

  std::string endpoint = bridge_status.ap_host;
  if (bridge_status.ap_port != 0) {
    endpoint += ":" + std::to_string(bridge_status.ap_port);
  }
  if (!bridge_status.ap_path.empty() && bridge_status.ap_path != "/") {
    endpoint += bridge_status.ap_path;
  }
  std::string sync_id = SnapshotOrMeta(snapshot, "seed_name");
  if (sync_id.empty()) {
    sync_id = SnapshotOrMeta(snapshot, "seed");
  }
  std::string slot_name = SnapshotOrMeta(snapshot, "slot_name");
  if (slot_name.empty()) {
    slot_name = bridge_status.ap_slot_name;
  }

  draw("SYNC STATUS", connected ? "CONNECTED" : "WAITING", connected ? ok : warning);
  draw("ROOM", endpoint, neutral);
  draw("PLAYER", slot_name, neutral);
  draw("SLOT", SnapshotOrMeta(snapshot, "slot"), neutral);
  draw("TEAM", SnapshotOrMeta(snapshot, "team"), neutral);
  draw("GAME", SnapshotOrMeta(snapshot, "game"), neutral);
  draw("SYNC ID", sync_id, neutral);
  draw("ROOM MODE", SnapshotOrMeta(snapshot, "room_mode"), neutral);
  draw("PLAYERS", SnapshotOrMeta(snapshot, "player_count").empty() ? "1 known"
                                                                    : SnapshotOrMeta(snapshot, "player_count"),
       neutral);
  draw("CHECKS", SnapshotOrMeta(snapshot, "location_name_count"), neutral);
  draw("ITEM NAMES", SnapshotOrMeta(snapshot, "item_name_count"), neutral);
  draw("BRIDGE", bridge_status.owner == BridgeOwner::Sklmi ? "SKLMI" : "LEGACY",
       bridge_status.owner == BridgeOwner::Sklmi ? ok : neutral);
}

}  // namespace sekaiemu::spike
