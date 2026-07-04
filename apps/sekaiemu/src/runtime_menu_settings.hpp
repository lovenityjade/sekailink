#pragma once

#include "overlay_canvas.hpp"
#include "runtime_settings_mode.hpp"
#include "tracker_runtime.hpp"

#include <string>

namespace sekaiemu::spike {

int RuntimeMenuSettingsRowCount();

void DrawRuntimeMenuSettingsPage(OverlayCanvas& canvas,
                                 int list_x,
                                 int list_y,
                                 int list_width,
                                 int list_bottom,
                                 int selected_index,
                                 RuntimeSettingsMode mode,
                                 bool chat_overlay_enabled,
                                 bool notifications_enabled,
                                 int master_volume_percent,
                                 const TrackerRuntime* tracker_runtime,
                                 std::string& tooltip);

}  // namespace sekaiemu::spike
