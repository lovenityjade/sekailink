#include "runtime_menu_controls.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace sekaiemu::spike {
namespace {

std::string Truncate(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
}

void DrawButton(OverlayCanvas& canvas,
                int x,
                int y,
                int size,
                std::string_view label,
                UiColor fill,
                UiColor text) {
  canvas.FillRect(x, y, size, size, fill);
  canvas.DrawRect(x, y, size, size, UiColor{210, 225, 245, 255});
  canvas.DrawText(x + size / 2 - 4, y + size / 2 - 4, label, text, 1);
}

void DrawControllerArt(OverlayCanvas& canvas, int x, int y, int width, int height) {
  const UiColor body{28, 36, 52, 235};
  const UiColor trim{76, 100, 140, 255};
  const UiColor accent{255, 215, 130, 255};
  const UiColor blue{108, 178, 255, 255};

  const int cx = x + width / 2;
  const int body_y = y + height / 5;
  const int body_h = height * 3 / 5;
  canvas.FillRect(x + width / 9, body_y, width * 7 / 9, body_h, body);
  canvas.DrawRect(x + width / 9, body_y, width * 7 / 9, body_h, trim);
  canvas.FillRect(x + width / 18, body_y + body_h / 3, width / 5, body_h * 2 / 3, body);
  canvas.DrawRect(x + width / 18, body_y + body_h / 3, width / 5, body_h * 2 / 3, trim);
  canvas.FillRect(x + width * 13 / 18, body_y + body_h / 3, width / 5, body_h * 2 / 3, body);
  canvas.DrawRect(x + width * 13 / 18, body_y + body_h / 3, width / 5, body_h * 2 / 3, trim);

  canvas.FillRect(x + width / 4, y + 8, width / 5, 18, body);
  canvas.DrawRect(x + width / 4, y + 8, width / 5, 18, trim);
  canvas.FillRect(x + width * 11 / 20, y + 8, width / 5, 18, body);
  canvas.DrawRect(x + width * 11 / 20, y + 8, width / 5, 18, trim);
  canvas.DrawText(x + width / 4 + 8, y + 14, "L", accent, 1);
  canvas.DrawText(x + width * 11 / 20 + 8, y + 14, "R", accent, 1);

  const int d = std::max(16, width / 16);
  const int dpx = x + width / 4;
  const int dpy = body_y + body_h / 2 - d / 2;
  canvas.FillRect(dpx - d, dpy, d * 3, d, UiColor{12, 16, 24, 255});
  canvas.FillRect(dpx, dpy - d, d, d * 3, UiColor{12, 16, 24, 255});
  canvas.DrawRect(dpx - d, dpy, d * 3, d, trim);
  canvas.DrawRect(dpx, dpy - d, d, d * 3, trim);
  canvas.DrawText(dpx - d, dpy + d + 8, "DPAD", UiColor{180, 205, 255, 255}, 1);

  const int b = std::max(18, width / 14);
  const int bx = x + width * 3 / 4 - b;
  const int by = body_y + body_h / 2 - b / 2;
  DrawButton(canvas, bx - b - 8, by, b, "B", blue, UiColor{5, 8, 14, 255});
  DrawButton(canvas, bx, by - b - 8, b, "X", accent, UiColor{5, 8, 14, 255});
  DrawButton(canvas, bx, by + b + 8, b, "A", accent, UiColor{5, 8, 14, 255});
  DrawButton(canvas, bx + b + 8, by, b, "Y", blue, UiColor{5, 8, 14, 255});

  canvas.FillRect(cx - 54, body_y + body_h / 2 - 6, 42, 12, UiColor{12, 16, 24, 255});
  canvas.FillRect(cx + 12, body_y + body_h / 2 - 6, 42, 12, UiColor{12, 16, 24, 255});
  canvas.DrawText(cx - 62, body_y + body_h / 2 + 14, "SELECT", UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(cx + 8, body_y + body_h / 2 + 14, "START", UiColor{180, 205, 255, 255}, 1);
}

struct FrontendControlRow {
  std::string_view label;
  std::string_view binding;
  std::string_view tooltip;
};

constexpr std::array<FrontendControlRow, 14> kFrontendControlRows{{
    {"SHORTCUT HELP", "F1", "PAUSE THE GAME AND SHOW SEKAIEMU SHORTCUT KEYS."},
    {"OPEN MENU", "ESC", "OPEN OR CLOSE THE SEKAIEMU MENU."},
    {"MENU PAGE", "TAB", "CHANGE MENU PAGE WHILE THE MENU IS OPEN."},
    {"MENU SELECT", "ENTER", "SELECT OR APPLY THE CURRENT MENU ROW."},
    {"FULLSCREEN", "F12 / ALT+ENTER", "TOGGLE FULLSCREEN FOR THE MAIN SEKAIEMU WINDOW."},
    {"CHAT INPUT", "T", "OPEN THE ROOM CHAT INPUT WHEN CHAT OVERLAY IS ENABLED."},
    {"TRACKER SCREEN", "TAB / F9", "TOGGLE GAMEPLAY AND TRACKER SCREEN IN TOGGLE MODE."},
    {"TRACKER DISPLAY", "F8", "CYCLE SPLIT, SEPARATE WINDOW, AND TOGGLE TRACKER MODES."},
    {"TRACKER MAP MENU", "RIGHT CLICK", "OPEN THE TRACKER MAP CONTEXT MENU."},
    {"TRACKER TAB", "F10", "CYCLE TRACKER TAB WHEN THE PACK EXPOSES TABS."},
    {"AUTOTRACKING", "F11", "TOGGLE AUTOMATIC MAP FOLLOW."},
    {"SAVE STATE", "F6", "SAVE THE DEFAULT QUICK STATE."},
    {"LOAD STATE", "F7", "LOAD THE DEFAULT QUICK STATE."},
    {"MEMORY SNAPSHOT", "F5", "WRITE A DEBUG MEMORY SNAPSHOT."},
}};

void DrawFrontendControlsRows(OverlayCanvas& canvas,
                              int rows_x,
                              int rows_width,
                              int list_y,
                              int list_bottom,
                              int selected_index,
                              int scroll_offset,
                              int& visible_rows,
                              std::string& tooltip) {
  const int row_height = 28;
  const int label_width = static_cast<int>(rows_width * 0.42f);
  const int value_x = rows_x + label_width + 12;
  const int value_width = rows_width - label_width - 20;
  visible_rows = std::max(1, (list_bottom - list_y) / row_height);
  const int total_rows = static_cast<int>(kFrontendControlRows.size());
  const int end = std::min(total_rows, scroll_offset + visible_rows);
  for (int index = scroll_offset; index < end; ++index) {
    const bool selected = index == selected_index;
    const int row = index - scroll_offset;
    const int y = list_y + row * row_height;
    const auto& control = kFrontendControlRows[static_cast<std::size_t>(index)];
    canvas.FillRect(rows_x, y, rows_width, row_height - 3,
                    selected ? UiColor{49, 70, 115, 255} : UiColor{24, 30, 42, 205});
    canvas.DrawText(rows_x + 8,
                    y + 8,
                    Truncate(control.label, static_cast<std::size_t>(std::max(8, label_width / 6))),
                    UiColor{255, 245, 225, 255},
                    1);
    canvas.DrawText(value_x,
                    y + 8,
                    Truncate(control.binding, static_cast<std::size_t>(std::max(8, value_width / 6))),
                    UiColor{170, 230, 180, 255},
                    1);
    if (selected) {
      tooltip = std::string(control.tooltip);
    }
  }

  if (total_rows > visible_rows) {
    const int track_x = rows_x + rows_width - 8;
    const int track_height = visible_rows * row_height - 4;
    canvas.FillRect(track_x, list_y, 4, track_height, UiColor{40, 52, 74, 180});
    const float ratio = static_cast<float>(visible_rows) / static_cast<float>(total_rows);
    const int thumb_height = std::max(14, static_cast<int>(track_height * ratio));
    const float offset_ratio =
        total_rows <= visible_rows
            ? 0.0f
            : static_cast<float>(scroll_offset) / static_cast<float>(total_rows - visible_rows);
    const int thumb_y = list_y + static_cast<int>((track_height - thumb_height) * offset_ratio);
    canvas.FillRect(track_x, thumb_y, 4, thumb_height, UiColor{130, 150, 190, 255});
  }
}

}  // namespace

int RuntimeMenuFrontendControlsRowCount() {
  return static_cast<int>(kFrontendControlRows.size());
}

void DrawRuntimeMenuControlsPage(OverlayCanvas& canvas,
                                 int list_x,
                                 int list_y,
                                 int list_width,
                                 int list_bottom,
                                 int selected_index,
                                 int scroll_offset,
                                 int& visible_rows,
                                 InputState& input_state,
                                 RuntimeSettingsMode mode,
                                 RuntimeControlsPageMode controls_mode,
                                 std::string& tooltip) {
  const int header_height = 70;
  const int header_bottom = std::min(list_bottom, list_y + header_height);
  const int rows_x = list_x;
  const int rows_width = list_width;
  const int rows_y = std::min(list_bottom, header_bottom + 10);

  canvas.FillRect(list_x, list_y, list_width, std::max(0, header_bottom - list_y),
                  UiColor{16, 22, 34, 220});
  canvas.DrawRect(list_x, list_y, list_width, std::max(0, header_bottom - list_y),
                  UiColor{65, 85, 120, 255});
  canvas.DrawText(list_x + 14,
                  list_y + 12,
                  controls_mode == RuntimeControlsPageMode::Sekaiemu
                      ? "SEKAIEMU SHORTCUTS"
                      : mode == RuntimeSettingsMode::Easy ? "CONTROLLER SETTINGS" : "CONTROLLER SETTINGS ADVANCED",
                  UiColor{255, 245, 225, 255},
                  2);
  canvas.DrawWrappedText(list_x + 14,
                         list_y + 42,
                         list_width - 28,
                         controls_mode == RuntimeControlsPageMode::Sekaiemu
                             ? "Frontend shortcuts are separate from game controls."
                             : "Enter changes a binding. Left and Right switch the active SDL controller.",
                         UiColor{180, 205, 255, 255},
                         1,
                         2);

  if (controls_mode == RuntimeControlsPageMode::Sekaiemu) {
    DrawFrontendControlsRows(canvas,
                             rows_x,
                             rows_width,
                             rows_y,
                             list_bottom,
                             selected_index,
                             scroll_offset,
                             visible_rows,
                             tooltip);
    return;
  }

  const int row_height = 30;
  const int label_width = static_cast<int>(rows_width * 0.34f);
  const int value_x = rows_x + label_width + 12;
  const int value_width = rows_width - label_width - 20;
  visible_rows = std::max(1, (list_bottom - rows_y) / row_height);
  const int total_rows = static_cast<int>(input_state.InputMenuRowCount());
  const int end = std::min(total_rows, scroll_offset + visible_rows);
  for (int index = scroll_offset; index < end; ++index) {
    const bool selected = index == selected_index;
    const int row = index - scroll_offset;
    const int y = rows_y + row * row_height;
    const auto menu_row = input_state.InputMenuRowAt(static_cast<std::size_t>(index));
    canvas.FillRect(rows_x, y, rows_width, row_height - 3,
                    selected ? UiColor{49, 70, 115, 255} : UiColor{24, 30, 42, 205});
    canvas.DrawText(rows_x + 8,
                    y + 9,
                    Truncate(input_state.InputMenuRowLabelAt(static_cast<std::size_t>(index)),
                             static_cast<std::size_t>(std::max(8, label_width / 6))),
                    UiColor{255, 245, 225, 255},
                    1);
    canvas.DrawText(value_x,
                    y + 9,
                    Truncate(input_state.InputMenuRowValueAt(static_cast<std::size_t>(index)),
                             static_cast<std::size_t>(std::max(8, value_width / 6))),
                    menu_row.kind == InputMenuRowKind::Controller
                        ? UiColor{120, 235, 245, 255}
                        : input_state.CaptureActive() && index == selected_index
                        ? UiColor{255, 215, 150, 255}
                        : UiColor{170, 230, 180, 255},
                    1);
    if (selected) {
      tooltip = input_state.InputMenuTooltipAt(static_cast<std::size_t>(index));
    }
  }

  if (total_rows > visible_rows) {
    const int track_x = rows_x + rows_width - 8;
    const int track_height = visible_rows * row_height - 4;
    canvas.FillRect(track_x, rows_y, 4, track_height, UiColor{40, 52, 74, 180});
    const float ratio = static_cast<float>(visible_rows) / static_cast<float>(total_rows);
    const int thumb_height = std::max(14, static_cast<int>(track_height * ratio));
    const float offset_ratio =
        total_rows <= visible_rows
            ? 0.0f
            : static_cast<float>(scroll_offset) / static_cast<float>(total_rows - visible_rows);
    const int thumb_y = rows_y + static_cast<int>((track_height - thumb_height) * offset_ratio);
    canvas.FillRect(track_x, thumb_y, 4, thumb_height, UiColor{130, 150, 190, 255});
  }

  if (input_state.CaptureActive()) {
    tooltip = input_state.CapturePrompt();
  }
}

}  // namespace sekaiemu::spike
