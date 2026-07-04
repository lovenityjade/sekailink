#pragma once

#include "runtime_settings_mode.hpp"
#include "tracker_runtime.hpp"
#include "video_backend.hpp"

#include <filesystem>
#include <string>

namespace sekaiemu::spike {

struct RuntimeFrontendSettings {
  RuntimeSettingsMode settings_mode = RuntimeSettingsMode::Easy;
  bool chat_overlay_enabled = true;
  bool notifications_enabled = true;
  bool activity_feed_enabled = false;
  bool client_core_hud_buttons_visible = true;
  bool bridge_terminal_enabled = false;
  bool background_gamepad_input = false;
  WindowMode window_mode = WindowMode::BorderlessWindow;
  int master_volume_percent = 35;
  TrackerDisplayMode tracker_display_mode = TrackerDisplayMode::SplitScreen;
  bool tracker_screen_visible = true;
  bool tracker_auto_follow = true;
};

class RuntimeFrontendSettingsStore {
 public:
  void Initialize(const std::filesystem::path& save_root);

  const RuntimeFrontendSettings& Values() const { return values_; }
  void SetSettingsMode(RuntimeSettingsMode mode) { values_.settings_mode = mode; }
  void SetChatOverlayEnabled(bool enabled) { values_.chat_overlay_enabled = enabled; }
  void SetNotificationsEnabled(bool enabled) { values_.notifications_enabled = enabled; }
  void SetActivityFeedEnabled(bool enabled) { values_.activity_feed_enabled = enabled; }
  void SetClientCoreHudButtonsVisible(bool visible) { values_.client_core_hud_buttons_visible = visible; }
  void SetBridgeTerminalEnabled(bool enabled) { values_.bridge_terminal_enabled = enabled; }
  void SetBackgroundGamepadInput(bool enabled) { values_.background_gamepad_input = enabled; }
  void SetWindowMode(WindowMode mode) { values_.window_mode = mode; }
  void SetMasterVolumePercent(int percent);
  void SetTrackerDisplayMode(TrackerDisplayMode mode) { values_.tracker_display_mode = mode; }
  void SetTrackerScreenVisible(bool visible) { values_.tracker_screen_visible = visible; }
  void SetTrackerAutoFollow(bool enabled) { values_.tracker_auto_follow = enabled; }

  bool Save(std::string& error) const;
  const std::filesystem::path& ConfigPath() const { return config_path_; }
  bool Initialized() const { return !config_path_.empty(); }

 private:
  void Load();

  std::filesystem::path config_path_;
  RuntimeFrontendSettings values_{};
};

}  // namespace sekaiemu::spike
