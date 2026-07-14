#include "libretro_menu_actions.hpp"

namespace sekaiemu::spike {

void ApplyRuntimeMenuAction(RuntimeMenuAction action,
                            RuntimeMenu& runtime_menu,
                            CoreOptionManager& core_option_manager,
                            bool& running,
                            const std::function<void()>& reset_core,
                            const std::function<void()>& save_battery,
                            const std::function<void()>& load_battery,
                            const std::function<void(int)>& save_state,
                            const std::function<void(int)>& load_state,
                            const std::function<void()>& restart_bridge,
                            const std::function<void()>& toggle_chat_overlay) {
  switch (action) {
    case RuntimeMenuAction::None:
      return;
    case RuntimeMenuAction::ResetCore:
      reset_core();
      break;
    case RuntimeMenuAction::SaveBattery:
      save_battery();
      break;
    case RuntimeMenuAction::LoadBattery:
      load_battery();
      break;
    case RuntimeMenuAction::SaveState:
      save_state(0);
      break;
    case RuntimeMenuAction::LoadState:
      load_state(0);
      break;
    case RuntimeMenuAction::SaveStateSlot1:
      save_state(1);
      break;
    case RuntimeMenuAction::SaveStateSlot2:
      save_state(2);
      break;
    case RuntimeMenuAction::SaveStateSlot3:
      save_state(3);
      break;
    case RuntimeMenuAction::SaveStateSlot4:
      save_state(4);
      break;
    case RuntimeMenuAction::LoadStateSlot1:
      load_state(1);
      break;
    case RuntimeMenuAction::LoadStateSlot2:
      load_state(2);
      break;
    case RuntimeMenuAction::LoadStateSlot3:
      load_state(3);
      break;
    case RuntimeMenuAction::LoadStateSlot4:
      load_state(4);
      break;
    case RuntimeMenuAction::ToggleChatOverlay:
      toggle_chat_overlay();
      break;
    case RuntimeMenuAction::DecreaseMasterVolume:
    case RuntimeMenuAction::IncreaseMasterVolume:
    case RuntimeMenuAction::ToggleNotifications:
    case RuntimeMenuAction::ToggleActivityFeed:
    case RuntimeMenuAction::CycleTrackerDisplayMode:
    case RuntimeMenuAction::ToggleTrackerScreen:
    case RuntimeMenuAction::ToggleTrackerAutoFollow:
    case RuntimeMenuAction::ToggleBridgeTerminal:
    case RuntimeMenuAction::CycleWindowMode:
      break;
    case RuntimeMenuAction::ApplyCoreSettings:
      core_option_manager.ApplyPendingChanges();
      break;
    case RuntimeMenuAction::ResetCoreSettingsToDefaults:
      core_option_manager.ResetPendingToDefaults();
      break;
    case RuntimeMenuAction::RestartBridge:
      restart_bridge();
      break;
    case RuntimeMenuAction::ChangeGame:
      running = false;
      break;
    case RuntimeMenuAction::CloseMenu:
      core_option_manager.DiscardPendingChanges();
      runtime_menu.Close();
      break;
    case RuntimeMenuAction::QuitRuntime:
      running = false;
      break;
  }
}

}  // namespace sekaiemu::spike
