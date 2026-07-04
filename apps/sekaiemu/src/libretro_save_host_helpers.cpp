#include "libretro_save_host_helpers.hpp"

#include <iostream>

namespace sekaiemu::spike {

void SaveBatteryNow(SaveStateManager& save_state_manager, CoreApi& core) {
  std::string error;
  if (save_state_manager.SaveBattery(core, error)) {
    save_state_manager.RefreshBatteryTracking(core);
    std::cerr << "[sekaiemu] battery saved: "
              << save_state_manager.BatteryPath() << "\n";
    return;
  }
  std::cerr << "[sekaiemu] battery save failed: " << error << "\n";
}

void LoadBatteryNow(SaveStateManager& save_state_manager, CoreApi& core) {
  std::string error;
  if (save_state_manager.LoadBattery(core, error)) {
    save_state_manager.RefreshBatteryTracking(core);
    std::cerr << "[sekaiemu] battery loaded: "
              << save_state_manager.BatteryPath() << "\n";
    return;
  }
  std::cerr << "[sekaiemu] battery load failed: " << error << "\n";
}

void SaveStateNow(SaveStateManager& save_state_manager, CoreApi& core, int slot) {
  std::string error;
  if (save_state_manager.SaveState(core, error, slot)) {
    std::cerr << "[sekaiemu] state saved: "
              << save_state_manager.StatePath(slot) << "\n";
    return;
  }
  std::cerr << "[sekaiemu] state save failed: " << error << "\n";
}

void LoadStateNow(SaveStateManager& save_state_manager, CoreApi& core, int slot) {
  std::string error;
  if (save_state_manager.LoadState(core, error, slot)) {
    save_state_manager.RefreshBatteryTracking(core);
    std::cerr << "[sekaiemu] state loaded: "
              << save_state_manager.StatePath(slot) << "\n";
    return;
  }
  std::cerr << "[sekaiemu] state load failed: " << error << "\n";
}

void TickBatteryPersistence(SaveStateManager& save_state_manager,
                            CoreApi& core,
                            bool probe_only,
                            std::uint64_t frame_counter) {
  if (probe_only) {
    return;
  }

  std::string error;
  if (save_state_manager.TickBatteryPersistence(core, frame_counter, error)) {
    std::cerr << "[sekaiemu] battery saved by trigger: "
              << save_state_manager.BatteryPath() << "\n";
  } else if (!error.empty() &&
             error != "No battery save memory is available.") {
    std::cerr << "[sekaiemu] battery trigger save failed: "
              << error << "\n";
  }
}

void SaveBatteryOnShutdown(SaveStateManager& save_state_manager,
                           CoreApi& core,
                           bool game_loaded_for_shutdown,
                           bool probe_only) {
  if (!game_loaded_for_shutdown || probe_only) {
    return;
  }

  std::string error;
  if (save_state_manager.FlushPendingBatterySave(core, error)) {
    std::cerr << "[sekaiemu] battery saved on shutdown flush: "
              << save_state_manager.BatteryPath() << "\n";
  } else if (!error.empty() && error != "No battery save memory is available.") {
    std::cerr << "[sekaiemu] battery save on shutdown skipped: "
              << error << "\n";
  }
}

}  // namespace sekaiemu::spike
