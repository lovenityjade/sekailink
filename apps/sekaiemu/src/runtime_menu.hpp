#pragma once

#include "bridge_runtime_status.hpp"
#include "core_option_manager.hpp"
#include "input_state.hpp"
#include "overlay_canvas.hpp"
#include "runtime_settings_mode.hpp"
#include "runtime_menu_controls.hpp"

#include <SDL.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

class TrackerRuntime;

enum class RuntimeMenuAction {
  None,
  ResetCore,
  SaveBattery,
  LoadBattery,
  SaveState,
  LoadState,
  SaveStateSlot1,
  SaveStateSlot2,
  SaveStateSlot3,
  SaveStateSlot4,
  LoadStateSlot1,
  LoadStateSlot2,
  LoadStateSlot3,
  LoadStateSlot4,
  ToggleChatOverlay,
  ToggleNotifications,
  DecreaseMasterVolume,
  IncreaseMasterVolume,
  CycleTrackerDisplayMode,
  ToggleTrackerScreen,
  ToggleTrackerAutoFollow,
  ApplyCoreSettings,
  ResetCoreSettingsToDefaults,
  RestartBridge,
  ToggleBridgeTerminal,
  ChangeGame,
  CloseMenu,
  QuitRuntime,
};

struct RuntimeMenuMainActionRow {
  std::string label;
  std::string tooltip;
  RuntimeMenuAction action = RuntimeMenuAction::None;
};

struct RuntimeMenuCoreActionRow {
  const char* label = "";
  const char* tooltip = "";
  RuntimeMenuAction action = RuntimeMenuAction::None;
};

struct SaveStateSlotMenuInfo {
  int slot_index = 0;
  bool has_state = false;
  bool has_screenshot = false;
  unsigned screenshot_width = 0;
  unsigned screenshot_height = 0;
  std::vector<std::uint8_t> screenshot_rgba;
  std::string label;
  std::string created_at;
  std::string completion;
  std::string detail;
};

class RuntimeMenu {
 public:
  static constexpr int kMainActionCount = 6;
  static constexpr int kMainSlotActionCount = 5;

  enum class Page {
    Main,
    Settings,
    CoreSettings,
    InputSettings,
    BridgeStatus,
    SyncInfo,
  };

  void Toggle();
  void Open();
  void OpenShortcutHelp();
  void Close();
  bool Visible() const { return visible_; }
  RuntimeSettingsMode SettingsMode() const { return settings_mode_; }
  void SetSettingsMode(RuntimeSettingsMode mode);
  bool ConsumeSettingsModeChanged();

  RuntimeMenuAction HandleKey(SDL_Scancode scancode,
                              CoreOptionManager& core_options,
                              InputState& input_state);
  void Render(OverlayCanvas& canvas,
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
              bool bridge_terminal_enabled);
  void RenderImGui(const CoreOptionManager& core_options,
                   InputState& input_state,
                   const BridgeRuntimeStatus& bridge_status,
                   const TrackerRuntime* tracker_runtime,
                   const std::vector<SaveStateSlotMenuInfo>& save_slots,
                   std::string_view core_name,
                   std::string_view game_name,
                   int master_volume_percent,
                   bool chat_overlay_enabled,
                   bool notifications_enabled,
                   bool bridge_terminal_enabled);
  RuntimeMenuAction ConsumePendingAction();
  void QueueAction(RuntimeMenuAction action) { pending_action_ = action; }

 private:
  enum class MainSlotMode {
    None,
    Save,
    Load,
  };

  Page page_ = Page::Main;
  MainSlotMode main_slot_mode_ = MainSlotMode::None;
  RuntimeControlsPageMode controls_page_mode_ = RuntimeControlsPageMode::Core;
  RuntimeSettingsMode settings_mode_ = RuntimeSettingsMode::Easy;
  bool settings_mode_changed_ = false;
  bool visible_ = false;
  int selected_index_ = 0;
  int scroll_offset_ = 0;
  int visible_rows_ = 8;
  RuntimeMenuAction pending_action_ = RuntimeMenuAction::None;
};

}  // namespace sekaiemu::spike
