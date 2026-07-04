#include "runtime_menu.hpp"

#include "runtime_menu_core_settings.hpp"
#include "runtime_menu_controls.hpp"
#include "runtime_menu_main_page.hpp"
#include "runtime_menu_settings.hpp"
#include "runtime_menu_sync.hpp"
#include "runtime_menu_tabs.hpp"

#include <algorithm>
#include <array>

namespace sekaiemu::spike {

namespace {

constexpr std::string_view kSekaiemuVersion = "V0.1.0-DEV";

const std::array<RuntimeMenuMainActionRow, RuntimeMenu::kMainActionCount> kMainRows{{
    {"RESUME", "CLOSE THE MENU AND RESUME EMULATION.", RuntimeMenuAction::None},
    {"RESET CORE", "RESET THE RUNNING GAME CORE.", RuntimeMenuAction::ResetCore},
    {"LOAD AUTOSAVE", "LOAD AUTOSAVE SLOT 0.", RuntimeMenuAction::LoadState},
    {"SAVE STATE >", "OPEN MANUAL SAVE STATE SLOTS 1 TO 4.", RuntimeMenuAction::None},
    {"LOAD STATE >", "OPEN MANUAL LOAD STATE SLOTS 1 TO 4.", RuntimeMenuAction::None},
    {"QUIT SEKAIEMU", "CLOSE SEKAIEMU.", RuntimeMenuAction::QuitRuntime},
}};

const std::array<RuntimeMenuMainActionRow, RuntimeMenu::kMainSlotActionCount> kSaveSlotRows{{
    {"SLOT 1", "WRITE MANUAL SAVESTATE SLOT 1.", RuntimeMenuAction::SaveStateSlot1},
    {"SLOT 2", "WRITE MANUAL SAVESTATE SLOT 2.", RuntimeMenuAction::SaveStateSlot2},
    {"SLOT 3", "WRITE MANUAL SAVESTATE SLOT 3.", RuntimeMenuAction::SaveStateSlot3},
    {"SLOT 4", "WRITE MANUAL SAVESTATE SLOT 4.", RuntimeMenuAction::SaveStateSlot4},
    {"BACK", "RETURN TO THE MAIN MENU.", RuntimeMenuAction::None},
}};

const std::array<RuntimeMenuMainActionRow, RuntimeMenu::kMainSlotActionCount> kLoadSlotRows{{
    {"SLOT 1", "LOAD MANUAL SAVESTATE SLOT 1.", RuntimeMenuAction::LoadStateSlot1},
    {"SLOT 2", "LOAD MANUAL SAVESTATE SLOT 2.", RuntimeMenuAction::LoadStateSlot2},
    {"SLOT 3", "LOAD MANUAL SAVESTATE SLOT 3.", RuntimeMenuAction::LoadStateSlot3},
    {"SLOT 4", "LOAD MANUAL SAVESTATE SLOT 4.", RuntimeMenuAction::LoadStateSlot4},
    {"BACK", "RETURN TO THE MAIN MENU.", RuntimeMenuAction::None},
}};

constexpr std::array<RuntimeMenuCoreActionRow, 3> kCoreSettingsRows{{
    {"APPLY", "APPLY PENDING CORE SETTING CHANGES. ITEMS MARKED RESTART WILL ONLY FULLY TAKE EFFECT AFTER A CORE RESTART.", RuntimeMenuAction::ApplyCoreSettings},
    {"DEFAULT", "RESTORE ALL CORE SETTINGS TO THEIR DEFAULT VALUES. USE APPLY TO COMMIT THEM.", RuntimeMenuAction::ResetCoreSettingsToDefaults},
    {"CLOSE", "DISCARD UNAPPLIED CORE SETTING CHANGES AND CLOSE THE MENU.", RuntimeMenuAction::CloseMenu},
}};

struct BridgeActionRow {
  const char* label;
  const char* tooltip;
  RuntimeMenuAction action;
};

constexpr std::array<BridgeActionRow, 2> kBridgeRows{{
    {"RESTART BRIDGE", "RESTART THE ACTIVE SKLMI COMPANION PROCESS WITHOUT RESTARTING THE GAME CORE.", RuntimeMenuAction::RestartBridge},
    {"OPEN BRIDGE TERMINAL", "OPEN OR CLOSE THE SEPARATE SEKAILINK ROOM EVENT TERMINAL.", RuntimeMenuAction::ToggleBridgeTerminal},
}};

RuntimeMenu::Page NextPage(RuntimeMenu::Page page, RuntimeSettingsMode mode) {
  switch (page) {
    case RuntimeMenu::Page::Main:
      return RuntimeMenu::Page::Settings;
    case RuntimeMenu::Page::Settings:
      return RuntimeMenu::Page::InputSettings;
    case RuntimeMenu::Page::InputSettings:
      return mode == RuntimeSettingsMode::Advanced ? RuntimeMenu::Page::CoreSettings
                                                   : RuntimeMenu::Page::SyncInfo;
    case RuntimeMenu::Page::CoreSettings:
      return RuntimeMenu::Page::BridgeStatus;
    case RuntimeMenu::Page::BridgeStatus:
      return RuntimeMenu::Page::SyncInfo;
    case RuntimeMenu::Page::SyncInfo:
      return RuntimeMenu::Page::Main;
  }
  return RuntimeMenu::Page::Main;
}

std::string TruncateText(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
}

}

void RuntimeMenu::Toggle() {
  if (visible_) {
    Close();
  } else {
    Open();
  }
}

void RuntimeMenu::Open() { visible_ = true; }

void RuntimeMenu::OpenShortcutHelp() {
  visible_ = true;
  page_ = Page::InputSettings;
  controls_page_mode_ = RuntimeControlsPageMode::Sekaiemu;
  main_slot_mode_ = MainSlotMode::None;
  selected_index_ = 0;
  scroll_offset_ = 0;
}

void RuntimeMenu::Close() {
  visible_ = false;
  main_slot_mode_ = MainSlotMode::None;
}

RuntimeMenuAction RuntimeMenu::HandleKey(SDL_Scancode scancode,
                                         CoreOptionManager& core_options,
                                         InputState& input_state) {
  if (!visible_) {
    return RuntimeMenuAction::None;
  }

  if (scancode == SDL_SCANCODE_ESCAPE) {
    core_options.DiscardPendingChanges();
    Close();
    return RuntimeMenuAction::None;
  }

  const int option_count = static_cast<int>(core_options.DefinitionCount());
  const int input_count = static_cast<int>(input_state.InputMenuRowCount());
  const int item_count = page_ == Page::Main
                             ? (main_slot_mode_ == MainSlotMode::None ? kMainActionCount : kMainSlotActionCount)
                             : page_ == Page::Settings
                                   ? RuntimeMenuSettingsRowCount()
                                   : page_ == Page::CoreSettings
                                         ? option_count + static_cast<int>(kCoreSettingsRows.size())
                                         : page_ == Page::InputSettings
                                               ? (controls_page_mode_ == RuntimeControlsPageMode::Sekaiemu
                                                      ? RuntimeMenuFrontendControlsRowCount()
                                                      : input_count)
                                               : page_ == Page::BridgeStatus
                                                     ? static_cast<int>(kBridgeRows.size())
                                                     : 1;
  if (item_count <= 0) {
    if (scancode == SDL_SCANCODE_TAB) {
      page_ = NextPage(page_, settings_mode_);
    }
    return RuntimeMenuAction::None;
  }

  const auto toggle_settings_mode = [&]() {
    SetSettingsMode(settings_mode_ == RuntimeSettingsMode::Easy ? RuntimeSettingsMode::Advanced
                                                                : RuntimeSettingsMode::Easy);
  };

  switch (scancode) {
    case SDL_SCANCODE_TAB:
      page_ = NextPage(page_, settings_mode_);
      main_slot_mode_ = MainSlotMode::None;
      selected_index_ = 0;
      scroll_offset_ = 0;
      return RuntimeMenuAction::None;
    case SDL_SCANCODE_UP:
      selected_index_ = std::max(0, selected_index_ - 1);
      break;
    case SDL_SCANCODE_DOWN:
      selected_index_ = std::min(item_count - 1, selected_index_ + 1);
      break;
    case SDL_SCANCODE_LEFT:
      if (page_ == Page::Settings && selected_index_ == 0) {
        toggle_settings_mode();
      } else if (page_ == Page::CoreSettings && selected_index_ < option_count) {
        if (const auto* definition = core_options.DefinitionAt(static_cast<std::size_t>(selected_index_))) {
          core_options.StepValue(definition->key, -1);
        }
      } else if (page_ == Page::InputSettings && controls_page_mode_ == RuntimeControlsPageMode::Core) {
        input_state.StepInputMenuRow(static_cast<std::size_t>(selected_index_), -1);
      } else if (page_ == Page::Settings) {
        switch (selected_index_) {
          case 1:
            return RuntimeMenuAction::DecreaseMasterVolume;
          case 3:
            return RuntimeMenuAction::ToggleNotifications;
          case 4:
            return RuntimeMenuAction::CycleTrackerDisplayMode;
          case 5:
            return RuntimeMenuAction::ToggleTrackerAutoFollow;
          default:
            break;
        }
      }
      break;
    case SDL_SCANCODE_RIGHT:
      if (page_ == Page::Settings && selected_index_ == 0) {
        toggle_settings_mode();
      } else if (page_ == Page::CoreSettings && selected_index_ < option_count) {
        if (const auto* definition = core_options.DefinitionAt(static_cast<std::size_t>(selected_index_))) {
          core_options.StepValue(definition->key, 1);
        }
      } else if (page_ == Page::InputSettings && controls_page_mode_ == RuntimeControlsPageMode::Core) {
        input_state.StepInputMenuRow(static_cast<std::size_t>(selected_index_), 1);
      } else if (page_ == Page::Settings) {
        switch (selected_index_) {
          case 1:
            return RuntimeMenuAction::IncreaseMasterVolume;
          case 3:
            return RuntimeMenuAction::ToggleNotifications;
          case 4:
            return RuntimeMenuAction::CycleTrackerDisplayMode;
          case 5:
            return RuntimeMenuAction::ToggleTrackerAutoFollow;
          default:
            break;
        }
      }
      break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
      if (page_ == Page::Main) {
        if (main_slot_mode_ != MainSlotMode::None) {
          if (selected_index_ >= 0 && selected_index_ <= 3) {
            return (main_slot_mode_ == MainSlotMode::Save ? kSaveSlotRows : kLoadSlotRows)
                [static_cast<std::size_t>(selected_index_)]
                    .action;
          }
          const auto closed_mode = main_slot_mode_;
          main_slot_mode_ = MainSlotMode::None;
          selected_index_ = closed_mode == MainSlotMode::Save ? 3 : 4;
          return RuntimeMenuAction::None;
        }
        if (selected_index_ == 0) {
          Close();
          return RuntimeMenuAction::None;
        }
        if (selected_index_ == 3 || selected_index_ == 4) {
          main_slot_mode_ = selected_index_ == 3 ? MainSlotMode::Save : MainSlotMode::Load;
          selected_index_ = 0;
          scroll_offset_ = 0;
          return RuntimeMenuAction::None;
        }
        return kMainRows[static_cast<std::size_t>(selected_index_)].action;
      }
      if (page_ == Page::Settings) {
        switch (selected_index_) {
          case 0:
            toggle_settings_mode();
            break;
          case 1:
            return RuntimeMenuAction::IncreaseMasterVolume;
          case 2:
            return RuntimeMenuAction::ToggleChatOverlay;
          case 3:
            return RuntimeMenuAction::ToggleNotifications;
          case 4:
            return RuntimeMenuAction::CycleTrackerDisplayMode;
          case 5:
            return RuntimeMenuAction::ToggleTrackerAutoFollow;
          case 6:
            controls_page_mode_ = RuntimeControlsPageMode::Sekaiemu;
            page_ = Page::InputSettings;
            selected_index_ = 0;
            scroll_offset_ = 0;
            break;
          case 7:
            controls_page_mode_ = RuntimeControlsPageMode::Core;
            page_ = Page::InputSettings;
            selected_index_ = 0;
            scroll_offset_ = 0;
            break;
          case 8:
            SetSettingsMode(RuntimeSettingsMode::Advanced);
            page_ = Page::CoreSettings;
            selected_index_ = 0;
            scroll_offset_ = 0;
            break;
          case 9:
            SetSettingsMode(RuntimeSettingsMode::Advanced);
            page_ = Page::BridgeStatus;
            selected_index_ = 0;
            scroll_offset_ = 0;
            break;
          case 10:
            page_ = Page::SyncInfo;
            selected_index_ = 0;
            scroll_offset_ = 0;
            break;
          default:
            break;
        }
        return RuntimeMenuAction::None;
      }
      if (page_ == Page::CoreSettings && selected_index_ < option_count) {
        if (const auto* definition =
                core_options.DefinitionAt(static_cast<std::size_t>(selected_index_))) {
          core_options.StepValue(definition->key, 1);
        }
      } else if (page_ == Page::CoreSettings) {
        const int action_index = selected_index_ - option_count;
        if (action_index >= 0 &&
            action_index < static_cast<int>(kCoreSettingsRows.size())) {
          return kCoreSettingsRows[static_cast<std::size_t>(action_index)].action;
        }
      } else if (page_ == Page::InputSettings && controls_page_mode_ == RuntimeControlsPageMode::Core) {
        input_state.ActivateInputMenuRow(static_cast<std::size_t>(selected_index_));
      } else if (page_ == Page::BridgeStatus) {
        return kBridgeRows[static_cast<std::size_t>(selected_index_)].action;
      }
      break;
    default:
      break;
  }

  if (page_ != Page::Main) {
    if (selected_index_ < scroll_offset_) {
      scroll_offset_ = selected_index_;
    } else if (selected_index_ >= scroll_offset_ + visible_rows_) {
      scroll_offset_ = selected_index_ - visible_rows_ + 1;
    }
  }

  return RuntimeMenuAction::None;
}

void RuntimeMenu::Render(OverlayCanvas& canvas,
                         const CoreOptionManager& core_options,
                         InputState& input_state,
                         const BridgeRuntimeStatus& bridge_status,
                         const TrackerRuntime* tracker_runtime,
                         const std::vector<SaveStateSlotMenuInfo>& save_slots,
                         std::string_view core_name,
                         std::string_view game_name,
                         int master_volume_percent,
                         bool chat_overlay_enabled,
                         bool notifications_enabled,
                         bool activity_feed_enabled,
                         bool bridge_terminal_enabled) {
  (void)activity_feed_enabled;
  canvas.Clear(UiColor{0, 0, 0, 0});
  canvas.FillRect(0, 0, static_cast<int>(canvas.Width()), static_cast<int>(canvas.Height()),
                  UiColor{0, 0, 0, 88});

  const int panel_width = std::min(static_cast<int>(canvas.Width()) - 120, 1120);
  const int panel_height = std::min(static_cast<int>(canvas.Height()) - 120, 760);
  const int panel_x = (static_cast<int>(canvas.Width()) - panel_width) / 2;
  const int panel_y = (static_cast<int>(canvas.Height()) - panel_height) / 2;
  canvas.FillRect(panel_x, panel_y, panel_width, panel_height, UiColor{17, 22, 33, 188});
  canvas.DrawRect(panel_x, panel_y, panel_width, panel_height, UiColor{92, 122, 196, 255});

  canvas.DrawText(panel_x + 24, panel_y + 20, "SEKAIEMU MENU", UiColor{245, 235, 210, 255}, 3);
  canvas.DrawText(panel_x + panel_width - 118,
                  panel_y + 24,
                  kSekaiemuVersion,
                  UiColor{130, 150, 190, 255},
                  1);
  canvas.DrawText(panel_x + 24,
                  panel_y + 52,
                  TruncateText(std::string("CORE: ") + std::string(core_name), 42),
                  UiColor{180, 205, 255, 255},
                  2);
  canvas.DrawWrappedText(panel_x + 24,
                         panel_y + 72,
                         panel_width - 48,
                         std::string("GAME: ") + std::string(game_name),
                         UiColor{180, 205, 255, 255},
                         2,
                         2);

  DrawRuntimeMenuCompletionHeader(canvas, panel_x + 24, panel_y + 104, panel_width - 48, tracker_runtime);

  const int tab_y = panel_y + 136;
  DrawRuntimeMenuTabs(canvas, panel_x + 24, tab_y, page_, settings_mode_, core_options);

  const int list_x = panel_x + 24;
  const int list_y = panel_y + 174;
  const int list_width = panel_width - 48;
  const int footer_height = 86;
  const int list_bottom = panel_y + panel_height - footer_height - 24;
  std::string tooltip;

  if (page_ == Page::Main) {
    int highlighted_slot = selected_index_ == 2 ? 0 : -1;
    if (main_slot_mode_ != MainSlotMode::None) {
      const auto& slot_rows = main_slot_mode_ == MainSlotMode::Save ? kSaveSlotRows : kLoadSlotRows;
      highlighted_slot = selected_index_ >= 0 && selected_index_ <= 3 ? selected_index_ + 1 : -1;
      DrawRuntimeMenuMainPage(canvas, list_x, list_y, list_width, list_bottom, selected_index_,
                              slot_rows, save_slots, highlighted_slot, tooltip);
    } else {
      DrawRuntimeMenuMainPage(canvas, list_x, list_y, list_width, list_bottom, selected_index_,
                              kMainRows, save_slots, highlighted_slot, tooltip);
    }
  } else if (page_ == Page::Settings) {
    DrawRuntimeMenuSettingsPage(canvas,
                                list_x,
                                list_y,
                                list_width,
                                list_bottom,
                                selected_index_,
                                settings_mode_,
                                chat_overlay_enabled,
                                notifications_enabled,
                                master_volume_percent,
                                tracker_runtime,
                                tooltip);
  } else if (page_ == Page::CoreSettings) {
    DrawRuntimeMenuCoreSettingsPage(canvas,
                                    list_x,
                                    list_y,
                                    list_width,
                                    list_bottom,
                                    selected_index_,
                                    scroll_offset_,
                                    visible_rows_,
                                    core_options,
                                    kCoreSettingsRows,
                                    tooltip);
  } else if (page_ == Page::InputSettings) {
    DrawRuntimeMenuControlsPage(canvas,
                                list_x,
                                list_y,
                                list_width,
                                list_bottom,
                                selected_index_,
                                scroll_offset_,
                                visible_rows_,
                                input_state,
                                settings_mode_,
                                controls_page_mode_,
                                tooltip);
  } else if (page_ == Page::BridgeStatus) {
    const int row_height = 20;
    const int field_y = list_y;
    const int field_width = list_width;
    const auto draw_field = [&](int row, std::string_view label, std::string_view value, UiColor value_color) {
      const int y = field_y + row * row_height;
      canvas.FillRect(list_x, y, field_width, row_height - 2, UiColor{24, 30, 42, 190});
      canvas.DrawText(list_x + 8, y + 6, TruncateText(label, 22), UiColor{255, 245, 225, 255}, 1);
      canvas.DrawText(list_x + 180, y + 6,
                      TruncateText(value, static_cast<std::size_t>(std::max(18, (field_width - 196) / 6))),
                      value_color, 1);
    };

    draw_field(0,
               "OWNER",
               bridge_status.owner == BridgeOwner::Sklmi ? "SKLMI" : "LEGACY",
               bridge_status.owner == BridgeOwner::Sklmi ? UiColor{170, 230, 180, 255}
                                                         : UiColor{180, 205, 255, 255});
    draw_field(1, "MIGRATED", bridge_status.migrated_game ? "YES" : "NO",
               bridge_status.migrated_game ? UiColor{255, 215, 150, 255}
                                           : UiColor{180, 205, 255, 255});
    draw_field(2, "STATUS",
               bridge_status.owner == BridgeOwner::Sklmi
                   ? (bridge_status.sklmi_active ? "RUNNING" : "STOPPED")
                   : (bridge_status.legacy_bridge_active ? "ACTIVE" : "IDLE"),
               bridge_status.owner == BridgeOwner::Sklmi
                   ? (bridge_status.sklmi_active ? UiColor{170, 230, 180, 255}
                                                : UiColor{255, 170, 170, 255})
                   : UiColor{180, 205, 255, 255});
    draw_field(3, "BRIDGE ID", bridge_status.bridge_id.empty() ? "-" : bridge_status.bridge_id,
               UiColor{180, 205, 255, 255});
    draw_field(4, "MEM SOCKET",
               bridge_status.runtime_memory_socket_path.empty() ? "-" : bridge_status.runtime_memory_socket_path,
               UiColor{180, 205, 255, 255});
    draw_field(5, "MANIFEST",
               bridge_status.manifest_path.empty() ? "-" : bridge_status.manifest_path,
               UiColor{180, 205, 255, 255});
    draw_field(6, "TRACE LOG",
               bridge_status.trace_log_path.empty() ? "-" : bridge_status.trace_log_path,
               UiColor{180, 205, 255, 255});
    draw_field(7, "ROOM STATE",
               bridge_status.room_state_path.empty() ? "-" : bridge_status.room_state_path,
               UiColor{180, 205, 255, 255});
    draw_field(8, "COMP LOG",
               bridge_status.companion_log_path.empty() ? "-" : bridge_status.companion_log_path,
               UiColor{180, 205, 255, 255});

    for (int index = 0; index < static_cast<int>(kBridgeRows.size()); ++index) {
      const int action_y = field_y + (10 + index) * row_height;
      const bool selected = selected_index_ == index;
      canvas.FillRect(list_x, action_y, list_width, row_height - 2,
                      selected ? UiColor{49, 70, 115, 255} : UiColor{24, 30, 42, 190});
      std::string label = kBridgeRows[static_cast<std::size_t>(index)].label;
      if (index == 1 && bridge_terminal_enabled) {
        label = "CLOSE BRIDGE TERMINAL";
      }
      canvas.DrawText(list_x + 8, action_y + 6, label, UiColor{255, 245, 225, 255}, 1);
      if (selected) {
        tooltip = kBridgeRows[static_cast<std::size_t>(index)].tooltip;
      }
    }
    if (!bridge_status.last_error.empty()) {
      tooltip += " LAST ERROR: " + bridge_status.last_error;
    } else if (bridge_status.owner == BridgeOwner::Legacy &&
               !bridge_status.legacy_bridge_socket_path.empty()) {
      tooltip += " LEGACY SOCKET: " + bridge_status.legacy_bridge_socket_path;
    }
  } else {
    DrawRuntimeMenuSyncInfoPage(canvas,
                                list_x,
                                list_y,
                                list_width,
                                list_bottom,
                                bridge_status,
                                tracker_runtime);
    tooltip = "SYNC INFOS ARE READ FROM THE CURRENT SKLMI TRACKER SNAPSHOT.";
  }

  const int footer_y = panel_y + panel_height - footer_height;
  canvas.FillRect(panel_x + 24, footer_y, panel_width - 48, 62,
                  UiColor{10, 14, 22, 180});
  canvas.DrawRect(panel_x + 24, footer_y, panel_width - 48, 62,
                  UiColor{65, 85, 120, 255});
  canvas.DrawText(panel_x + 36, footer_y + 10,
                  "ESC CLOSE  TAB PAGE  ENTER SELECT/APPLY",
                  UiColor{180, 205, 255, 255},
                  1);
  canvas.DrawText(panel_x + 36, footer_y + 24,
                  page_ == Page::Settings ? "ENTER OPEN/TOGGLE  LEFT RIGHT MODE"
                                           : page_ == Page::InputSettings ? "LEFT RIGHT SELECT CONTROLLER"
                                                                          : page_ == Page::BridgeStatus ? "ENTER RESTART BRIDGE"
                                                                                                        : page_ == Page::SyncInfo ? "READ ONLY"
                                                                                                                                   : "LEFT RIGHT CHANGE VALUE",
                  UiColor{180, 205, 255, 255},
                  1);
  canvas.DrawWrappedText(panel_x + 36, footer_y + 38, panel_width - 72,
                         tooltip, UiColor{255, 245, 225, 255}, 1, 2);
}

}  // namespace sekaiemu::spike
