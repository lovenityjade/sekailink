#pragma once

#include "libretro_core_api.hpp"
#include "save_state_manager.hpp"

#include <cstdint>

namespace sekaiemu::spike {

void SaveBatteryNow(SaveStateManager& save_state_manager, CoreApi& core);
void LoadBatteryNow(SaveStateManager& save_state_manager, CoreApi& core);
void SaveStateNow(SaveStateManager& save_state_manager, CoreApi& core, int slot = 0);
void LoadStateNow(SaveStateManager& save_state_manager, CoreApi& core, int slot = 0);
void TickBatteryPersistence(SaveStateManager& save_state_manager,
                            CoreApi& core,
                            bool probe_only,
                            std::uint64_t frame_counter);
void SaveBatteryOnShutdown(SaveStateManager& save_state_manager,
                           CoreApi& core,
                           bool game_loaded_for_shutdown,
                           bool probe_only);

}  // namespace sekaiemu::spike
