#pragma once

#include "core_option_manager.hpp"
#include "overlay_canvas.hpp"
#include "runtime_menu.hpp"

namespace sekaiemu::spike {

void DrawRuntimeMenuTabs(OverlayCanvas& canvas,
                         int x,
                         int y,
                         RuntimeMenu::Page page,
                         RuntimeSettingsMode mode,
                         const CoreOptionManager& core_options);

}  // namespace sekaiemu::spike
