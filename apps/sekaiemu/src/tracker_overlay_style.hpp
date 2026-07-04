#pragma once

#include "overlay_canvas.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_overlay_render_state.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

struct UiPalette {
  UiColor panel_background{8, 12, 22, 255};
  UiColor panel_border{82, 106, 132, 255};
  UiColor header_background{22, 34, 52, 255};
  UiColor section_background{14, 21, 34, 255};
  UiColor section_border{64, 84, 108, 255};
  UiColor accent{232, 196, 112, 255};
  UiColor accent_soft{146, 182, 216, 255};
  UiColor text_primary{246, 244, 232, 255};
  UiColor text_secondary{174, 188, 206, 255};
  UiColor success{124, 198, 132, 255};
  UiColor warning{232, 180, 96, 255};
};

std::string DisplayModeLabel(TrackerDisplayMode mode, bool compact);
std::string BadgeText(const std::string& label, const std::string& fallback);
UiColor ParseHexColor(std::string_view value, UiColor fallback);
UiColor PinFillColor(std::string_view color, bool checked);

void DrawSectionBox(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    int x,
                    int y,
                    int width,
                    int height);
void DrawSectionHeader(OverlayCanvas& canvas,
                       const UiPalette& palette,
                       int x,
                       int y,
                       int width,
                       std::string_view title);
void DrawChip(OverlayCanvas& canvas,
              const UiPalette& palette,
              int x,
              int y,
              std::string_view text,
              bool active);
void DrawChipRows(OverlayCanvas& canvas,
                  const UiPalette& palette,
                  const std::vector<std::string>& chips,
                  int x,
                  int y,
                  int width,
                  int max_rows,
                  int active_index);
void DrawMetricRow(OverlayCanvas& canvas,
                   const UiPalette& palette,
                   int x,
                   int y,
                   std::string_view label,
                   std::string_view value);
void DrawProgressBar(OverlayCanvas& canvas,
                     const UiPalette& palette,
                     int x,
                     int y,
                     int width,
                     std::size_t value,
                     std::size_t maximum,
                     UiColor fill);
void DrawItemBadge(OverlayCanvas& canvas,
                   const UiPalette& palette,
                   int x,
                   int y,
                   int size,
                   const BundleItemRenderMetadata& item,
                   bool received,
                   const TrackerOverlayAssetResolver* asset_resolver);
void DrawPinMarker(OverlayCanvas& canvas,
                   const UiPalette& palette,
                   int center_x,
                   int center_y,
                   const BundlePinRenderMetadata& pin,
                   bool checked);
int ResolveMapCoordinate(double coordinate,
                         int target_origin,
                         int target_size,
                         unsigned source_size);

}  // namespace sekaiemu::spike
