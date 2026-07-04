#pragma once

#include "runtime_menu.hpp"
#include "save_state_manager.hpp"

#include <vector>

namespace sekaiemu::spike {

std::vector<SaveStateSlotMenuInfo> LoadSaveStateSlotMenuInfos(const SaveStateManager& save_state_manager);

}  // namespace sekaiemu::spike
