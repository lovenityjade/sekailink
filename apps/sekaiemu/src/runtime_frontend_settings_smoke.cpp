#include "runtime_frontend_settings.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
  namespace fs = std::filesystem;
  using namespace sekaiemu::spike;

  const fs::path root = fs::temp_directory_path() / "sekaiemu-runtime-frontend-settings-smoke";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);

  RuntimeFrontendSettingsStore store;
  store.Initialize(root);
  if (store.Values().settings_mode != RuntimeSettingsMode::Easy ||
      !store.Values().chat_overlay_enabled ||
      !store.Values().notifications_enabled ||
      store.Values().activity_feed_enabled ||
      store.Values().bridge_terminal_enabled ||
      store.Values().background_gamepad_input ||
      store.Values().master_volume_percent != 35 ||
      store.Values().tracker_display_mode != TrackerDisplayMode::SplitScreen ||
      !store.Values().tracker_screen_visible ||
      !store.Values().tracker_auto_follow) {
    std::cerr << "frontend_settings_defaults_failed\n";
    return 1;
  }

  store.SetSettingsMode(RuntimeSettingsMode::Advanced);
  store.SetChatOverlayEnabled(false);
  store.SetNotificationsEnabled(false);
  store.SetActivityFeedEnabled(true);
  store.SetBridgeTerminalEnabled(true);
  store.SetBackgroundGamepadInput(true);
  store.SetMasterVolumePercent(65);
  store.SetTrackerDisplayMode(TrackerDisplayMode::ToggleScreen);
  store.SetTrackerScreenVisible(false);
  store.SetTrackerAutoFollow(false);
  std::string error;
  if (!store.Save(error)) {
    std::cerr << "frontend_settings_save_failed:" << error << "\n";
    return 1;
  }

  RuntimeFrontendSettingsStore loaded;
  loaded.Initialize(root);
  if (loaded.Values().settings_mode != RuntimeSettingsMode::Advanced ||
      loaded.Values().chat_overlay_enabled ||
      loaded.Values().notifications_enabled ||
      !loaded.Values().activity_feed_enabled ||
      !loaded.Values().bridge_terminal_enabled ||
      !loaded.Values().background_gamepad_input ||
      loaded.Values().master_volume_percent != 65 ||
      loaded.Values().tracker_display_mode != TrackerDisplayMode::ToggleScreen ||
      loaded.Values().tracker_screen_visible ||
      loaded.Values().tracker_auto_follow) {
    std::cerr << "frontend_settings_reload_failed\n";
    return 1;
  }

  {
    std::ofstream stream(loaded.ConfigPath(), std::ios::trunc);
    stream << "# aliases are accepted\n";
    stream << "menu_mode=simple\n";
    stream << "chat_overlay=on\n";
    stream << "notification=on\n";
    stream << "activity_feed=off\n";
    stream << "bridge_terminal=off\n";
    stream << "system_wide_gamepad_input=on\n";
    stream << "volume=999\n";
    stream << "tracker_display=separate\n";
    stream << "tracker_screen=off\n";
    stream << "auto_follow_map=no\n";
  }
  RuntimeFrontendSettingsStore aliased;
  aliased.Initialize(root);
  if (aliased.Values().settings_mode != RuntimeSettingsMode::Easy ||
      !aliased.Values().chat_overlay_enabled ||
      !aliased.Values().notifications_enabled ||
      aliased.Values().activity_feed_enabled ||
      aliased.Values().bridge_terminal_enabled ||
      !aliased.Values().background_gamepad_input ||
      aliased.Values().master_volume_percent != 150 ||
      aliased.Values().tracker_display_mode != TrackerDisplayMode::SeparateWindow ||
      aliased.Values().tracker_screen_visible ||
      aliased.Values().tracker_auto_follow) {
    std::cerr << "frontend_settings_alias_parse_failed\n";
    return 1;
  }

  std::cout << "runtime_frontend_settings_smoke_ok\n";
  return 0;
}
