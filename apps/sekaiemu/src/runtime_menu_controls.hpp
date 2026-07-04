#pragma once

#include "input_state.hpp"
#include "overlay_canvas.hpp"
#include "runtime_settings_mode.hpp"

#include <string>

namespace sekaiemu::spike {

enum class RuntimeControlsPageMode {
  Sekaiemu,
  Core,
};

int RuntimeMenuFrontendControlsRowCount();

void DrawRuntimeMenuControlsPage(OverlayCanvas& canvas,
                                 int list_x,
                                 int list_y,
                                 int list_width,
                                 int list_bottom,
                                 int selected_index,
                                 int scroll_offset,
                                 int& visible_rows,
                                 InputState& input_state,
                                 RuntimeSettingsMode mode,
                                 RuntimeControlsPageMode controls_mode,
                                 std::string& tooltip);

}  // namespace sekaiemu::spike
