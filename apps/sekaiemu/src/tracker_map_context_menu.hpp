#pragma once

#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace sekaiemu::spike {

enum class TrackerMapContextMenuEntryKind {
  ActualMap,
  PreviousMap,
  Map,
  Child,
};

struct TrackerMapContextMenuEntry {
  TrackerMapContextMenuEntryKind kind = TrackerMapContextMenuEntryKind::Map;
  std::size_t map_index = 0;
  std::size_t child_index = 0;
  std::string label;
  std::string map_id;
  std::string tab_id;
  int depth = 0;
  bool expandable = false;
  bool expanded = false;
};

struct TrackerMapContextMenuMetrics {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int row_height = 18;
  int rows = 0;
};

std::vector<TrackerMapContextMenuEntry> BuildTrackerMapContextMenuEntries(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved);

TrackerMapContextMenuMetrics BuildTrackerMapContextMenuMetrics(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y);

std::optional<std::size_t> HitTestTrackerMapContextMenu(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y,
    int point_x,
    int point_y);

}  // namespace sekaiemu::spike
