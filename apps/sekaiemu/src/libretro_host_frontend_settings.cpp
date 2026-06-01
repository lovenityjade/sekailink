#include "libretro_host_internal.hpp"

#include <iostream>

namespace sekaiemu::spike {

void LibretroHost::Impl::LoadFrontendSettings() {
  frontend_settings_.Initialize(options.save_directory);
  runtime_menu.SetSettingsMode(frontend_settings_.Values().settings_mode);
  runtime_menu.ConsumeSettingsModeChanged();
  chat_overlay_.SetEnabled(frontend_settings_.Values().chat_overlay_enabled);
  notifications_enabled_ = frontend_settings_.Values().notifications_enabled;
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

void LibretroHost::Impl::SaveFrontendSettingsNow() {
  if (!frontend_settings_.Initialized()) {
    return;
  }
  frontend_settings_.SetSettingsMode(runtime_menu.SettingsMode());
  frontend_settings_.SetChatOverlayEnabled(chat_overlay_.Enabled());
  frontend_settings_.SetNotificationsEnabled(notifications_enabled_);
  frontend_settings_.SetBridgeTerminalEnabled(bridge_terminal_presenter_.Enabled());
  frontend_settings_.SetMasterVolumePercent(audio_output.MasterVolumePercent());
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

}  // namespace sekaiemu::spike
