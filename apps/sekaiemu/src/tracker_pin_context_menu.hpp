#pragma once

#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct TrackerPinContextMenuEntry {
  std::string location_id;
  std::string label;
  std::string color;
  bool checked = false;
};

struct TrackerPinContextMenuMetrics {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int row_height = 18;
  int rows = 0;
};

std::vector<TrackerPinContextMenuEntry> BuildTrackerPinContextMenuEntries(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved);

TrackerPinContextMenuMetrics BuildTrackerPinContextMenuMetrics(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y);

std::optional<std::size_t> HitTestTrackerPinContextMenu(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y,
    int point_x,
    int point_y);

}  // namespace sekaiemu::spike
