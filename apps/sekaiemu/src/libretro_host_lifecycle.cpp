#include "libretro_host_internal.hpp"

#include "libretro_bridge_host_helpers.hpp"
#include "libretro_core_loader.hpp"
#include "libretro_core_utils.hpp"
#include "libretro_frame_automation.hpp"
#include "libretro_frame_dump.hpp"
#include "libretro_session.hpp"
#include "libretro_tracker_host_state.hpp"
#include "libretro_video_host_helpers.hpp"
#include "memory_profile.hpp"

#include <cstring>
#include <iostream>
#include <utility>

namespace sekaiemu::spike {

LibretroHost::Impl::Impl(HostOptions host_options)
    : options(std::move(host_options)) {}

LibretroHost::Impl::~Impl() { Shutdown(); }

bool LibretroHost::Impl::Initialize() {
  if (!options.probe_only && !InitializeSdl()) {
    return false;
  }
  if (!LoadCore()) {
    return false;
  }
  if (!InitializeCore()) {
    return false;
  }
  if (!InitializeInputScript()) {
    return false;
  }
  if (!LoadContent()) {
    return false;
  }
  if (!ApplyStartupStateActions()) {
    return false;
  }
  if (!LoadLaunchProfileMetadata()) {
    return false;
  }
  if (!InitializeTrackerRuntime()) {
    return false;
  }
  InitializeChatBridge();
  if (!InitializeRuntimeMemoryServer()) {
    return false;
  }
  if (!InitializeSklmiCompanion()) {
    return false;
  }
  if (!InitializeProfileBridge()) {
    return false;
  }
  if (!options.probe_only && !InitializeVideoBackend()) {
    return false;
  }
  initialized = true;
  return true;
}

std::string LibretroHost::Impl::LastError() const { return last_error; }

bool LibretroHost::Impl::InitializeSdl() {
  return InitializeFrontendSdl(options.probe_only, last_error);
}

bool LibretroHost::Impl::LoadCore() {
  return LoadCoreLibrarySession(options.core_path, core_handle, core, last_error);
}

bool LibretroHost::Impl::InitializeCore() {
  active_host = this;
  core_option_manager.Initialize(options.save_directory, options.core_path, options.game_path);
  input_state.Initialize(options.save_directory, options.core_path, options.game_path);
  LoadFrontendSettings();

  retro_log_callback log_callback{&Impl::LogPrintf};
  log_callback_ = log_callback;

  environment_context = LibretroEnvironmentContext{
      .options = &options,
      .audio_output = &audio_output,
      .core_option_manager = &core_option_manager,
      .memory_domains = &memory_domains,
      .log_callback = &log_callback_,
      .system_directory_string = &system_directory_string,
      .save_directory_string = &save_directory_string,
      .audio_buffer_status_callback = &audio_buffer_status_callback,
      .av_info = &av_info,
      .pixel_format = &pixel_format,
      .hw_render_callback = &hw_render_callback,
      .hw_render_requested = &hw_render_requested,
      .rumble_callback = &Impl::SetRumbleState,
      .current_geometry = [this]() { return CurrentVideoGeometry(); },
      .prepare_hardware_video_backend = [this](const VideoGeometry& geometry, std::string& error) {
        return PrepareHardwareVideoBackend(geometry, error);
      },
      .maybe_update_video_backend_geometry = [this]() { MaybeUpdateVideoBackendGeometry(); },
      .trace = [](const std::string& line) { std::cerr << line << "\n"; },
  };

  core.retro_set_environment(&Impl::EnvironmentCallback);
  core.retro_set_video_refresh(&Impl::VideoRefreshCallback);
  core.retro_set_audio_sample(&Impl::AudioSampleCallback);
  core.retro_set_audio_sample_batch(&Impl::AudioSampleBatchCallback);
  core.retro_set_input_poll(&Impl::InputPollCallback);
  core.retro_set_input_state(&Impl::InputStateCallback);
  core.retro_init();
  if (core.retro_set_controller_port_device) {
    const auto core_path_name = Lowercase(options.core_path.filename().string());
    if (core_path_name.find("fceumm") == std::string::npos &&
        core_path_name.find("fceu") == std::string::npos) {
      core.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    }
  }

  std::memset(&system_info, 0, sizeof(system_info));
  core.retro_get_system_info(&system_info);

  std::cerr << "[sekaiemu-libretro-spike] core loaded: "
            << (system_info.library_name ? system_info.library_name : "unknown")
            << " "
            << (system_info.library_version ? system_info.library_version : "unknown")
            << "\n";
  return true;
}

bool LibretroHost::Impl::LoadContent() {
  return LoadGameContentSession(LoadGameContentContext{
                                    .options = &options,
                                    .core = &core,
                                    .av_info = &av_info,
                                    .game_data = &game_data,
                                    .save_state_manager = &save_state_manager,
                                    .audio_output = &audio_output,
                                    .game_loaded_for_shutdown = &game_loaded_for_shutdown,
                                    .on_probe_memory = [this]() {
                                      std::cerr << "[sekaiemu-libretro-spike] pixel format: "
                                                << PixelFormatName(pixel_format) << "\n";
                                      if (!memory_domains.Descriptors().empty()) {
                                        std::cerr << "[sekaiemu-libretro-spike] memory descriptors: "
                                                  << memory_domains.Descriptors().size() << "\n";
                                      }
                                      ProbeMemory();
                                    },
                                },
                                last_error);
}

bool LibretroHost::Impl::InitializeVideoBackend() {
  std::string error;
  if (!InitializeVideoBackendForHost(hw_render_requested,
                                     hw_render_callback,
                                     hw_render_context_ready,
                                     video_backend,
                                     CurrentVideoGeometry(),
                                     error)) {
    last_error = error;
    return false;
  }
  return true;
}

bool LibretroHost::Impl::LoadLaunchProfileMetadata() {
  if (options.profile_path.empty()) {
    return true;
  }

  std::string error;
  MemoryProfile profile;
  if (!LoadMemoryProfile(options.profile_path, profile, error)) {
    last_error = error;
    return false;
  }
  launch_profile = std::move(profile);
  return true;
}

bool LibretroHost::Impl::InitializeTrackerRuntime() {
  const bool ok = InitializeTrackerRuntimeForHost(options,
                                                  sklmi_companion_runtime,
                                                  tracker_bundle_,
                                                  tracker_runtime_,
                                                  tracker_asset_resolver_,
                                                  tracker_state_path_,
                                                  tracker_snapshot_path_,
                                                  tracker_command_log_path_,
                                                  tracker_last_mutation_serial_,
                                                  tracker_dirty_,
                                                  tracker_active_,
                                                  last_error);
  if (ok) {
    ApplyFrontendTrackerSettings();
  }
  return ok;
}

void LibretroHost::Impl::InitializeChatBridge() {
  core_chat_bridge_.Configure(options.chat_inbox_path, options.chat_outbox_path);
}

bool LibretroHost::Impl::InitializeInputScript() {
  return InitializeInputScriptForHost(options.input_script_path, input_script, last_error);
}

void LibretroHost::Impl::ApplyInputScriptFrame() {
  ApplyInputScriptFrameForHost(input_script,
                               options.input_script_quit_after_end,
                               frame_counter,
                               input_state,
                               running);
}

bool LibretroHost::Impl::ApplyStartupStateActions() {
  return ApplyStartupStateActionsForHost(options.load_state_on_start,
                                         save_state_manager,
                                         core,
                                         last_error);
}

void LibretroHost::Impl::ApplyFrameAutomation() {
  ApplyFrameAutomationForHost(frame_counter,
                              options.save_state_at_frame,
                              options.quit_after_frame,
                              scheduled_state_saved_,
                              running,
                              [this]() { ApplyInputScriptFrame(); },
                              [this]() { SaveStateNow(); });
}

void LibretroHost::Impl::MaybeDumpFrame(const void* data,
                                        unsigned width,
                                        unsigned height,
                                        std::size_t pitch) {
  sekaiemu::spike::MaybeDumpFrame(data,
                                  width,
                                  height,
                                  pitch,
                                  pixel_format,
                                  options.dump_frame_path,
                                  options.save_directory,
                                  frame_counter,
                                  options.dump_frame_at_frame,
                                  frame_dump_written_);
  if (!pending_state_screenshot_slot_) {
    return;
  }

  const int slot = *pending_state_screenshot_slot_;
  const auto screenshot_path = save_state_manager.StateScreenshotPath(slot);
  std::string error;
  if (WriteFramePpm(screenshot_path, data, width, height, pitch, pixel_format, error)) {
    pending_state_metadata_["screenshot_path"] = screenshot_path.string();
    if (!save_state_manager.WriteStateMetadata(slot, pending_state_metadata_.dump(2), error)) {
      std::cerr << "[sekaiemu] state metadata write failed: " << error << "\n";
    } else {
      std::cerr << "[sekaiemu] state screenshot saved: " << screenshot_path << "\n";
    }
  } else {
    std::cerr << "[sekaiemu] state screenshot failed: " << error << "\n";
  }
  pending_state_screenshot_slot_.reset();
  pending_state_metadata_ = nlohmann::json::object();
}

bool LibretroHost::Impl::InitializeProfileBridge() {
  return InitializeProfileBridgeForHost(options, bridge_owner, profile_bridge, last_error);
}

bool LibretroHost::Impl::InitializeSklmiCompanion() {
  return InitializeSklmiCompanionForHost(options,
                                         launch_profile,
                                         runtime_memory_server,
                                         sklmi_companion_runtime,
                                         active_sklmi_bridge_spec,
                                         resolved_sklmi_runtime_binary,
                                         resolved_sklmi_manifest_directory,
                                         bridge_owner,
                                         bridge_runtime_last_error,
                                         last_error,
                                         tracker_active_,
                                         tracker_snapshot_path_,
                                         tracker_command_log_path_,
                                         tracker_snapshot_reader_);
}

bool LibretroHost::Impl::InitializeRuntimeMemoryServer() {
  return InitializeRuntimeMemoryServerForHost(options,
                                              core,
                                              memory_domains,
                                              runtime_memory_server,
                                              last_error);
}

void LibretroHost::Impl::Shutdown() {
  SaveFrontendSettingsNow();
  SaveTrackerState("shutdown");
  ShutdownSession(ShutdownSessionContext{
      .options = &options,
      .core = &core,
      .core_handle = &core_handle,
      .game_loaded_for_shutdown = &game_loaded_for_shutdown,
      .audio_output = &audio_output,
      .on_save_battery_on_shutdown = [this]() { SaveBatteryOnShutdown(); },
      .on_shutdown_video_backend = [this]() {
        tracker_window_presenter_.Shutdown();
        if (video_backend) {
          video_backend->Shutdown();
          video_backend.reset();
        }
      },
      .on_clear_active_host = [this]() {
        sklmi_companion_runtime.Shutdown();
        runtime_memory_server.Shutdown();
        active_host = nullptr;
      },
  });
}

}  // namespace sekaiemu::spike
