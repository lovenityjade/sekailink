#pragma once

#include "overlay_canvas.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

namespace sekaiemu::spike {

bool RenderPackDrivenTrackerBody(OverlayCanvas& canvas,
                                 const TrackerRuntime& runtime,
                                 const TrackerResolvedViewState& resolved,
                                 int x,
                                 int y,
                                 int width,
                                 int height,
                                 const TrackerOverlayAssetResolver* asset_resolver = nullptr);

}  // namespace sekaiemu::spike
