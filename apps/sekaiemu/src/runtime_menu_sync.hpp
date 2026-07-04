#pragma once

#include "bridge_runtime_status.hpp"
#include "overlay_canvas.hpp"
#include "tracker_runtime.hpp"

namespace sekaiemu::spike {

void DrawRuntimeMenuCompletionHeader(OverlayCanvas& canvas,
                                     int x,
                                     int y,
                                     int width,
                                     const TrackerRuntime* tracker_runtime);

void DrawRuntimeMenuSyncInfoPage(OverlayCanvas& canvas,
                                 int x,
                                 int y,
                                 int width,
                                 int bottom,
                                 const BridgeRuntimeStatus& bridge_status,
                                 const TrackerRuntime* tracker_runtime);

}  // namespace sekaiemu::spike
