#include "libretro_host_internal.hpp"

#include "libretro_bridge_host_helpers.hpp"
#include "libretro_menu_presentation.hpp"
#include "libretro_memory_tools.hpp"
#include "libretro_tracker_host_state.hpp"
#include "profile_bridge.hpp"
#include "runtime_loop.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

int LibretroHost::Impl::Run() {
  if (!initialized) {
    last_error = "Host not initialized.";
    return 1;
  }

  if (options.probe_only) {
    return 0;
  }

  RuntimeLoopContext context{
      .options = &options,
      .core = &core,
      .runtime_menu = &runtime_menu,
      .core_option_manager = &core_option_manager,
      .input_state = &input_state,
      .audio_buffer_status_callback = &audio_buffer_status_callback,
      .frame_counter = &frame_counter,
      .target_frame_rate = av_info.timing.fps,
      .running = &running,
      .exit_code = &run_exit_code,
      .on_core_run = [this]() { core.retro_run(); },
      .on_tick_profile_bridge = [this]() { TickProfileBridge(); },
      .on_tick_sklmi_companion = [this]() {
        TickSklmiCompanion();
        TickTrackerRuntime();
      },
      .on_tick_memory_server = [this]() { TickRuntimeMemoryServer(); },
      .on_tick_battery_persistence = [this]() { TickBatteryPersistence(); },
      .on_dump_memory_snapshot = [this]() { DumpMemorySnapshot(); },
      .on_save_battery = [this]() { SaveBatteryNow(); },
      .on_load_battery = [this]() { LoadBatteryNow(); },
      .on_save_state = [this]() { SaveStateNow(); },
      .on_load_state = [this]() { LoadStateNow(); },
      .on_begin_frame = [this]() {
        ApplyFrameAutomation();
        TickCoreChatBridge();
      },
      .on_audio_buffer_status = [this]() {
        if (!audio_buffer_status_callback) {
          return;
        }
        const auto status = audio_output.CurrentBufferStatus(48000);
        audio_buffer_status_callback(status.active, status.occupancy_percent, status.underrun_likely);
      },
      .on_cycle_tracker_display_mode = [this]() { CycleTrackerDisplayMode(); },
      .on_toggle_tracker_screen = [this]() { ToggleTrackerScreen(); },
      .on_cycle_tracker_tab = [this]() { CycleTrackerTab(); },
      .on_toggle_tracker_auto_follow = [this]() { ToggleTrackerAutoFollow(); },
      .on_open_tracker_map_menu = [this]() { OpenTrackerMapMenu(); },
      .on_open_tracker_map_menu_at = [this](int x, int y) { return OpenTrackerMapMenuAt(x, y); },
      .on_activate_tracker_map_menu = [this]() { return ActivateTrackerMapMenu(); },
      .on_activate_tracker_map_menu_at = [this](int x, int y) { return ActivateTrackerMapMenuAt(x, y); },
      .on_hover_tracker_map_menu_at = [this](int x, int y) { return HoverTrackerMapMenuAt(x, y); },
      .on_step_tracker_map_menu = [this](int delta) { StepTrackerMapMenu(delta); },
      .on_close_tracker_map_menu = [this]() { CloseTrackerMapMenu(); },
      .tracker_map_menu_visible = [this]() { return tracker_runtime_.UiState().map_context_menu_visible; },
      .chat_overlay_enabled = [this]() { return chat_overlay_.Enabled(); },
      .chat_typing_active = [this]() { return chat_overlay_.Typing(); },
      .on_open_chat_input = [this]() { OpenChatInput(); },
      .on_cancel_chat_input = [this]() { CancelChatInput(); },
      .on_submit_chat_input = [this]() { SubmitChatInput(); },
      .on_backspace_chat_input = [this]() { BackspaceChatInput(); },
      .on_append_chat_input = [this](std::string_view text) { AppendChatInput(text); },
      .on_present_frame = [this]() { PresentFrame(); },
      .on_update_menu_overlay = [this]() { UpdateMenuOverlay(); },
      .on_reset_core = [this]() { core.retro_reset(); },
      .on_apply_menu_action = [this](RuntimeMenuAction action) { ApplyMenuAction(action); },
      .on_menu_visibility_changed = [this](bool visible) {
        if (video_backend) {
          video_backend->SetMenuVisible(visible, CurrentVideoGeometry());
          if (!visible) {
            video_backend->ClearOverlay();
          }
        }
        if (!visible) {
          tracker_last_render_frame_ = 0;
          tracker_force_next_render_ = true;
        }
      },
  };
  return RunRuntimeLoop(context);
}

void LibretroHost::Impl::PresentFrame() {
  PresentFrameForHost(video_backend.get(),
                      tracker_window_presenter_,
                      runtime_menu.Visible(),
                      frame_ready);
  bridge_terminal_presenter_.Present();
}

void LibretroHost::Impl::UpdateMenuOverlay() {
  UpdateMenuOverlayForHost(video_backend.get(),
                           runtime_menu,
                           core_option_manager,
                           input_state,
                           CurrentBridgeRuntimeStatus(),
                           tracker_active_ ? &tracker_runtime_ : nullptr,
                           save_state_manager,
                           CurrentVideoGeometry(),
                           system_info.library_name ? system_info.library_name : "Unknown Core",
                           options.game_path.stem().string(),
                           tracker_window_presenter_,
                           [this]() { RenderTrackerPresentation(); },
                           audio_output.MasterVolumePercent(),
                           chat_overlay_.Enabled(),
                           notifications_enabled_,
                           bridge_terminal_presenter_.Enabled());
  RenderBridgeTerminal();
}

void LibretroHost::Impl::ProbeMemory() {
  ProbeMemoryRegions(core, memory_domains);
}

void LibretroHost::Impl::TickProfileBridge() {
  if (bridge_owner == BridgeOwner::Sklmi) {
    return;
  }
  if (!profile_bridge.Active()) {
    return;
  }
  profile_bridge.Tick(ProfileBridgeCallbacks{
      .read_region = [this](const WatchRegion& region) { return ReadProfileRegion(region); },
      .resolve_mutable = [this](std::string_view domain, std::uint32_t start, std::uint32_t length) {
        return ResolveMutableMemorySource(domain, start, length);
      },
      .trace = [](const std::string& line) { std::cerr << line << "\n"; },
  });
}

void LibretroHost::Impl::TickRuntimeMemoryServer() {
  runtime_memory_server.Poll();
}

void LibretroHost::Impl::TickSklmiCompanion() {
  const auto previous_error = bridge_runtime_last_error;
  TickSklmiCompanionForHost(sklmi_companion_runtime, bridge_runtime_last_error);
  if (tracker_active_ && !bridge_runtime_last_error.empty() &&
      bridge_runtime_last_error != previous_error) {
    tracker_runtime_.ApplyServerSnapshot(BuildTrackerErrorSnapshot(
        "sklmi_runtime_failed",
        bridge_runtime_last_error));
    tracker_dirty_ = true;
    tracker_force_next_render_ = true;
  }
}

void LibretroHost::Impl::TickTrackerRuntime() {
  const auto previous_mutation_serial = tracker_runtime_.MutationSerial();
  TickTrackerRuntimeForHost(tracker_runtime_,
                            tracker_active_,
                            tracker_snapshot_path_,
                            tracker_snapshot_reader_,
                            sklmi_companion_runtime,
                            tracker_room_state_last_write_time_,
                            tracker_trace_offset_,
                            tracker_item_label_by_key_,
                            frame_counter,
                            tracker_last_poll_frame_,
                            tracker_last_mutation_serial_,
                            tracker_last_save_frame_,
                            tracker_dirty_,
                            tracker_state_path_,
                            kTrackerPollFrameInterval,
                            kTrackerSnapshotPollFrameInterval,
                            kTrackerAutosaveFrameInterval);
  if (tracker_runtime_.MutationSerial() != previous_mutation_serial) {
    RefreshTrackerAssetResolverForHost(options,
                                       tracker_bundle_,
                                       &tracker_runtime_,
                                       tracker_asset_resolver_);
  }
}

void LibretroHost::Impl::TickCoreChatBridge() {
  if (!core_chat_bridge_.Active()) {
    return;
  }
  core_chat_bridge_.Tick(frame_counter);
  for (const auto& message : core_chat_bridge_.DrainIncoming()) {
    chat_overlay_.AddExternalMessage(message.id, message.author, message.text, frame_counter);
  }
}

BridgeRuntimeStatus LibretroHost::Impl::CurrentBridgeRuntimeStatus() const {
  return BuildBridgeRuntimeStatusForHost(options,
                                         launch_profile,
                                         bridge_owner,
                                         active_sklmi_bridge_spec,
                                         profile_bridge,
                                         runtime_memory_server,
                                         sklmi_companion_runtime,
                                         bridge_runtime_last_error);
}

void LibretroHost::Impl::DumpMemorySnapshot() {
  sekaiemu::spike::DumpMemorySnapshot(core, options.save_directory, dump_counter);
}

std::vector<std::uint8_t> LibretroHost::Impl::ReadProfileRegion(const WatchRegion& region) {
  const auto* source = ResolveMemorySource(region.memory_domain, region.start, region.length);
  if (!source) {
    return {};
  }
  return std::vector<std::uint8_t>(source, source + region.length);
}

const std::uint8_t* LibretroHost::Impl::ResolveMemorySource(std::string_view memory_domain,
                                                            std::uint32_t start,
                                                            std::uint32_t length) const {
  return memory_domains.Resolve(core, memory_domain, start, length);
}

std::uint8_t* LibretroHost::Impl::ResolveMutableMemorySource(std::string_view memory_domain,
                                                             std::uint32_t start,
                                                             std::uint32_t length) {
  return memory_domains.ResolveMutable(core, memory_domain, start, length);
}

}  // namespace sekaiemu::spike
