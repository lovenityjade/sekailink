#pragma once

#include "overlay_canvas.hpp"
#include "runtime_menu.hpp"

#include <span>
#include <string>

namespace sekaiemu::spike {

void DrawRuntimeMenuMainPage(OverlayCanvas& canvas,
                             int list_x,
                             int list_y,
                             int list_width,
                             int list_bottom,
                             int selected_index,
                             std::span<const RuntimeMenuMainActionRow> rows,
                             std::span<const SaveStateSlotMenuInfo> save_slots,
                             int highlighted_slot,
                             std::string& tooltip);

}  // namespace sekaiemu::spike
