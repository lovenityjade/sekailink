#include "tracker_overlay_map_sections.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_pack_layout_json.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {
namespace {

std::string ActivePackMapName(const TrackerRuntime& runtime,
                              const TrackerResolvedViewState& resolved) {
  const auto* active_map = ResolveActiveMap(runtime, resolved);
  if (active_map == nullptr) {
    return {};
  }
  return JsonStringAtAnyKey(active_map->raw, {"poptracker_map_name", "pack_map_name", "packMapName"});
}

bool PinMatchesMap(const BundlePinRenderMetadata& pin,
                   std::string_view map_id,
                   std::string_view pack_map) {
  if (!pack_map.empty()) {
    if (!pin.pack_map.empty()) {
      return CanonicalToken(pin.pack_map) == CanonicalToken(pack_map);
    }
    return !map_id.empty() && pin.map_id == map_id;
  }
  if (!map_id.empty()) {
    return !pin.map_id.empty() && pin.map_id == map_id;
  }
  return true;
}

std::vector<BundlePinRenderMetadata> FilterPinsForMap(const TrackerRuntime& runtime,
                                                      const TrackerResolvedViewState& resolved,
                                                      const std::vector<BundlePinRenderMetadata>& pins) {
  const auto pack_map = ActivePackMapName(runtime, resolved);
  std::vector<BundlePinRenderMetadata> filtered;
  filtered.reserve(pins.size());
  for (const auto& pin : pins) {
    if (PinMatchesMap(pin, resolved.active_map_id, pack_map)) {
      filtered.push_back(pin);
    }
  }
  return filtered;
}

void DrawMapPins(OverlayCanvas& canvas,
                 const UiPalette& palette,
                 const std::vector<BundlePinRenderMetadata>& pins,
                 const std::unordered_set<std::string>& checked_ids,
                 int target_x,
                 int target_y,
                 int target_width,
                 int target_height,
                 unsigned source_width,
                 unsigned source_height) {
  if (pins.empty()) {
    return;
  }
  std::size_t rendered = 0;
  for (const auto& pin : pins) {
    if (rendered >= 24) {
      break;
    }
    const auto state_id = pin.location_id.empty() ? pin.id : pin.location_id;
    const bool checked = pin.has_explicit_checked ? pin.checked : checked_ids.find(state_id) != checked_ids.end();
    if (checked) {
      continue;
    }
    const int px = ResolveMapCoordinate(pin.x, target_x, target_width, source_width);
    const int py = ResolveMapCoordinate(pin.y, target_y, target_height, source_height);
    DrawPinMarker(canvas, palette, px, py, pin, checked);
    ++rendered;
  }
}

}  // namespace

void DrawTabStrip(OverlayCanvas& canvas,
                  const UiPalette& palette,
                  const TrackerResolvedViewState& resolved,
                  int x,
                  int y,
                  int width) {
  DrawSectionBox(canvas, palette, x, y, width, 24);
  int cursor_x = x + 6;
  const int max_x = x + width - 54;
  for (const auto& tab : resolved.visible_tabs) {
    if (!tab.is_object()) {
      continue;
    }
    const auto tab_id = tab.value("id", std::string{});
    const auto label = TruncateText(LabelOrId(tab), 12);
    const int chip_width = std::max(48, static_cast<int>(label.size()) * 6 + 12);
    if (cursor_x + chip_width > max_x) {
      break;
    }
    DrawChip(canvas, palette, cursor_x, y + 4, label, tab_id == resolved.active_tab_id);
    cursor_x += chip_width + 6;
  }
}

void DrawMapSection(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    const TrackerRuntime& runtime,
                    const TrackerResolvedViewState& resolved,
                    int x,
                    int y,
                    int width,
                    int height,
                    const TrackerOverlayAssetResolver* asset_resolver) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "MAP VIEW");

  const auto* active_map = ResolveActiveMap(runtime, resolved);
  const std::string map_label =
      active_map != nullptr && !active_map->label.empty() ? active_map->label : "Unknown";
  canvas.DrawText(x + 8, y + 24, TruncateText(map_label, 20), palette.text_primary, 1);
  canvas.DrawText(x + width - 98,
                  y + 24,
                  TruncateText(resolved.current_zone_id.empty() ? "zone.unknown"
                                                                : resolved.current_zone_id,
                               15),
                  palette.text_secondary,
                  1);

  const int image_x = x + 8;
  const int image_y = y + 40;
  const int image_width = width - 16;
  const int image_height = std::max(32, height - 68);
  int target_x = image_x + 4;
  int target_y = image_y + 4;
  int target_width = std::max(1, image_width - 8);
  int target_height = std::max(1, image_height - 8);
  unsigned source_width = 0;
  unsigned source_height = 0;

  canvas.FillRect(image_x, image_y, image_width, image_height, UiColor{5, 7, 12, 255});
  canvas.DrawRect(image_x, image_y, image_width, image_height, palette.section_border);

  std::optional<TrackerOverlayResolvedAsset> resolved_map_asset;
  if (active_map != nullptr && asset_resolver != nullptr && !active_map->image.empty()) {
    resolved_map_asset = asset_resolver->ResolveTrackerAsset(active_map->image);
  }
  if (active_map == nullptr ||
      ((!active_map->raster_image.has_value() || active_map->raster_image->width == 0 ||
        active_map->raster_image->height == 0) &&
       (!resolved_map_asset.has_value() || resolved_map_asset->rgba_pixels == nullptr ||
        resolved_map_asset->width == 0 || resolved_map_asset->height == 0))) {
    canvas.DrawWrappedText(
        image_x + 8,
        image_y + 10,
        image_width - 16,
        "NO MAP RASTER YET. BUNDLE PINS STILL RENDER AS A FALLBACK STATE LAYER.",
        palette.text_secondary,
        1,
        3);
  } else {
    TrackerOverlayResolvedAsset image;
    if (active_map->raster_image.has_value() && active_map->raster_image->width > 0 &&
        active_map->raster_image->height > 0) {
      image = TrackerOverlayResolvedAsset{active_map->raster_image->width,
                                          active_map->raster_image->height,
                                          active_map->raster_image->rgba_pixels.data()};
    } else {
      image = *resolved_map_asset;
    }
    source_width = image.width;
    source_height = image.height;
    const double aspect = static_cast<double>(image.width) / static_cast<double>(image.height);
    target_width = std::max(1, image_width - 8);
    target_height = std::max(1, static_cast<int>(target_width / aspect));
    if (target_height > image_height - 8) {
      target_height = std::max(1, image_height - 8);
      target_width = std::max(1, static_cast<int>(target_height * aspect));
    }
    target_x = image_x + ((image_width - target_width) / 2);
    target_y = image_y + ((image_height - target_height) / 2);
    canvas.DrawImage(target_x,
                     target_y,
                     target_width,
                     target_height,
                     image.rgba_pixels,
                     image.width,
                     image.height);
  }

  const auto pins = BuildBundlePins(runtime, resolved);
  const auto map_pins = FilterPinsForMap(runtime, resolved, pins);
  const auto checked_ids = CheckedLocationIds(runtime);
  if (!map_pins.empty()) {
    canvas.FillRect(image_x + 4, image_y + image_height - 18, std::min(image_width - 8, 126), 14, UiColor{5, 7, 12, 210});
    canvas.DrawText(image_x + 8,
                    image_y + image_height - 14,
                    TruncateText("PINS " + std::to_string(checked_ids.size()) + "/" + std::to_string(map_pins.size()), 18),
                    palette.text_secondary,
                    1);
  }
  DrawMapPins(canvas,
              palette,
              map_pins,
              checked_ids,
              target_x,
              target_y,
              target_width,
              target_height,
              source_width,
              source_height);
}

void DrawSummarySection(OverlayCanvas& canvas,
                        const UiPalette& palette,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        int x,
                        int y,
                        int width,
                        int height) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "SUMMARY");

  const auto& observed = runtime.ObservedState();
  const std::size_t checked = CheckedCount(runtime);
  const std::size_t missing = MissingCount(runtime);
  const std::size_t total_known = std::max<std::size_t>(checked + missing, checked);
  const std::size_t received = ReceivedCount(runtime);
  const std::size_t local_checks = observed.locally_checked_locations.size();
  const std::size_t local_items = observed.locally_received_items.size();
  const std::string checked_summary =
      total_known == 0 ? std::to_string(checked) : std::to_string(checked) + "/" + std::to_string(total_known);

  int cursor_y = y + 24;
  DrawMetricRow(canvas, palette, x + 8, cursor_y, "CLEAR", checked_summary);
  canvas.DrawText(x + width - 40, cursor_y, FormatPercent(checked, total_known), palette.success, 1);
  DrawProgressBar(canvas, palette, x + 8, cursor_y + 10, width - 16, checked, total_known, palette.success);
  cursor_y += 22;

  DrawMetricRow(canvas, palette, x + 8, cursor_y, "OPEN", std::to_string(missing));
  DrawProgressBar(canvas, palette, x + 8, cursor_y + 10, width - 16, missing, total_known, palette.warning);
  cursor_y += 22;

  DrawMetricRow(canvas, palette, x + 8, cursor_y, "ITEMS", std::to_string(received));
  DrawMetricRow(canvas, palette, x + (width / 2), cursor_y, "LOCAL", std::to_string(local_items));
  cursor_y += 14;
  DrawMetricRow(canvas, palette, x + 8, cursor_y, "CHECK BUF", std::to_string(local_checks));
  DrawMetricRow(canvas,
                palette,
                x + (width / 2),
                cursor_y,
                "FOLLOW",
                resolved.auto_follow_map ? "AUTO" : "MANUAL");
}

}  // namespace sekaiemu::spike
