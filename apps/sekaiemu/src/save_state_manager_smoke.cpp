#include "save_state_manager.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::array<std::uint8_t, 4> g_battery{{0x10, 0x20, 0x30, 0x40}};
int g_memory_data_calls = 0;
int g_memory_size_calls = 0;

void* FakeMemoryData(unsigned id) {
  ++g_memory_data_calls;
  if (id == RETRO_MEMORY_SAVE_RAM) {
    return g_battery.data();
  }
  return nullptr;
}

std::size_t FakeMemorySize(unsigned id) {
  ++g_memory_size_calls;
  if (id == RETRO_MEMORY_SAVE_RAM) {
    return g_battery.size();
  }
  return 0;
}

}  // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "sekaiemu-save-state-manager-smoke";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);

  sekaiemu::spike::CoreApi core;
  core.retro_get_memory_data = &FakeMemoryData;
  core.retro_get_memory_size = &FakeMemorySize;

  sekaiemu::spike::SaveStateManager manager;
  manager.Initialize(root, "game.sfc");
  manager.InitializeBatteryTracking(core);

  if (g_memory_size_calls != 1) {
    std::cerr << "save_state_manager_initial_size_calls=" << g_memory_size_calls << "\n";
    return 1;
  }

  std::string error;
  for (std::uint64_t frame = 1; frame <= 20; ++frame) {
    manager.TickBatteryPersistence(core, frame, error);
  }
  if (g_memory_size_calls != 1) {
    std::cerr << "save_state_manager_tick_requeried_size=" << g_memory_size_calls << "\n";
    return 1;
  }

  g_battery[0] = 0x99;
  manager.TickBatteryPersistence(core, 21, error);
  for (std::uint64_t frame = 22; frame <= 142; ++frame) {
    manager.TickBatteryPersistence(core, frame, error);
  }

  if (g_memory_size_calls != 1) {
    std::cerr << "save_state_manager_dirty_tick_requeried_size=" << g_memory_size_calls << "\n";
    return 1;
  }

  if (!fs::exists(manager.BatteryPath())) {
    std::cerr << "save_state_manager_battery_not_saved\n";
    return 1;
  }
  if (manager.StatePath(0) != manager.StatePath() ||
      manager.StatePath(1).filename().string().find(".slot1.state") == std::string::npos) {
    std::cerr << "save_state_manager_slot_paths_failed\n";
    return 1;
  }
  if (!manager.WriteStateMetadata(1, "{\"slot\":1}", error) ||
      !fs::exists(manager.StateMetadataPath(1))) {
    std::cerr << "save_state_manager_slot_metadata_failed\n";
    return 1;
  }

  std::cout << "save_state_manager_smoke_ok data_calls=" << g_memory_data_calls << "\n";
  return 0;
}
