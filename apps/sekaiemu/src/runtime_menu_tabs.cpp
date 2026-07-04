#include "runtime_menu_tabs.hpp"

#include <array>
#include <string_view>

namespace sekaiemu::spike {
namespace {

struct TabInfo {
  RuntimeMenu::Page page;
  std::string_view label;
  int width = 0;
  bool advanced_only = false;
};

constexpr std::array<TabInfo, 6> kTabs{{
    {RuntimeMenu::Page::Main, "MAIN", 110, false},
    {RuntimeMenu::Page::Settings, "SETTINGS", 150, false},
    {RuntimeMenu::Page::InputSettings, "CONTROLS", 150, false},
    {RuntimeMenu::Page::CoreSettings, "CORE", 110, true},
    {RuntimeMenu::Page::BridgeStatus, "BRIDGE", 130, true},
    {RuntimeMenu::Page::SyncInfo, "SYNC INFOS", 150, false},
}};

}  // namespace

void DrawRuntimeMenuTabs(OverlayCanvas& canvas,
                         int x,
                         int y,
                         RuntimeMenu::Page page,
                         RuntimeSettingsMode mode,
                         const CoreOptionManager& core_options) {
  int tab_x = x;
  for (const auto& tab : kTabs) {
    const bool selected = page == tab.page;
    const bool locked = tab.advanced_only && mode == RuntimeSettingsMode::Easy;
    const UiColor fill = selected ? UiColor{92, 122, 196, 255}
                         : locked ? UiColor{20, 24, 34, 210}
                                  : UiColor{34, 42, 58, 255};
    const UiColor text = locked ? UiColor{105, 120, 150, 255}
                                : UiColor{255, 255, 255, 255};
    canvas.FillRect(tab_x, y, tab.width, 26, fill);
    canvas.DrawRect(tab_x, y, tab.width, 26,
                    locked ? UiColor{44, 56, 78, 255} : UiColor{72, 94, 132, 255});
    canvas.DrawText(tab_x + 16, y + 7, tab.label, text, 2);
    tab_x += tab.width + 10;
  }

  const char* mode_label = mode == RuntimeSettingsMode::Easy ? "EASY" : "ADVANCED";
  canvas.DrawText(tab_x + 10,
                  y + 8,
                  mode_label,
                  mode == RuntimeSettingsMode::Easy ? UiColor{170, 230, 180, 255}
                                                    : UiColor{255, 215, 150, 255},
                  1);
  if (core_options.HasPendingChanges()) {
    canvas.DrawText(tab_x + 74,
                    y + 8,
                    core_options.PendingChangesRequireRestart() ? "CORE PENDING RESTART" : "CORE PENDING",
                    core_options.PendingChangesRequireRestart()
                        ? UiColor{255, 205, 120, 255}
                        : UiColor{180, 205, 255, 255},
                    1);
  }
}

}  // namespace sekaiemu::spike
