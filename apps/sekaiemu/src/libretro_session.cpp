#include "libretro_session.hpp"

#include "libretro_core_loader.hpp"

#include <SDL.h>

#include <cstring>
#include <iostream>

namespace sekaiemu::spike {

bool InitializeFrontendSdl(bool probe_only, std::string& error) {
  if (probe_only) {
    return true;
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS |
               SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
    error = std::string("SDL_Init failed: ") + SDL_GetError();
    return false;
  }
  return true;
}

bool LoadCoreLibrarySession(const std::filesystem::path& core_path,
                            void*& core_handle,
                            CoreApi& core,
                            std::string& error) {
  return LoadLibretroCore(core_path, core_handle, core, error);
}

bool LoadGameContentSession(const LoadGameContentContext& context, std::string& error) {
  *context.game_data = ReadWholeFile(context.options->game_path);
  if (context.game_data->empty()) {
    error = "Failed to read game content: " + context.options->game_path.string();
    return false;
  }

  retro_game_info info{};
  std::string game_path_string = context.options->game_path.string();
  info.path = game_path_string.c_str();
  info.data = context.game_data->data();
  info.size = context.game_data->size();
  info.meta = nullptr;

  if (!context.core->retro_load_game(&info)) {
    error = "retro_load_game returned false.";
    return false;
  }

  std::memset(context.av_info, 0, sizeof(*context.av_info));
  context.core->retro_get_system_av_info(context.av_info);
  *context.game_loaded_for_shutdown = true;
  std::cerr << "[sekaiemu-libretro-spike] game loaded: " << context.options->game_path << "\n";
  std::cerr << "[sekaiemu-libretro-spike] geometry: "
            << context.av_info->geometry.base_width << "x" << context.av_info->geometry.base_height
            << ", fps=" << context.av_info->timing.fps
            << ", sample_rate=" << context.av_info->timing.sample_rate << "\n";
  if (context.on_probe_memory) {
    context.on_probe_memory();
  }

  context.save_state_manager->Initialize(context.options->save_directory, context.options->game_path);

  if (!context.options->probe_only) {
    if (!context.audio_output->Initialize(context.av_info->timing.sample_rate,
                                          context.av_info->timing.fps,
                                          error)) {
      return false;
    }
  }

  context.save_state_manager->InitializeBatteryTracking(*context.core);
  return true;
}

void ShutdownSession(const ShutdownSessionContext& context) {
  if (context.on_save_battery_on_shutdown) {
    context.on_save_battery_on_shutdown();
  }

  if (context.core->retro_unload_game && *context.game_loaded_for_shutdown) {
    context.core->retro_unload_game();
    *context.game_loaded_for_shutdown = false;
  }

  if (context.core->retro_deinit && *context.core_handle) {
    context.core->retro_deinit();
  }

  if (*context.core_handle) {
    UnloadLibretroCore(*context.core_handle);
    *context.core_handle = nullptr;
  }

  if (context.on_shutdown_video_backend) {
    context.on_shutdown_video_backend();
  }
  context.audio_output->Shutdown();

  if (!context.options->probe_only) {
    SDL_Quit();
  }

  if (context.on_clear_active_host) {
    context.on_clear_active_host();
  }
}

}  // namespace sekaiemu::spike
