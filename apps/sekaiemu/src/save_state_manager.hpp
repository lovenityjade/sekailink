#pragma once

#include "libretro_core_api.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

class SaveStateManager {
 public:
  static constexpr int kSaveSlotCount = 5;

  void Initialize(const std::filesystem::path& save_root,
                  const std::filesystem::path& game_path);

  bool SaveBattery(const CoreApi& core, std::string& error) const;
  bool LoadBattery(const CoreApi& core, std::string& error) const;
  bool SaveState(const CoreApi& core, std::string& error, int slot = 0) const;
  bool LoadState(const CoreApi& core, std::string& error, int slot = 0) const;
  bool WriteStateMetadata(int slot, std::string_view metadata, std::string& error) const;
  void InitializeBatteryTracking(const CoreApi& core);
  bool TickBatteryPersistence(const CoreApi& core,
                              std::uint64_t frame_counter,
                              std::string& error);
  void RefreshBatteryTracking(const CoreApi& core);
  bool FlushPendingBatterySave(const CoreApi& core, std::string& error);

  const std::filesystem::path& BatteryPath() const { return battery_path_; }
  const std::filesystem::path& StatePath() const { return state_path_; }
  std::filesystem::path StatePath(int slot) const;
  std::filesystem::path StateScreenshotPath(int slot) const;
  std::filesystem::path StateMetadataPath(int slot) const;

 private:
  std::string SlotSuffix(int slot) const;
  bool EnsureParentDirectory(const std::filesystem::path& path, std::string& error) const;
  bool SaveBatteryBytes(const std::uint8_t* data, std::size_t size, std::string& error) const;

  std::filesystem::path battery_path_;
  std::filesystem::path state_path_;
  std::filesystem::path state_directory_;
  std::string state_stem_;
  std::vector<std::uint8_t> battery_shadow_;
  std::size_t battery_size_ = 0;
  std::uint64_t last_battery_change_frame_ = 0;
  bool battery_tracking_ready_ = false;
  bool battery_dirty_ = false;
};

}  // namespace sekaiemu::spike
