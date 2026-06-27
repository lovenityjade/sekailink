#include "tracker_overlay_renderer.hpp"

#include "tracker_overlay_detail_sections.hpp"
#include "tracker_overlay_map_sections.hpp"
#include "tracker_overlay_render_state.hpp"
#include "tracker_overlay_style.hpp"
#include "tracker_map_context_menu.hpp"
#include "tracker_pack_layout_renderer.hpp"
#include "tracker_pin_context_menu.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace sekaiemu::spike {
namespace {

std::string SnapshotStatusString(const TrackerRuntime& runtime, std::string_view key) {
  const auto& snapshot = runtime.AuthoritativeState().snapshot;
  if (!snapshot.is_object()) {
    return {};
  }
  if (const auto status = snapshot.find("status"); status != snapshot.end() && status->is_object()) {
    if (const auto found = status->find(std::string(key));
        found != status->end() && found->is_string()) {
      return found->get<std::string>();
    }
  }
  if (const auto found = snapshot.find(std::string(key));
      found != snapshot.end() && found->is_string()) {
    return found->get<std::string>();
  }
  return {};
}

bool SnapshotIsTrackerError(const TrackerRuntime& runtime) {
  return SnapshotStatusString(runtime, "state") == "error" ||
         !SnapshotStatusString(runtime, "error").empty();
}

bool SnapshotIsTrackerLoading(const TrackerRuntime& runtime) {
  return SnapshotStatusString(runtime, "state") == "loading";
}

void DrawTrackerLoading(OverlayCanvas& canvas,
                        const UiPalette& palette,
                        const TrackerRuntime& runtime,
                        int x,
                        int y,
                        int width,
                        int height) {
  const auto message = SnapshotStatusString(runtime, "message");
  DrawSectionBox(canvas, palette, x, y, width, height);
  canvas.DrawText(x + 10, y + 12, "LOADING TRACKER...", palette.accent, 2);
  canvas.DrawWrappedText(x + 10,
                         y + 44,
                         std::max(80, width - 20),
                         message.empty() ? "Loading tracker..." : message,
                         palette.text_primary,
                         1,
                         5);
  canvas.DrawWrappedText(x + 10,
                         y + std::max(96, height - 54),
                         std::max(80, width - 20),
                         "Gameplay can continue while Sekaiemu prepares tracker data.",
                         palette.text_secondary,
                         1,
                         5);
}

void DrawTrackerError(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      int x,
                      int y,
                      int width,
                      int height) {
  const auto code = SnapshotStatusString(runtime, "error");
  const auto message = SnapshotStatusString(runtime, "message");
  DrawSectionBox(canvas, palette, x, y, width, height);
  canvas.DrawText(x + 10, y + 12, "TRACKER ERROR", UiColor{255, 116, 116, 255}, 2);
  canvas.DrawText(x + 10,
                  y + 38,
                  TruncateText(code.empty() ? "tracker_unavailable" : code, 36),
                  palette.warning,
                  1);
  canvas.DrawWrappedText(x + 10,
                         y + 58,
                         std::max(80, width - 20),
                         message.empty()
                             ? "Sekaiemu did not receive a compatible tracker snapshot."
                             : message,
                         palette.text_primary,
                         1,
                         5);
  canvas.DrawWrappedText(x + 10,
                         y + std::max(96, height - 54),
                         std::max(80, width - 20),
                         "Gameplay keeps running, but the old tracker fallback is disabled.",
                         palette.text_secondary,
                         1,
                         5);
}

void DrawMapContextMenu(OverlayCanvas& canvas,
                        const UiPalette& palette,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        const TrackerPanelLayout& layout,
                        int body_y) {
  if (!runtime.UiState().map_context_menu_visible) {
    return;
  }
  const auto entries = BuildTrackerMapContextMenuEntries(runtime, resolved);
  if (entries.empty()) {
    return;
  }
  const auto metrics = BuildTrackerMapContextMenuMetrics(runtime, resolved, layout, body_y);
  const auto selected = runtime.UiState().map_context_menu_selected_index;

  canvas.FillRect(metrics.x, metrics.y, metrics.width, metrics.height, UiColor{8, 12, 20, 238});
  canvas.DrawRect(metrics.x, metrics.y, metrics.width, metrics.height, palette.accent_soft);

  for (int row = 0; row < metrics.rows; ++row) {
    const bool active = static_cast<std::size_t>(row) == selected;
    const int y = metrics.y + 4 + row * metrics.row_height;
    canvas.FillRect(metrics.x + 4,
                    y,
                    metrics.width - 8,
                    metrics.row_height - 2,
                    active ? palette.accent : UiColor{20, 28, 42, 230});
    const auto& entry = entries[static_cast<std::size_t>(row)];
    std::string label = entry.label.empty() ? "MAP" : entry.label;
    if (entry.expandable) {
      label = std::string(entry.expanded ? "- " : "+ ") + label;
    }
    const int depth = std::clamp(entry.depth, 0, 4);
    if (depth > 0) {
      canvas.FillRect(metrics.x + 12,
                      y + 4,
                      2,
                      metrics.row_height - 6,
                      active ? palette.panel_background : palette.accent_soft);
    }
    const int text_x = metrics.x + 10 + depth * 14;
    canvas.DrawText(text_x,
                    y + 5,
                    TruncateText(label,
                                 static_cast<std::size_t>(
                                     std::max(8, (metrics.width - (text_x - metrics.x) - 10) / 6))),
                    active ? palette.panel_background : palette.text_primary,
                    1);
  }
}

void DrawPinContextMenu(OverlayCanvas& canvas,
                        const UiPalette& palette,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        const TrackerPanelLayout& layout,
                        int body_y) {
  if (!runtime.UiState().pin_context_menu_visible) {
    return;
  }
  const auto entries = BuildTrackerPinContextMenuEntries(runtime, resolved);
  if (entries.empty()) {
    return;
  }
  const auto metrics = BuildTrackerPinContextMenuMetrics(runtime, resolved, layout, body_y);
  const auto selected = runtime.UiState().pin_context_menu_selected_index;

  canvas.FillRect(metrics.x, metrics.y, metrics.width, metrics.height, UiColor{8, 12, 20, 242});
  canvas.DrawRect(metrics.x, metrics.y, metrics.width, metrics.height, palette.accent_soft);

  for (int row = 0; row < metrics.rows; ++row) {
    const bool active = static_cast<std::size_t>(row) == selected;
    const int y = metrics.y + 4 + row * metrics.row_height;
    const auto& entry = entries[static_cast<std::size_t>(row)];
    canvas.FillRect(metrics.x + 4,
                    y,
                    metrics.width - 8,
                    metrics.row_height - 2,
                    active ? palette.accent : UiColor{20, 28, 42, 230});
    const UiColor state_color = entry.checked ? UiColor{108, 118, 132, 255}
                                              : PinFillColor(entry.color, false);
    canvas.DrawRect(metrics.x + 8, y + 4, 9, 9, UiColor{0, 0, 0, 255});
    canvas.FillRect(metrics.x + 9, y + 5, 7, 7, state_color);
    canvas.DrawText(metrics.x + 22,
                    y + 5,
                    TruncateText(entry.label.empty() ? entry.location_id : entry.label,
                                 static_cast<std::size_t>(std::max(8, (metrics.width - 34) / 6))),
                    active ? palette.panel_background : palette.text_primary,
                    1);
  }
}

void DrawHoverTooltip(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerPanelLayout& layout) {
  const auto& ui = runtime.UiState();
  if (!ui.hover_tooltip_visible || ui.hover_tooltip_text.empty() ||
      ui.map_context_menu_visible || ui.pin_context_menu_visible) {
    return;
  }
  const int text_width =
      static_cast<int>(std::min<std::size_t>(ui.hover_tooltip_text.size(), 36)) * 6;
  const int width = std::clamp(text_width + 14, 72, std::max(72, layout.width - 18));
  const int height = 20;
  const int x = std::clamp(ui.hover_tooltip_x + 12, layout.x + 4, layout.x + layout.width - width - 4);
  const int y = std::clamp(ui.hover_tooltip_y + 12, layout.y + 4, layout.y + layout.height - height - 4);
  canvas.FillRect(x, y, width, height, UiColor{0, 0, 0, 220});
  canvas.DrawRect(x, y, width, height, palette.accent_soft);
  canvas.DrawText(x + 7, y + 6, TruncateText(ui.hover_tooltip_text, 36), palette.text_primary, 1);
}

}  // namespace

void RenderTrackerPanel(OverlayCanvas& canvas,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        const TrackerPanelLayout& layout,
                        bool compact,
                        std::string_view header_suffix,
                        const TrackerOverlayAssetResolver* asset_resolver) {
  if (layout.width <= 0 || layout.height <= 0) {
    return;
  }

  const auto* bundle = runtime.Bundle();
  const std::string title =
      bundle != nullptr && !bundle->display_name.empty() ? bundle->display_name : "TRACKER";
  const UiPalette palette;

  canvas.FillRect(layout.x,
                  layout.y,
                  layout.width,
                  layout.height,
                  UiColor{palette.panel_background.r,
                          palette.panel_background.g,
                          palette.panel_background.b,
                          static_cast<std::uint8_t>(compact ? 228 : palette.panel_background.a)});
  canvas.DrawRect(layout.x, layout.y, layout.width, layout.height, palette.panel_border);

  const int header_height = compact ? 24 : 44;
  canvas.FillRect(layout.x, layout.y, layout.width, header_height, palette.header_background);
  std::string header = title;
  if (!header_suffix.empty()) {
    header += " / ";
    header += std::string(header_suffix);
  }
  const bool narrow_header = layout.width < 460;
  const bool show_display_mode = layout.width >= 440;
  const auto display_mode_text =
      show_display_mode ? TruncateText(DisplayModeLabel(runtime.UiState().display_mode, narrow_header), 11)
                        : std::string{};
  const int display_mode_width = show_display_mode ? static_cast<int>(display_mode_text.size()) * 6 + 8 : 0;
  const int header_max_chars =
      compact ? std::max(12, (layout.width - display_mode_width - 24) / 6)
              : std::max(16, (layout.width - display_mode_width - 28) / 10);
  canvas.DrawText(layout.x + 8,
                  layout.y + 8,
                  TruncateText(header, static_cast<std::size_t>(header_max_chars)),
                  palette.accent,
                  compact ? 1 : 2);
  if (!compact) {
    const auto tab_label = ResolveActiveTabLabel(resolved);
    const auto* active_map = ResolveActiveMap(runtime, resolved);
    const std::string map_label =
        active_map != nullptr && !active_map->label.empty() ? active_map->label : "No map";
    canvas.DrawText(layout.x + 8,
                    layout.y + 28,
                    TruncateText("TAB " + tab_label + " / MAP " + map_label, 50),
                    palette.text_secondary,
                    1);
  }
  if (show_display_mode) {
    canvas.DrawText(layout.x + layout.width - display_mode_width,
                    layout.y + 8,
                    display_mode_text,
                    palette.text_secondary,
                    1);
  }

  const int body_x = layout.x + 8;
  const int body_y = layout.y + header_height + 6;
  const int body_width = layout.width - 16;
  const int body_height = layout.y + layout.height - body_y - 8;

  if (SnapshotIsTrackerError(runtime)) {
    DrawTrackerError(canvas, palette, runtime, body_x, body_y, body_width, body_height);
    return;
  }
  if (SnapshotIsTrackerLoading(runtime)) {
    DrawTrackerLoading(canvas, palette, runtime, body_x, body_y, body_width, body_height);
    return;
  }

  if (compact || body_width < 320 || body_height < 180) {
    DrawCompactBody(canvas, palette, runtime, resolved, body_x, body_y, body_width, body_height);
    DrawMapContextMenu(canvas, palette, runtime, resolved, layout, body_y);
    DrawPinContextMenu(canvas, palette, runtime, resolved, layout, body_y);
    DrawHoverTooltip(canvas, palette, runtime, layout);
    return;
  }

  if (RenderPackDrivenTrackerBody(
          canvas, runtime, resolved, body_x, body_y, body_width, body_height, asset_resolver)) {
    DrawMapContextMenu(canvas, palette, runtime, resolved, layout, body_y);
    DrawPinContextMenu(canvas, palette, runtime, resolved, layout, body_y);
    DrawHoverTooltip(canvas, palette, runtime, layout);
    return;
  }

  const int gap = 8;
  const bool condensed_side = body_height < 420 || body_width < 420;
  const int map_width = std::max(148, body_width * (condensed_side ? 46 : 50) / 100);
  const int side_width = std::max(120, body_width - map_width - gap);
  const int map_height = body_height;

  DrawMapSection(canvas, palette, runtime, resolved, body_x, body_y, map_width, map_height, asset_resolver);

  const int side_x = body_x + map_width + gap;
  int side_y = body_y;

  const int summary_height = condensed_side ? 78 : 88;
  const int items_height = condensed_side ? 82 : 96;
  const int metadata_height = 0;
  const int recent_height = condensed_side ? 56 : 62;
  const int section_gaps = gap * (metadata_height > 0 ? 4 : 3);
  const int info_height =
      std::max(condensed_side ? 132 : 72,
               body_height - summary_height - items_height - metadata_height - recent_height - section_gaps);

  DrawSummarySection(canvas, palette, runtime, resolved, side_x, side_y, side_width, summary_height);
  side_y += summary_height + gap;
  DrawItemsSection(canvas, palette, runtime, resolved, side_x, side_y, side_width, items_height, asset_resolver);
  side_y += items_height + gap;
  DrawInfoPanels(canvas, palette, resolved, side_x, side_y, side_width, info_height);
  side_y += info_height + gap;
  DrawRecentEvents(canvas, palette, runtime, resolved, side_x, side_y, side_width, recent_height);
  DrawMapContextMenu(canvas, palette, runtime, resolved, layout, body_y);
  DrawPinContextMenu(canvas, palette, runtime, resolved, layout, body_y);
  DrawHoverTooltip(canvas, palette, runtime, layout);
}

}  // namespace sekaiemu::spike
