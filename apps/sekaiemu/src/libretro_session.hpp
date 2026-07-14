#pragma once

#include "audio_output.hpp"
#include "host_io_utils.hpp"
#include "libretro_core_api.hpp"
#include "libretro_host.hpp"
#include "save_state_manager.hpp"

#include <libretro.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace sekaiemu::spike {

bool InitializeFrontendSdl(bool probe_only, bool background_gamepad_input, std::string& error);
bool LoadCoreLibrarySession(const std::filesystem::path& core_path,
                            void*& core_handle,
                            CoreApi& core,
                            std::string& error);

struct LoadGameContentContext {
  HostOptions* options = nullptr;
  CoreApi* core = nullptr;
  retro_system_av_info* av_info = nullptr;
  std::vector<std::uint8_t>* game_data = nullptr;
  SaveStateManager* save_state_manager = nullptr;
  AudioOutput* audio_output = nullptr;
  bool* game_loaded_for_shutdown = nullptr;
  std::function<void()> on_probe_memory;
};

bool LoadGameContentSession(const LoadGameContentContext& context, std::string& error);

struct ShutdownSessionContext {
  HostOptions* options = nullptr;
  CoreApi* core = nullptr;
  void** core_handle = nullptr;
  bool* game_loaded_for_shutdown = nullptr;
  AudioOutput* audio_output = nullptr;
  std::function<void()> on_save_battery_on_shutdown;
  std::function<void()> on_shutdown_video_backend;
  std::function<void()> on_clear_active_host;
};

void ShutdownSession(const ShutdownSessionContext& context);

}  // namespace sekaiemu::spike
