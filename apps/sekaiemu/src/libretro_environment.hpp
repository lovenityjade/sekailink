#pragma once

#include "audio_output.hpp"
#include "core_option_manager.hpp"
#include "libretro_host.hpp"
#include "memory_domain_registry.hpp"
#include "video_backend.hpp"

#include <libretro.h>

#include <functional>
#include <string>

namespace sekaiemu::spike {

struct LibretroEnvironmentContext {
  HostOptions* options = nullptr;
  AudioOutput* audio_output = nullptr;
  CoreOptionManager* core_option_manager = nullptr;
  MemoryDomainRegistry* memory_domains = nullptr;
  retro_log_callback* log_callback = nullptr;
  std::string* system_directory_string = nullptr;
  std::string* save_directory_string = nullptr;
  retro_audio_buffer_status_callback_t* audio_buffer_status_callback = nullptr;
  retro_system_av_info* av_info = nullptr;
  retro_pixel_format* pixel_format = nullptr;
  retro_hw_render_callback* hw_render_callback = nullptr;
  bool* hw_render_requested = nullptr;
  retro_set_rumble_state_t rumble_callback = nullptr;

  std::function<VideoGeometry()> current_geometry;
  std::function<bool(const VideoGeometry&, std::string&)> prepare_hardware_video_backend;
  std::function<void()> maybe_update_video_backend_geometry;
  std::function<void(const std::string&)> trace;
};

bool HandleLibretroEnvironmentCommand(LibretroEnvironmentContext& context,
                                      unsigned cmd,
                                      void* data);

}  // namespace sekaiemu::spike
