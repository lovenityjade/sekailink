#pragma once

#include "overlay_canvas.hpp"
#include "tracker_runtime.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace sekaiemu::spike {

struct TrackerPanelLayout {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct TrackerOverlayResolvedAsset {
  unsigned width = 0;
  unsigned height = 0;
  const std::uint8_t* rgba_pixels = nullptr;
};

class TrackerOverlayAssetResolver {
 public:
  virtual ~TrackerOverlayAssetResolver() = default;
  virtual std::optional<TrackerOverlayResolvedAsset> ResolveTrackerAsset(
      std::string_view bundle_relative_path) const = 0;
};

void RenderTrackerPanel(OverlayCanvas& canvas,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        const TrackerPanelLayout& layout,
                        bool compact,
                        std::string_view header_suffix = {},
                        const TrackerOverlayAssetResolver* asset_resolver = nullptr);

}  // namespace sekaiemu::spike
