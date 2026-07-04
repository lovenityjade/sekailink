#include "save_state_manager.hpp"

#include "host_io_utils.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace sekaiemu::spike {

namespace {

constexpr std::uint64_t kBatterySaveDebounceFrames = 120;

}

void SaveStateManager::Initialize(const std::filesystem::path& save_root,
                                  const std::filesystem::path& game_path) {
  state_stem_ = game_path.stem().string();
  state_directory_ = save_root / "states";
  battery_path_ = save_root / "battery" / (state_stem_ + ".sav");
  state_path_ = state_directory_ / (state_stem_ + ".state");
}

bool SaveStateManager::SaveBattery(const CoreApi& core, std::string& error) const {
  if (!core.retro_get_memory_data || !core.retro_get_memory_size) {
    error = "Core does not expose battery save memory.";
    return false;
  }

  void* data = core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
  const std::size_t size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!data || size == 0) {
    error = "No battery save memory is available.";
    return false;
  }

  return SaveBatteryBytes(static_cast<const std::uint8_t*>(data), size, error);
}

bool SaveStateManager::LoadBattery(const CoreApi& core, std::string& error) const {
  if (!core.retro_get_memory_data || !core.retro_get_memory_size) {
    error = "Core does not expose battery save memory.";
    return false;
  }

  void* data = core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
  const std::size_t size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!data || size == 0) {
    error = "No battery save memory is available.";
    return false;
  }

  if (!std::filesystem::exists(battery_path_)) {
    error = "No battery save file exists yet.";
    return false;
  }

  const auto bytes = ReadWholeFile(battery_path_);
  if (bytes.empty()) {
    error = "Failed to read battery save file.";
    return false;
  }
  if (bytes.size() != size) {
    std::memset(data, 0xFF, size);
    std::memcpy(data, bytes.data(), std::min<std::size_t>(bytes.size(), size));
    return true;
  }

  std::memcpy(data, bytes.data(), size);
  return true;
}

bool SaveStateManager::SaveState(const CoreApi& core, std::string& error, int slot) const {
  if (!core.retro_serialize_size || !core.retro_serialize) {
    error = "Core does not support savestates.";
    return false;
  }

  const std::size_t state_size = core.retro_serialize_size();
  if (state_size == 0) {
    error = "Core reported an empty savestate size.";
    return false;
  }

  std::vector<std::uint8_t> state(state_size);
  if (!core.retro_serialize(state.data(), state.size())) {
    error = "Core failed to serialize state.";
    return false;
  }

  const auto path = StatePath(slot);
  if (!EnsureParentDirectory(path, error)) {
    return false;
  }

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "Failed to open savestate file for writing.";
    return false;
  }
  stream.write(reinterpret_cast<const char*>(state.data()),
               static_cast<std::streamsize>(state.size()));
  if (!stream) {
    error = "Failed to write savestate file.";
    return false;
  }
  return true;
}

bool SaveStateManager::LoadState(const CoreApi& core, std::string& error, int slot) const {
  if (!core.retro_serialize_size || !core.retro_unserialize) {
    error = "Core does not support savestates.";
    return false;
  }

  const auto path = StatePath(slot);
  if (!std::filesystem::exists(path)) {
    error = "No savestate file exists yet.";
    return false;
  }

  const auto bytes = ReadWholeFile(path);
  if (bytes.empty()) {
    error = "Failed to read savestate file.";
    return false;
  }

  if (!core.retro_unserialize(bytes.data(), bytes.size())) {
    error = "Core failed to load savestate data.";
    return false;
  }
  return true;
}

bool SaveStateManager::WriteStateMetadata(int slot, std::string_view metadata, std::string& error) const {
  const auto path = StateMetadataPath(slot);
  if (!EnsureParentDirectory(path, error)) {
    return false;
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "Failed to open savestate metadata file for writing.";
    return false;
  }
  stream << metadata << "\n";
  if (!stream) {
    error = "Failed to write savestate metadata file.";
    return false;
  }
  return true;
}

void SaveStateManager::InitializeBatteryTracking(const CoreApi& core) {
  battery_shadow_.clear();
  battery_size_ = 0;
  last_battery_change_frame_ = 0;
  battery_tracking_ready_ = false;
  battery_dirty_ = false;

  if (!core.retro_get_memory_data || !core.retro_get_memory_size) {
    return;
  }

  auto* data = static_cast<const std::uint8_t*>(core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
  const std::size_t size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!data || size == 0) {
    return;
  }

  battery_shadow_.assign(data, data + size);
  battery_size_ = size;
  battery_tracking_ready_ = true;
}

bool SaveStateManager::TickBatteryPersistence(const CoreApi& core,
                                              std::uint64_t frame_counter,
                                              std::string& error) {
  if (!battery_tracking_ready_) {
    InitializeBatteryTracking(core);
    return false;
  }

  auto* data = static_cast<const std::uint8_t*>(core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
  const std::size_t size = battery_size_;
  if (!data || size == 0) {
    return false;
  }

  if (battery_shadow_.size() != size) {
    battery_shadow_.assign(data, data + size);
    battery_dirty_ = false;
    last_battery_change_frame_ = frame_counter;
    return false;
  }

  if (!std::equal(battery_shadow_.begin(), battery_shadow_.end(), data)) {
    battery_shadow_.assign(data, data + size);
    battery_dirty_ = true;
    last_battery_change_frame_ = frame_counter;
    return false;
  }

  if (!battery_dirty_) {
    return false;
  }

  if (frame_counter - last_battery_change_frame_ < kBatterySaveDebounceFrames) {
    return false;
  }

  if (!SaveBatteryBytes(battery_shadow_.data(), battery_shadow_.size(), error)) {
    return false;
  }

  battery_dirty_ = false;
  return true;
}

void SaveStateManager::RefreshBatteryTracking(const CoreApi& core) {
  InitializeBatteryTracking(core);
}

bool SaveStateManager::FlushPendingBatterySave(const CoreApi& core, std::string& error) {
  if (!battery_tracking_ready_) {
    InitializeBatteryTracking(core);
  }

  auto* data = static_cast<const std::uint8_t*>(core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
  const std::size_t size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!data || size == 0) {
    error = "No battery save memory is available.";
    return false;
  }

  if (battery_shadow_.size() != size || !std::equal(battery_shadow_.begin(), battery_shadow_.end(), data)) {
    battery_shadow_.assign(data, data + size);
    battery_dirty_ = true;
  }

  if (!battery_dirty_) {
    return false;
  }

  if (!SaveBatteryBytes(battery_shadow_.data(), battery_shadow_.size(), error)) {
    return false;
  }

  battery_dirty_ = false;
  return true;
}

std::filesystem::path SaveStateManager::StatePath(int slot) const {
  if (slot <= 0) {
    return state_path_;
  }
  return state_directory_ / (state_stem_ + SlotSuffix(slot) + ".state");
}

std::filesystem::path SaveStateManager::StateScreenshotPath(int slot) const {
  return state_directory_ / (state_stem_ + SlotSuffix(slot) + ".ppm");
}

std::filesystem::path SaveStateManager::StateMetadataPath(int slot) const {
  return state_directory_ / (state_stem_ + SlotSuffix(slot) + ".json");
}

std::string SaveStateManager::SlotSuffix(int slot) const {
  if (slot <= 0) {
    return ".autosave";
  }
  if (slot >= kSaveSlotCount) {
    slot = kSaveSlotCount - 1;
  }
  return ".slot" + std::to_string(slot);
}

bool SaveStateManager::EnsureParentDirectory(const std::filesystem::path& path,
                                             std::string& error) const {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "Failed to create save directory: " + ec.message();
    return false;
  }
  return true;
}

bool SaveStateManager::SaveBatteryBytes(const std::uint8_t* data,
                                        std::size_t size,
                                        std::string& error) const {
  if (!EnsureParentDirectory(battery_path_, error)) {
    return false;
  }

  std::ofstream stream(battery_path_, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "Failed to open battery save file for writing.";
    return false;
  }

  stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  if (!stream) {
    error = "Failed to write battery save file.";
    return false;
  }
  return true;
}

}  // namespace sekaiemu::spike
