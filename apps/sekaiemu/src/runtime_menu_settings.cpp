#include "runtime_menu_settings.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace sekaiemu::spike {
namespace {

struct SettingsRow {
  std::string_view label;
  std::string_view easy_value;
  std::string_view advanced_value;
  std::string_view tooltip;
};

constexpr std::array<SettingsRow, 11> kRows{{
    {"SETTINGS MODE", "EASY", "ADVANCED", "SWITCH BETWEEN THE STREAMLINED MENU AND FULL TECHNICAL SETTINGS."},
    {"MASTER VOLUME", "50%", "50%", "CHANGE THE SDL OUTPUT VOLUME. DEFAULT IS 50 PERCENT."},
    {"CHAT OVERLAY", "TOGGLE", "TOGGLE", "SHOW OR HIDE THE ROOM CHAT AND ITEM EVENT OVERLAY."},
    {"NOTIFICATION", "TOGGLE", "TOGGLE", "SHOW OR HIDE LIGHTWEIGHT SEKAILINK NOTIFICATIONS."},
    {"TRACKER DISPLAY", "CYCLE", "CYCLE", "SWITCH BETWEEN SPLIT, SEPARATE WINDOW, AND TOGGLE TRACKER MODES."},
    {"AUTOTRACKING", "TOGGLE", "TOGGLE", "LET THE TRACKER RETURN TO THE CURRENT PLAYER MAP WHEN RUNTIME HINTS EXIST."},
    {"SEKAIEMU CONTROLS", "OPEN", "OPEN", "OPEN FRONTEND AND MENU CONTROL SETTINGS."},
    {"CORE CONTROLS", "OPEN", "OPEN", "OPEN GAME CORE INPUT BINDINGS."},
    {"CORE OPTIONS", "ADVANCED", "OPEN", "CHANGE LIBRETRO CORE OPTIONS. THESE ARE HIDDEN FROM THE EASY TAB LOOP."},
    {"BRIDGE STATUS", "ADVANCED", "OPEN", "CHECK OR RESTART THE LOCAL SKLMI COMPANION PROCESS."},
    {"SYNC INFOS", "OPEN", "OPEN", "VIEW ROOM, PLAYER, AND CURRENT SYNC METADATA."},
}};

std::string Truncate(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
}

std::string DisplayModeLabel(TrackerDisplayMode mode) {
  switch (mode) {
    case TrackerDisplayMode::SplitScreen:
      return "SPLIT";
    case TrackerDisplayMode::SeparateWindow:
      return "SEPARATE";
    case TrackerDisplayMode::PipOverlay:
      return "PIP";
    case TrackerDisplayMode::ToggleScreen:
      return "TOGGLE";
  }
  return "SPLIT";
}

}  // namespace

int RuntimeMenuSettingsRowCount() { return static_cast<int>(kRows.size()); }

void DrawRuntimeMenuSettingsPage(OverlayCanvas& canvas,
                                 int list_x,
                                 int list_y,
                                 int list_width,
                                 int list_bottom,
                                 int selected_index,
                                 RuntimeSettingsMode mode,
                                 bool chat_overlay_enabled,
                                 bool notifications_enabled,
                                 int master_volume_percent,
                                 const TrackerRuntime* tracker_runtime,
                                 std::string& tooltip) {
  const int row_height = 48;
  const int label_width = std::max(220, list_width * 36 / 100);
  const int value_x = list_x + label_width + 16;
  const int value_width = list_width - label_width - 28;
  const UiColor normal_fill{24, 30, 42, 205};
  const UiColor selected_fill{49, 70, 115, 255};

  for (int index = 0; index < static_cast<int>(kRows.size()); ++index) {
    const int y = list_y + index * row_height;
    if (y + row_height > list_bottom) {
      break;
    }
    const bool selected = index == selected_index;
    const auto& row = kRows[static_cast<std::size_t>(index)];
    const bool advanced_locked =
        (index == 8 || index == 9) && mode == RuntimeSettingsMode::Easy;
    canvas.FillRect(list_x, y, list_width, row_height - 6,
                    selected ? selected_fill : normal_fill);
    canvas.DrawRect(list_x, y, list_width, row_height - 6,
                    selected ? UiColor{255, 215, 130, 255} : UiColor{65, 85, 120, 255});
    canvas.DrawText(list_x + 12,
                    y + 9,
                    row.label,
                    advanced_locked ? UiColor{125, 140, 165, 255}
                                    : UiColor{255, 245, 225, 255},
                    2);

    std::string value;
    if (index == 0) {
      value = mode == RuntimeSettingsMode::Easy ? "EASY" : "ADVANCED";
    } else if (index == 1) {
      value = std::to_string(master_volume_percent) + "%";
    } else if (index == 2) {
      value = chat_overlay_enabled ? "ON" : "OFF";
    } else if (index == 3) {
      value = notifications_enabled ? "ON" : "OFF";
    } else if (index == 4) {
      value = tracker_runtime != nullptr ? DisplayModeLabel(tracker_runtime->UiState().display_mode) : "NO TRACKER";
    } else if (index == 5) {
      value = tracker_runtime != nullptr
                  ? (tracker_runtime->LocalOverrideState().auto_follow_map ? "ON" : "OFF")
                  : "NO TRACKER";
    } else {
      value = std::string(mode == RuntimeSettingsMode::Easy ? row.easy_value : row.advanced_value);
    }
    canvas.DrawText(value_x,
                    y + 10,
                    Truncate(value, static_cast<std::size_t>(std::max(8, value_width / 12))),
                    advanced_locked ? UiColor{125, 140, 165, 255}
                                    : UiColor{170, 230, 180, 255},
                    2);
    canvas.DrawText(value_x,
                    y + 30,
                    index == 0 ? "ENTER OR LEFT/RIGHT TO SWITCH"
                               : index == 1 ? "LEFT/RIGHT CHANGES BY 5 PERCENT"
                               : advanced_locked ? "ENABLE ADVANCED MODE TO OPEN"
                                                 : "ENTER TO OPEN OR TOGGLE",
                    advanced_locked ? UiColor{105, 120, 150, 255}
                                    : UiColor{150, 170, 205, 255},
                    1);
    if (index == 1) {
      const int slider_x = std::min(value_x + 96, list_x + list_width - 230);
      const int slider_y = y + 14;
      const int slider_width = std::max(100, list_x + list_width - slider_x - 18);
      const int filled = (slider_width * std::clamp(master_volume_percent, 0, 150)) / 150;
      canvas.FillRect(slider_x, slider_y, slider_width, 8, UiColor{8, 12, 20, 255});
      canvas.FillRect(slider_x, slider_y, filled, 8, UiColor{255, 215, 130, 255});
      canvas.DrawRect(slider_x, slider_y, slider_width, 8, UiColor{105, 130, 170, 255});
    }
    if (selected) {
      tooltip = std::string(row.tooltip);
    }
  }
}

}  // namespace sekaiemu::spike
