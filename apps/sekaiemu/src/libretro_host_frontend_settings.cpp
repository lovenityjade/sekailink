#include "libretro_host_internal.hpp"

#include <iostream>

namespace sekaiemu::spike {

void LibretroHost::Impl::LoadFrontendSettings() {
  frontend_settings_.Initialize(options.save_directory);
  runtime_menu.SetSettingsMode(frontend_settings_.Values().settings_mode);
  runtime_menu.ConsumeSettingsModeChanged();
  chat_overlay_.SetEnabled(false);
  notifications_enabled_ = frontend_settings_.Values().notifications_enabled;
  activity_feed_enabled_ = frontend_settings_.Values().activity_feed_enabled;
  client_core_hud_.Configure(options.client_core_hud_state_path,
                             options.client_core_hud_events_path,
                             frontend_settings_.Values().client_core_hud_buttons_visible);
  bridge_terminal_presenter_.SetEnabled(frontend_settings_.Values().bridge_terminal_enabled);
  audio_output.SetMasterVolumePercent(frontend_settings_.Values().master_volume_percent);
}

void LibretroHost::Impl::ApplyFrontendTrackerSettings() {
  if (!tracker_active_) {
    return;
  }
  const auto& settings = frontend_settings_.Values();
  tracker_runtime_.SetDisplayMode(settings.tracker_display_mode);
  tracker_runtime_.SetPrimaryScreenVisible(settings.tracker_screen_visible);
  tracker_runtime_.SetAutoMapFollow(settings.tracker_auto_follow);
  tracker_dirty_ = true;
}

void LibretroHost::Impl::ApplyFrontendWindowSettings() {
  if (!video_backend) {
    return;
  }
  std::string error;
  if (!video_backend->SetWindowMode(frontend_settings_.Values().window_mode, error)) {
    std::cerr << "[sekaiemu] frontend window mode apply failed: " << error << "\n";
  }
}

void LibretroHost::Impl::SaveFrontendSettingsNow() {
  if (!frontend_settings_.Initialized()) {
    return;
  }
  frontend_settings_.SetSettingsMode(runtime_menu.SettingsMode());
  frontend_settings_.SetChatOverlayEnabled(false);
  frontend_settings_.SetNotificationsEnabled(notifications_enabled_);
  frontend_settings_.SetActivityFeedEnabled(activity_feed_enabled_);
  frontend_settings_.SetClientCoreHudButtonsVisible(client_core_hud_.ButtonsVisible());
  frontend_settings_.SetBridgeTerminalEnabled(bridge_terminal_presenter_.Enabled());
  frontend_settings_.SetBackgroundGamepadInput(frontend_settings_.Values().background_gamepad_input);
  frontend_settings_.SetMasterVolumePercent(audio_output.MasterVolumePercent());
  if (video_backend) {
    frontend_settings_.SetWindowMode(video_backend->CurrentWindowMode());
  }
  if (tracker_active_) {
    frontend_settings_.SetTrackerDisplayMode(tracker_runtime_.UiState().display_mode);
    frontend_settings_.SetTrackerScreenVisible(tracker_runtime_.UiState().show_tracker_screen);
    frontend_settings_.SetTrackerAutoFollow(tracker_runtime_.LocalOverrideState().auto_follow_map);
  }

  std::string error;
  if (!frontend_settings_.Save(error)) {
    std::cerr << "[sekaiemu] frontend settings save failed: " << error << "\n";
  }
}

void LibretroHost::Impl::PersistRuntimeMenuSettingsIfNeeded() {
  if (!runtime_menu.ConsumeSettingsModeChanged()) {
    return;
  }
  SaveFrontendSettingsNow();
}

void LibretroHost::Impl::AdjustMasterVolume(int delta_percent) {
  audio_output.SetMasterVolumePercent(audio_output.MasterVolumePercent() + delta_percent);
  SaveFrontendSettingsNow();
}

void LibretroHost::Impl::ToggleNotifications() {
  notifications_enabled_ = !notifications_enabled_;
  SaveFrontendSettingsNow();
}

void LibretroHost::Impl::ToggleActivityFeed() {
  activity_feed_enabled_ = !activity_feed_enabled_;
  if (!activity_feed_enabled_) {
    activity_feed_window_presenter_.Shutdown();
  }
  SaveFrontendSettingsNow();
}

}  // namespace sekaiemu::spike
