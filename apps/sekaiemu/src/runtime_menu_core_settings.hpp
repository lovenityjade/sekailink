#pragma once

#include "core_option_manager.hpp"
#include "overlay_canvas.hpp"
#include "runtime_menu.hpp"

#include <span>
#include <string>

namespace sekaiemu::spike {

void DrawRuntimeMenuCoreSettingsPage(OverlayCanvas& canvas,
                                     int list_x,
                                     int list_y,
                                     int list_width,
                                     int list_bottom,
                                     int selected_index,
                                     int scroll_offset,
                                     int& visible_rows,
                                     const CoreOptionManager& core_options,
                                     std::span<const RuntimeMenuCoreActionRow> action_rows,
                                     std::string& tooltip);

}  // namespace sekaiemu::spike
