#pragma once

#include "core_option_manager.hpp"
#include "runtime_menu.hpp"

#include <functional>

namespace sekaiemu::spike {

void ApplyRuntimeMenuAction(RuntimeMenuAction action,
                            RuntimeMenu& runtime_menu,
                            CoreOptionManager& core_option_manager,
                            bool& running,
                            const std::function<void()>& reset_core,
                            const std::function<void()>& save_battery,
                            const std::function<void()>& load_battery,
                            const std::function<void(int)>& save_state,
                            const std::function<void(int)>& load_state,
                            const std::function<void()>& restart_bridge,
                            const std::function<void()>& toggle_chat_overlay);

}  // namespace sekaiemu::spike
