#pragma once

#include "tracker_overlay_style.hpp"
#include "tracker_pack_layout_model.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace sekaiemu::spike::tracker_pack_layout_detail {

void DrawFrame(OverlayCanvas& canvas, const UiPalette& palette, const Rect& rect, std::string_view title);
void DrawFrameWithCustomHeader(OverlayCanvas& canvas,
                               const UiPalette& palette,
                               const Rect& rect,
                               std::string_view title,
                               std::string_view header_background);
std::optional<TrackerOverlayResolvedAsset> ResolveAsset(const TrackerOverlayAssetResolver* asset_resolver,
                                                        std::string_view asset_path);
std::string SelectLayoutRoot(const PackLayoutDocument& document, int width, int height);
void DrawPackItemCell(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const Rect& rect,
                      const PackStateContext& context,
                      std::string_view code,
                      int requested_badge_scale = 1);
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
                        const TrackerOverlayAssetResolver* asset_resolver);
void DrawPackItemRow(OverlayCanvas& canvas,
                     const UiPalette& palette,
                     const PackStateContext& context,
                     const Rect& rect,
                     std::string_view code,
                     std::string_view label_override,
                     bool compact);
void DrawRecentPins(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    const PackStateContext& context,
                    const Rect& rect,
                    RecentPinsStyle style,
                    bool compact);

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
