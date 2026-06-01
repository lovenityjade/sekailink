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

const nlohmann::json* MapChildren(const nlohmann::json& map) {
  if (!map.is_object()) {
    return nullptr;
  }
  for (const char* key : {"menu_children", "menuChildren", "children"}) {
    const auto found = map.find(key);
    if (found != map.end() && found->is_array()) {
      return &*found;
    }
  }
  return nullptr;
}

std::string LabelOrId(const nlohmann::json& value) {
  auto label = JsonStringAtAnyKey(value, {"label", "name", "title"});
  if (!label.empty()) {
    return label;
  }
  label = JsonStringAtAnyKey(value, {"id", "map_id", "mapId", "tab", "tab_id", "tabId"});
  return label.empty() ? "MAP" : label;
}

int EntryDepth(const nlohmann::json& value, int fallback) {
  if (!value.is_object()) {
    return fallback;
  }
  return std::clamp(value.value("menu_depth", value.value("menuDepth", fallback)), 0, 4);
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

bool HasVisibleTab(const TrackerResolvedViewState& resolved, std::string_view tab_id) {
  if (tab_id.empty()) {
    return true;
  }
  return std::any_of(resolved.visible_tabs.begin(), resolved.visible_tabs.end(), [&](const auto& tab) {
    return tab.is_object() && tab.value("id", std::string{}) == tab_id;
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

  const auto& expanded_map_id = runtime.UiState().map_context_menu_expanded_map_id;
  for (std::size_t map_index = 0; map_index < resolved.visible_maps.size(); ++map_index) {
    const auto& map = resolved.visible_maps[map_index];
    if (!map.is_object()) {
      continue;
    }
    const auto map_id = JsonStringAtAnyKey(map, {"id", "map_id", "mapId"});
    const auto* children = MapChildren(map);
    std::vector<TrackerMapContextMenuEntry> child_entries;
    if (children != nullptr) {
      for (std::size_t child_index = 0; child_index < children->size(); ++child_index) {
        const auto& child = (*children)[child_index];
        if (!child.is_object()) {
          continue;
        }
        auto child_map_id = JsonStringAtAnyKey(child, {"map", "map_id", "mapId"});
        if (child_map_id.empty()) {
          child_map_id = map_id;
        }
        const auto child_tab_id = JsonStringAtAnyKey(child, {"tab", "tab_id", "tabId"});
        if (!HasVisibleMap(resolved, child_map_id) || !HasVisibleTab(resolved, child_tab_id)) {
          continue;
        }
        child_entries.push_back(TrackerMapContextMenuEntry{
            .kind = TrackerMapContextMenuEntryKind::Child,
            .map_index = map_index,
            .child_index = child_index,
            .label = LabelOrId(child),
            .map_id = child_map_id,
            .tab_id = child_tab_id,
            .depth = EntryDepth(child, EntryDepth(map, 0) + 1),
        });
      }
    }
    const bool expandable = !child_entries.empty();
    const bool expanded = expandable && map_id == expanded_map_id;
    entries.push_back(TrackerMapContextMenuEntry{
        .kind = TrackerMapContextMenuEntryKind::Map,
        .map_index = map_index,
        .label = LabelOrId(map),
        .map_id = map_id,
        .depth = EntryDepth(map, 0),
        .expandable = expandable,
        .expanded = expanded,
    });

    if (!expanded) {
      continue;
    }
    entries.insert(entries.end(), child_entries.begin(), child_entries.end());
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
