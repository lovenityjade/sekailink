#include "tracker_pack_layout_widgets.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_pack_layout_engine.hpp"
#include "tracker_pack_layout_json.hpp"
#include "tracker_pack_layout_visual_state.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace sekaiemu::spike::tracker_pack_layout_detail {
namespace {

constexpr UiColor kItemCellBackground{4, 6, 10, 255};
constexpr UiColor kDisabledItemCellBackground{3, 4, 7, 255};
constexpr UiColor kItemCellBorder{42, 54, 70, 255};
constexpr UiColor kLegacyTransparencyKey{255, 0, 255, 255};
constexpr UiColor kTransparent{0, 0, 0, 0};

void DrawImageMods(OverlayCanvas& canvas,
                   const TrackerOverlayAssetResolver* asset_resolver,
                   std::string_view mods,
                   const Rect& rect) {
  if (mods.empty() || mods == "none" || mods == "@disabled") {
    return;
  }
  constexpr std::string_view kOverlayPrefix = "overlay|";
  if (mods.rfind(kOverlayPrefix, 0) != 0) {
    return;
  }
  const auto asset_path = NormalizePackAssetPath(std::string(mods.substr(kOverlayPrefix.size())));
  const auto asset = ResolveAsset(asset_resolver, asset_path);
  if (!asset.has_value() || asset->rgba_pixels == nullptr || asset->width == 0 || asset->height == 0) {
    return;
  }
  canvas.DrawImageWithColorKey(rect.x,
                               rect.y,
                               rect.width,
                               rect.height,
                               asset->rgba_pixels,
                               asset->width,
                               asset->height,
                               kLegacyTransparencyKey,
                               kTransparent);
}

bool PinChecked(const BundlePinRenderMetadata& pin,
                const std::unordered_set<std::string>& checked_ids) {
  const auto state_id = pin.location_id.empty() ? pin.id : pin.location_id;
  return pin.has_explicit_checked ? pin.checked : checked_ids.contains(state_id);
}

bool ShouldDrawCountBadge(std::string_view code,
                          const PackVisualDefinition* definition,
                          const PackVisualState& state) {
  if (state.count <= 1 && state.stage <= 1) {
    return false;
  }
  std::string token = CanonicalToken(code);
  if (definition != nullptr) {
    token += CanonicalToken(definition->primary_code);
    token += CanonicalToken(definition->label);
  }
  return token.find("smallkey") != std::string::npos ||
         token.find("bigkey") != std::string::npos ||
         token.find("keydrop") != std::string::npos;
}

}  // namespace

void DrawFrame(OverlayCanvas& canvas, const UiPalette& palette, const Rect& rect, std::string_view title) {
  canvas.FillRect(rect.x, rect.y, rect.width, rect.height, palette.section_background);
  canvas.DrawRect(rect.x, rect.y, rect.width, rect.height, palette.section_border);
  if (!title.empty()) {
    canvas.FillRect(rect.x, rect.y, rect.width, 18, palette.header_background);
    canvas.DrawText(rect.x + 6, rect.y + 5, title, palette.accent, 1);
  }
}

void DrawFrameWithCustomHeader(OverlayCanvas& canvas,
                               const UiPalette& palette,
                               const Rect& rect,
                               std::string_view title,
                               std::string_view header_background) {
  canvas.FillRect(rect.x, rect.y, rect.width, rect.height, palette.section_background);
  canvas.DrawRect(rect.x, rect.y, rect.width, rect.height, palette.section_border);
  if (!title.empty()) {
    const auto header_color = ParseHexColor(header_background, palette.header_background);
    canvas.FillRect(rect.x, rect.y, rect.width, 18, header_color);
    canvas.DrawText(rect.x + 6, rect.y + 5, title, palette.accent, 1);
  }
}

std::optional<TrackerOverlayResolvedAsset> ResolveAsset(const TrackerOverlayAssetResolver* asset_resolver,
                                                        std::string_view asset_path) {
  if (asset_resolver == nullptr || asset_path.empty()) {
    return std::nullopt;
  }
  return asset_resolver->ResolveTrackerAsset(asset_path);
}

std::string SelectLayoutRoot(const PackLayoutDocument& document, int width, int height) {
  if (width > height && document.layouts.contains("tracker_horizontal")) {
    return "tracker_horizontal";
  }
  if (document.layouts.contains("tracker_default")) {
    return "tracker_default";
  }
  if (document.layouts.contains("tracker_horizontal")) {
    return "tracker_horizontal";
  }
  if (!document.layouts.empty()) {
    return document.layouts.begin()->first;
  }
  return {};
}

void DrawPackItemCell(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const Rect& rect,
                      const PackStateContext& context,
                      std::string_view code,
                      int requested_badge_scale) {
  if (code.empty()) {
    return;
  }
  const PackVisualDefinition* definition = FindPackVisualDefinition(context, code);
  std::unordered_set<std::string> visiting;
  const auto state = ResolvePackVisualState(context, code, &visiting);

  const bool active = definition != nullptr && definition->static_only ? true : state.acquired;
  const bool visually_enabled =
      active || (definition != nullptr && !definition->allow_disabled);
  canvas.FillRect(rect.x,
                  rect.y,
                  rect.width,
                  rect.height,
                  visually_enabled ? kItemCellBackground : kDisabledItemCellBackground);
  canvas.DrawRect(rect.x,
                  rect.y,
                  rect.width,
                  rect.height,
                  visually_enabled ? kItemCellBorder : palette.section_border);

  bool drew_image = false;
  if (definition != nullptr) {
    const auto selection = SelectVisualAsset(*definition, state);
    const auto asset = ResolveAsset(context.asset_resolver, selection.image);
    if (asset.has_value() && asset->rgba_pixels != nullptr && asset->width > 0 && asset->height > 0) {
      const Rect image_rect{rect.x + 1, rect.y + 1, std::max(1, rect.width - 2), std::max(1, rect.height - 2)};
      canvas.DrawImageWithColorKey(image_rect.x,
                                   image_rect.y,
                                   image_rect.width,
                                   image_rect.height,
                                   asset->rgba_pixels,
                                   asset->width,
                                   asset->height,
                                   kLegacyTransparencyKey,
                                   kTransparent);
      DrawImageMods(canvas, context.asset_resolver, selection.mods, image_rect);
      drew_image = true;
      if (!visually_enabled && !definition->static_only) {
        canvas.FillRect(rect.x + 1, rect.y + 1, std::max(1, rect.width - 2), std::max(1, rect.height - 2),
                        UiColor{10, 12, 16, 170});
      }
    }
  }

  const int text_scale =
      requested_badge_scale > 1 && rect.width >= 24 && rect.height >= 20 ? 2 : 1;

  if (!drew_image) {
    const auto badge = BadgeText(state.label, std::string(code));
    canvas.DrawText(rect.x + 2,
                    rect.y + std::max(1, rect.height / 2 - 4 * text_scale),
                    TruncateText(badge, 2),
                    visually_enabled ? palette.text_primary : palette.text_secondary,
                    text_scale);
  }

  if (ShouldDrawCountBadge(code, definition, state) && rect.width >= 14 && rect.height >= 12) {
    const auto badge_text = std::to_string(std::max(state.count, state.stage));
    int badge_scale = text_scale;
    int badge_width = static_cast<int>(badge_text.size()) * 6 * badge_scale + 4;
    int badge_height = 8 * badge_scale + 2;
    if (badge_width > rect.width - 2 || badge_height > rect.height - 2) {
      badge_scale = 1;
      badge_width = static_cast<int>(badge_text.size()) * 6 + 4;
      badge_height = 10;
    }
    const int badge_x = rect.x + rect.width - badge_width - 2;
    const int badge_y = rect.y + rect.height - badge_height - 2;
    canvas.FillRect(badge_x, badge_y, badge_width, badge_height, UiColor{4, 20, 12, 220});
    canvas.DrawText(badge_x + 2,
                    badge_y + 2,
                    badge_text,
                    definition != nullptr && definition->max_quantity > 0 && state.count >= definition->max_quantity
                        ? palette.success
                        : visually_enabled ? palette.text_primary
                                           : palette.text_secondary,
                    badge_scale);
  }
}

void DrawPackPinsForMap(OverlayCanvas& canvas,
                        const UiPalette& palette,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        int target_x,
                        int target_y,
                        int target_width,
                        int target_height,
                        unsigned source_width,
                        unsigned source_height,
                        std::string_view map_id,
                        std::string_view pack_map,
                        const TrackerOverlayAssetResolver*) {
  auto pins = BuildBundlePins(runtime, resolved);
  const auto checked_ids = CheckedLocationIds(runtime);
  std::size_t rendered = 0;
  for (const auto& pin : pins) {
    if (!pack_map.empty()) {
      if (pin.pack_map.empty() || CanonicalToken(pin.pack_map) != CanonicalToken(pack_map)) {
        continue;
      }
    } else if (!map_id.empty()) {
      if (pin.map_id.empty() || pin.map_id != map_id) {
        continue;
      }
    }
    const bool checked = PinChecked(pin, checked_ids);
    if (checked) {
      continue;
    }
    const int px = ResolveMapCoordinate(pin.x, target_x, target_width, source_width);
    const int py = ResolveMapCoordinate(pin.y, target_y, target_height, source_height);
    DrawPinMarker(canvas, palette, px, py, pin, checked);
    if (++rendered >= 160) {
      break;
    }
  }
}

void DrawPackItemRow(OverlayCanvas& canvas,
                     const UiPalette& palette,
                     const PackStateContext& context,
                     const Rect& rect,
                     std::string_view code,
                     std::string_view label_override,
                     bool compact) {
  if (code.empty()) {
    return;
  }
  std::unordered_set<std::string> visiting;
  const auto state = ResolvePackVisualState(context, code, &visiting);
  const PackVisualDefinition* definition = FindPackVisualDefinition(context, code);
  const bool active = definition != nullptr && definition->static_only ? true : state.acquired;

  canvas.FillRect(rect.x, rect.y, rect.width, rect.height, active ? UiColor{26, 36, 48, 255}
                                                                   : UiColor{14, 18, 24, 255});
  canvas.DrawRect(rect.x, rect.y, rect.width, rect.height, active ? palette.accent_soft : palette.section_border);

  const int icon_size = std::max(14, std::min(rect.height - 6, compact ? 18 : 20));
  const Rect icon_rect{rect.x + 4, rect.y + (rect.height - icon_size) / 2, icon_size, icon_size};
  DrawPackItemCell(canvas, palette, icon_rect, context, code);

  std::string label = !label_override.empty() ? std::string(label_override)
                                              : !state.label.empty() ? state.label
                                                                     : std::string(code);
  const int text_x = icon_rect.x + icon_rect.width + 6;
  const int text_width = std::max(0, rect.x + rect.width - text_x - 6);
  if (text_width > 24) {
    canvas.DrawText(text_x,
                    rect.y + (compact ? 9 : 10),
                    TruncateText(label, static_cast<std::size_t>(std::max(8, text_width / 6))),
                    active ? palette.text_primary : palette.text_secondary,
                    1);
  }

  if ((state.count > 1 || state.stage > 1) && rect.width >= 34) {
    const auto badge = std::to_string(std::max(state.count, state.stage));
    const int badge_width = std::max(12, static_cast<int>(badge.size()) * 6 + 4);
    const int badge_x = rect.x + rect.width - badge_width - 4;
    canvas.FillRect(badge_x, rect.y + rect.height - 10, badge_width, 8, UiColor{4, 20, 12, 220});
    canvas.DrawText(badge_x + 2,
                    rect.y + rect.height - 8,
                    badge,
                    definition != nullptr && definition->max_quantity > 0 && state.count >= definition->max_quantity
                        ? palette.success
                        : active ? palette.text_primary
                                 : palette.text_secondary,
                    1);
  }
}

void DrawRecentPins(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    const PackStateContext& context,
                    const Rect& rect,
                    RecentPinsStyle style,
                    bool compact) {
  auto recent_rows = BuildRecentRows(context.runtime, context.resolved);
  if (recent_rows.empty()) {
    canvas.DrawText(rect.x + 6, rect.y + 8, "NO RECENT PINS", palette.text_secondary, 1);
    return;
  }

  const int row_height = compact ? 16 : 18;
  const int chip_height = compact ? 14 : 16;
  const int max_rows = compact ? 6 : 8;
  if (style == RecentPinsStyle::Wrap) {
    int cursor_x = rect.x + 4;
    int cursor_y = rect.y + 4;
    int rendered = 0;
    for (auto it = recent_rows.rbegin(); it != recent_rows.rend() && rendered < max_rows; ++it, ++rendered) {
      const auto text = TruncateText(it->label.empty() ? it->canonical_id : it->label, compact ? 14 : 18);
      const int chip_width = std::max(44, static_cast<int>(text.size()) * 6 + 14);
      if (cursor_x + chip_width > rect.x + rect.width - 4) {
        cursor_x = rect.x + 4;
        cursor_y += chip_height + 4;
      }
      if (cursor_y + chip_height > rect.y + rect.height - 2) {
        break;
      }
      canvas.FillRect(cursor_x, cursor_y, chip_width, chip_height, palette.header_background);
      canvas.DrawRect(cursor_x, cursor_y, chip_width, chip_height, palette.section_border);
      canvas.DrawText(cursor_x + 4, cursor_y + 4, text, palette.text_secondary, 1);
      cursor_x += chip_width + 4;
    }
    return;
  }

  int cursor_y = rect.y + 4;
  int rendered = 0;
  for (auto it = recent_rows.rbegin(); it != recent_rows.rend() && rendered < max_rows; ++it, ++rendered) {
    if (cursor_y + row_height > rect.y + rect.height - 2) {
      break;
    }
    canvas.FillRect(rect.x + 4, cursor_y, std::max(0, rect.width - 8), row_height, UiColor{18, 26, 40, 255});
    canvas.DrawRect(rect.x + 4, cursor_y, std::max(0, rect.width - 8), row_height, palette.section_border);
    const std::string label =
        it->label.empty() ? it->canonical_id : !it->detail.empty() ? (it->label + " " + it->detail) : it->label;
    canvas.DrawText(rect.x + 8,
                    cursor_y + 5,
                    TruncateText(label, static_cast<std::size_t>(std::max(10, (rect.width - 16) / 6))),
                    palette.text_secondary,
                    1);
    cursor_y += row_height + 3;
  }
}

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
