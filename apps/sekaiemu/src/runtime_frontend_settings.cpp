#include "runtime_frontend_settings.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace sekaiemu::spike {
namespace {

constexpr const char* kFrontendConfigDir = "frontend-config";
constexpr const char* kFrontendConfigFile = "sekaiemu.cfg";

std::string Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

std::string Lower(std::string_view text) {
  std::string output(text);
  std::transform(output.begin(), output.end(), output.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  return output;
}

const char* SettingsModeToken(RuntimeSettingsMode mode) {
  return mode == RuntimeSettingsMode::Advanced ? "advanced" : "easy";
}

const char* TrackerDisplayModeToken(TrackerDisplayMode mode) {
  switch (mode) {
    case TrackerDisplayMode::SeparateWindow:
      return "separate-window";
    case TrackerDisplayMode::PipOverlay:
      return "pip-overlay";
    case TrackerDisplayMode::ToggleScreen:
      return "toggle-screen";
    case TrackerDisplayMode::SplitScreen:
      return "split-screen";
  }
  return "split-screen";
}

std::optional<RuntimeSettingsMode> ParseSettingsMode(std::string_view text) {
  const auto value = Lower(Trim(text));
  if (value == "advanced" || value == "expert") {
    return RuntimeSettingsMode::Advanced;
  }
  if (value == "easy" || value == "simple") {
    return RuntimeSettingsMode::Easy;
  }
  return std::nullopt;
}

TrackerDisplayMode ParseTrackerDisplayMode(std::string_view text) {
  const auto value = Lower(Trim(text));
  if (value == "separate-window" || value == "separate-windows" || value == "separate") {
    return TrackerDisplayMode::SeparateWindow;
  }
  if (value == "pip-overlay" || value == "pip" || value == "overlay") {
    return TrackerDisplayMode::PipOverlay;
  }
  if (value == "toggle-screen" || value == "toggle") {
    return TrackerDisplayMode::ToggleScreen;
  }
  return TrackerDisplayMode::SplitScreen;
}

std::optional<bool> ParseBool(std::string_view text) {
  const auto value = Lower(Trim(text));
  if (value == "1" || value == "true" || value == "yes" || value == "on") {
    return true;
  }
  if (value == "0" || value == "false" || value == "no" || value == "off") {
    return false;
  }
  return std::nullopt;
}

std::optional<int> ParseInt(std::string_view text) {
  try {
    std::size_t parsed = 0;
    const int value = std::stoi(std::string(Trim(text)), &parsed, 10);
    if (parsed == Trim(text).size()) {
      return value;
    }
  } catch (...) {
  }
  return std::nullopt;
}

}  // namespace

void RuntimeFrontendSettingsStore::Initialize(const std::filesystem::path& save_root) {
  config_path_ = save_root / kFrontendConfigDir / kFrontendConfigFile;
  values_ = RuntimeFrontendSettings{};
  Load();
}

void RuntimeFrontendSettingsStore::SetMasterVolumePercent(int percent) {
  values_.master_volume_percent = std::clamp(percent, 0, 150);
}

void RuntimeFrontendSettingsStore::Load() {
  if (config_path_.empty() || !std::filesystem::exists(config_path_)) {
    return;
  }

  std::ifstream stream(config_path_);
  if (!stream) {
    return;
  }

  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const auto key = Lower(Trim(line.substr(0, equals)));
    const auto value = Trim(line.substr(equals + 1));
    if (key == "settings_mode" || key == "menu_mode") {
      if (const auto parsed = ParseSettingsMode(value)) {
        values_.settings_mode = *parsed;
      }
    } else if (key == "chat_overlay_enabled" || key == "chat_overlay") {
      if (const auto parsed = ParseBool(value)) {
        values_.chat_overlay_enabled = *parsed;
      }
    } else if (key == "notifications_enabled" || key == "notification_enabled" || key == "notifications" ||
               key == "notification") {
      if (const auto parsed = ParseBool(value)) {
        values_.notifications_enabled = *parsed;
      }
    } else if (key == "bridge_terminal_enabled" || key == "bridge_terminal") {
      if (const auto parsed = ParseBool(value)) {
        values_.bridge_terminal_enabled = *parsed;
      }
    } else if (key == "master_volume_percent" || key == "volume_percent" || key == "volume") {
      if (const auto parsed = ParseInt(value)) {
        SetMasterVolumePercent(*parsed);
      }
    } else if (key == "tracker_display_mode" || key == "tracker_display") {
      values_.tracker_display_mode = ParseTrackerDisplayMode(value);
    } else if (key == "tracker_screen_visible" || key == "tracker_screen") {
      if (const auto parsed = ParseBool(value)) {
        values_.tracker_screen_visible = *parsed;
      }
    } else if (key == "tracker_auto_follow" || key == "auto_follow_map") {
      if (const auto parsed = ParseBool(value)) {
        values_.tracker_auto_follow = *parsed;
      }
    }
  }
}

bool RuntimeFrontendSettingsStore::Save(std::string& error) const {
  if (config_path_.empty()) {
    error = "Frontend settings path is empty.";
    return false;
  }

  std::error_code ec;
  std::filesystem::create_directories(config_path_.parent_path(), ec);
  if (ec) {
    error = "Failed to create frontend config directory: " + ec.message();
    return false;
  }

  auto temp_path = config_path_;
  temp_path += ".tmp";
  {
    std::ofstream stream(temp_path, std::ios::trunc);
    if (!stream) {
      error = "Failed to open frontend settings for writing.";
      return false;
    }
    stream << "# Sekaiemu frontend settings\n";
    stream << "settings_mode=" << SettingsModeToken(values_.settings_mode) << "\n";
    stream << "chat_overlay_enabled=" << (values_.chat_overlay_enabled ? "true" : "false") << "\n";
    stream << "notifications_enabled=" << (values_.notifications_enabled ? "true" : "false") << "\n";
    stream << "bridge_terminal_enabled=" << (values_.bridge_terminal_enabled ? "true" : "false") << "\n";
    stream << "master_volume_percent=" << values_.master_volume_percent << "\n";
    stream << "tracker_display_mode=" << TrackerDisplayModeToken(values_.tracker_display_mode) << "\n";
    stream << "tracker_screen_visible=" << (values_.tracker_screen_visible ? "true" : "false") << "\n";
    stream << "tracker_auto_follow=" << (values_.tracker_auto_follow ? "true" : "false") << "\n";
    if (!stream) {
      error = "Failed to write frontend settings.";
      return false;
    }
  }

  std::filesystem::rename(temp_path, config_path_, ec);
  if (ec) {
    std::filesystem::remove(config_path_, ec);
    ec.clear();
    std::filesystem::rename(temp_path, config_path_, ec);
  }
  if (ec) {
    error = "Failed to replace frontend settings: " + ec.message();
    return false;
  }
  return true;
}

}  // namespace sekaiemu::spike
