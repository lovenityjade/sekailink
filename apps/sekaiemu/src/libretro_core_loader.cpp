#include "libretro_core_loader.hpp"

#include "libretro_core_utils.hpp"

#include <dlfcn.h>
#include <libretro.h>

namespace sekaiemu::spike {

bool LoadLibretroCore(const std::filesystem::path& core_path,
                      void*& core_handle,
                      CoreApi& api,
                      std::string& error) {
  core_handle = dlopen(core_path.c_str(), RTLD_NOW);
  if (!core_handle) {
    error = std::string("dlopen failed: ") + dlerror();
    return false;
  }

  const bool ok =
      ResolveSymbol(core_handle, "retro_init", api.retro_init) &&
      ResolveSymbol(core_handle, "retro_deinit", api.retro_deinit) &&
      ResolveSymbol(core_handle, "retro_api_version", api.retro_api_version) &&
      ResolveSymbol(core_handle, "retro_get_system_info", api.retro_get_system_info) &&
      ResolveSymbol(core_handle, "retro_get_system_av_info", api.retro_get_system_av_info) &&
      ResolveSymbol(core_handle, "retro_set_environment", api.retro_set_environment) &&
      ResolveSymbol(core_handle, "retro_set_video_refresh", api.retro_set_video_refresh) &&
      ResolveSymbol(core_handle, "retro_set_audio_sample", api.retro_set_audio_sample) &&
      ResolveSymbol(core_handle, "retro_set_audio_sample_batch", api.retro_set_audio_sample_batch) &&
      ResolveSymbol(core_handle, "retro_set_input_poll", api.retro_set_input_poll) &&
      ResolveSymbol(core_handle, "retro_set_input_state", api.retro_set_input_state) &&
      ResolveSymbol(core_handle, "retro_set_controller_port_device", api.retro_set_controller_port_device) &&
      ResolveSymbol(core_handle, "retro_reset", api.retro_reset) &&
      ResolveSymbol(core_handle, "retro_run", api.retro_run) &&
      ResolveSymbol(core_handle, "retro_load_game", api.retro_load_game) &&
      ResolveSymbol(core_handle, "retro_unload_game", api.retro_unload_game) &&
      ResolveSymbol(core_handle, "retro_get_memory_data", api.retro_get_memory_data) &&
      ResolveSymbol(core_handle, "retro_get_memory_size", api.retro_get_memory_size);

  if (!ok) {
    error = "Failed to resolve required libretro symbols.";
    dlclose(core_handle);
    core_handle = nullptr;
    return false;
  }

  if (api.retro_api_version() != RETRO_API_VERSION) {
    error = "libretro API version mismatch.";
    dlclose(core_handle);
    core_handle = nullptr;
    return false;
  }

  ResolveSymbol(core_handle, "retro_serialize_size", api.retro_serialize_size);
  ResolveSymbol(core_handle, "retro_serialize", api.retro_serialize);
  ResolveSymbol(core_handle, "retro_unserialize", api.retro_unserialize);
  ResolveSymbol(core_handle, "retro_cheat_reset", api.retro_cheat_reset);
  ResolveSymbol(core_handle, "retro_cheat_set", api.retro_cheat_set);
  ResolveSymbol(core_handle, "retro_load_game_special", api.retro_load_game_special);
  ResolveSymbol(core_handle, "retro_get_region", api.retro_get_region);

  return true;
}

void UnloadLibretroCore(void*& core_handle) {
  if (core_handle) {
    dlclose(core_handle);
    core_handle = nullptr;
  }
}

}  // namespace sekaiemu::spike
