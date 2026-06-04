#include "tracker_map_context_menu.hpp"

#include <algorithm>
#include <initializer_list>

namespace sekaiemu::spike {
namespace {

std::string JsonStringAtAnyKey(const nlohmann::json& root,
                               std::initializer_list<const char*> keys) {
  if (!root.is_object()) {
    return {};
  }
  for (const char* key : keys) {
    const auto found = root.find(key);
    if (found != root.end() && found->is_string()) {
      return found->get<std::string>();
    }
  }
  return {};
}

std::string LabelOrId(const nlohmann::json& value) {
  auto label = JsonStringAtAnyKey(value, {"label", "name", "title"});
  if (!label.empty()) {
    return label;
  }
  label = JsonStringAtAnyKey(value, {"id", "map_id", "mapId", "tab", "tab_id", "tabId"});
  return label.empty() ? "MAP" : label;
}

int ClampMenuCoordinate(int value, int low, int high) {
  if (high < low) {
    return low;
  }
  return std::clamp(value, low, high);
}

bool HasVisibleMap(const TrackerResolvedViewState& resolved, std::string_view map_id) {
  if (map_id.empty()) {
    return false;
  }
  return std::any_of(resolved.visible_maps.begin(), resolved.visible_maps.end(), [&](const auto& map) {
    return map.is_object() && map.value("id", std::string{}) == map_id;
  });
}

std::size_t VisibleMapIndex(const TrackerResolvedViewState& resolved, std::string_view map_id) {
  for (std::size_t index = 0; index < resolved.visible_maps.size(); ++index) {
    const auto& map = resolved.visible_maps[index];
    if (map.is_object() && map.value("id", std::string{}) == map_id) {
      return index;
    }
  }
  return 0;
}

bool MapHasDrawableSurface(const TrackerResolvedViewState& resolved, std::string_view map_id) {
  if (map_id.empty()) {
    return false;
  }
  for (const auto& map : resolved.visible_maps) {
    if (!map.is_object() || map.value("id", std::string{}) != map_id) {
      continue;
    }
    return !JsonStringAtAnyKey(map, {"image"}).empty() ||
           map.value("image_loaded", false);
  }
  return false;
}

std::string LabelForMapOrTab(const TrackerResolvedViewState& resolved,
                             std::string_view map_id,
                             std::string_view tab_id) {
  for (const auto& map : resolved.visible_maps) {
    if (map.is_object() && map.value("id", std::string{}) == map_id) {
      return LabelOrId(map);
    }
  }
  if (!tab_id.empty()) {
    for (const auto& tab : resolved.visible_tabs) {
      if (tab.is_object() && tab.value("id", std::string{}) == tab_id) {
        return LabelOrId(tab);
      }
    }
  }
  return {};
}

bool EntriesContainMap(const std::vector<TrackerMapContextMenuEntry>& entries,
                       std::string_view map_id) {
  return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
    return entry.map_id == map_id;
  });
}

}  // namespace

std::vector<TrackerMapContextMenuEntry> BuildTrackerMapContextMenuEntries(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved) {
  std::vector<TrackerMapContextMenuEntry> entries;
  entries.push_back(TrackerMapContextMenuEntry{
      .kind = TrackerMapContextMenuEntryKind::ActualMap,
      .label = "ACTUAL MAP",
  });

  const auto& ui = runtime.UiState();
  if (HasVisibleMap(resolved, ui.previous_map_id) &&
      MapHasDrawableSurface(resolved, ui.previous_map_id)) {
    auto label = LabelForMapOrTab(resolved, ui.previous_map_id, ui.previous_tab_id);
    entries.push_back(TrackerMapContextMenuEntry{
        .kind = TrackerMapContextMenuEntryKind::PreviousMap,
        .label = label.empty() ? "PREVIOUS MAP" : "BACK: " + label,
        .map_id = ui.previous_map_id,
        .tab_id = ui.previous_tab_id,
    });
  }

  for (const auto& tab : resolved.visible_tabs) {
    if (!tab.is_object()) {
      continue;
    }
    const auto tab_id = JsonStringAtAnyKey(tab, {"id", "tab", "tab_id", "tabId"});
    const auto map_id = JsonStringAtAnyKey(tab, {"map_id", "mapId", "map"});
    if (!HasVisibleMap(resolved, map_id) || !MapHasDrawableSurface(resolved, map_id)) {
      continue;
    }
    if (EntriesContainMap(entries, map_id)) {
      continue;
    }
    auto label = LabelForMapOrTab(resolved, map_id, tab_id);
    entries.push_back(TrackerMapContextMenuEntry{
        .kind = TrackerMapContextMenuEntryKind::Map,
        .map_index = VisibleMapIndex(resolved, map_id),
        .label = label.empty() ? LabelOrId(tab) : label,
        .map_id = map_id,
        .tab_id = tab_id,
    });
  }
  return entries;
}

TrackerMapContextMenuMetrics BuildTrackerMapContextMenuMetrics(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y) {
  TrackerMapContextMenuMetrics metrics;
  const auto entries = BuildTrackerMapContextMenuEntries(runtime, resolved);
  metrics.width = std::min(280, std::max(190, layout.width - 28));
  const int max_rows =
      std::max(3, (layout.height - (body_y - layout.y) - 12) / metrics.row_height);
  metrics.rows = std::max(1, std::min(max_rows, static_cast<int>(entries.size())));
  metrics.height = metrics.rows * metrics.row_height + 8;
  metrics.x = ClampMenuCoordinate(runtime.UiState().map_context_menu_x,
                                  layout.x + 4,
                                  layout.x + layout.width - metrics.width - 4);
  metrics.y = ClampMenuCoordinate(runtime.UiState().map_context_menu_y,
                                  body_y + 4,
                                  layout.y + layout.height - metrics.height - 4);
  return metrics;
}

std::optional<std::size_t> HitTestTrackerMapContextMenu(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y,
    int point_x,
    int point_y) {
  const auto metrics = BuildTrackerMapContextMenuMetrics(runtime, resolved, layout, body_y);
  if (point_x < metrics.x || point_x >= metrics.x + metrics.width ||
      point_y < metrics.y || point_y >= metrics.y + metrics.height) {
    return std::nullopt;
  }
  const int row = (point_y - metrics.y - 4) / metrics.row_height;
  if (row < 0 || row >= metrics.rows) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(row);
}

}  // namespace sekaiemu::spike
