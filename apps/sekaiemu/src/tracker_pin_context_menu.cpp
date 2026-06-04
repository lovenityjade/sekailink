#include "tracker_pin_context_menu.hpp"

#include "tracker_overlay_render_state.hpp"

#include <algorithm>

namespace sekaiemu::spike {
namespace {

int ClampMenuCoordinate(int value, int low, int high) {
  if (high < low) {
    return low;
  }
  return std::clamp(value, low, high);
}

void AppendUniqueEntry(std::vector<TrackerPinContextMenuEntry>& entries,
                       TrackerPinContextMenuEntry entry) {
  if (entry.location_id.empty()) {
    return;
  }
  const auto duplicate = std::any_of(entries.begin(), entries.end(), [&](const auto& existing) {
    return existing.location_id == entry.location_id;
  });
  if (!duplicate) {
    entries.push_back(std::move(entry));
  }
}

}  // namespace

std::vector<TrackerPinContextMenuEntry> BuildTrackerPinContextMenuEntries(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved) {
  const auto& ui = runtime.UiState();
  if (!ui.pin_context_menu_visible || ui.pin_context_menu_pin_id.empty()) {
    return {};
  }

  std::vector<TrackerPinContextMenuEntry> entries;
  for (const auto& pin : BuildBundlePins(runtime, resolved)) {
    if (pin.id != ui.pin_context_menu_pin_id) {
      continue;
    }
    for (const auto& segment : pin.segments) {
      for (const auto& check : segment.checks) {
        AppendUniqueEntry(entries,
                          TrackerPinContextMenuEntry{
                              .location_id = check.location_id,
                              .label = check.label.empty() ? segment.label : check.label,
                              .color = segment.color.empty() ? pin.color : segment.color,
                              .checked = check.checked,
                          });
      }
    }
    if (entries.empty() && !pin.location_id.empty()) {
      AppendUniqueEntry(entries,
                        TrackerPinContextMenuEntry{
                            .location_id = pin.location_id,
                            .label = pin.label.empty() ? pin.id : pin.label,
                            .color = pin.color,
                            .checked = pin.checked,
                        });
    }
    break;
  }
  return entries;
}

TrackerPinContextMenuMetrics BuildTrackerPinContextMenuMetrics(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y) {
  TrackerPinContextMenuMetrics metrics;
  const auto entries = BuildTrackerPinContextMenuEntries(runtime, resolved);
  metrics.width = std::min(360, std::max(220, layout.width - 28));
  const int max_rows =
      std::max(3, (layout.height - (body_y - layout.y) - 12) / metrics.row_height);
  metrics.rows = std::max(1, std::min(max_rows, static_cast<int>(entries.size())));
  metrics.height = metrics.rows * metrics.row_height + 8;
  metrics.x = ClampMenuCoordinate(runtime.UiState().pin_context_menu_x,
                                  layout.x + 4,
                                  layout.x + layout.width - metrics.width - 4);
  metrics.y = ClampMenuCoordinate(runtime.UiState().pin_context_menu_y,
                                  body_y + 4,
                                  layout.y + layout.height - metrics.height - 4);
  return metrics;
}

std::optional<std::size_t> HitTestTrackerPinContextMenu(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    const TrackerPanelLayout& layout,
    int body_y,
    int point_x,
    int point_y) {
  const auto metrics = BuildTrackerPinContextMenuMetrics(runtime, resolved, layout, body_y);
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
