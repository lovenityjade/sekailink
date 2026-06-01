#include "libretro_frame_automation.hpp"

#include <iostream>

namespace sekaiemu::spike {

bool InitializeInputScriptForHost(const std::filesystem::path& input_script_path,
                                  InputScriptPlayer& input_script,
                                  std::string& last_error) {
  if (input_script_path.empty()) {
    return true;
  }
  std::string error;
  if (!input_script.LoadFromFile(input_script_path, error)) {
    last_error = error;
    return false;
  }
  std::cerr << "[sekaiemu] input script active: " << input_script_path
            << " last_frame=" << input_script.LastFrame() << "\n";
  return true;
}

void ApplyInputScriptFrameForHost(const InputScriptPlayer& input_script,
                                  bool quit_after_end,
                                  std::uint64_t frame_counter,
                                  InputState& input_state,
                                  bool& running) {
  if (input_script.Empty()) {
    return;
  }
  if (quit_after_end && frame_counter > input_script.LastFrame()) {
    std::cerr << "[sekaiemu] input script completed; quitting runtime.\n";
    running = false;
    return;
  }
  input_state.SetScriptedControls(input_script.ControlsForFrame(frame_counter));
}

bool ApplyStartupStateActionsForHost(bool load_state_on_start,
                                     SaveStateManager& save_state_manager,
                                     CoreApi& core,
                                     std::string& last_error) {
  if (!load_state_on_start) {
    return true;
  }
  std::string error;
  if (!save_state_manager.LoadState(core, error)) {
    last_error = "startup load-state failed: " + error;
    return false;
  }
  save_state_manager.RefreshBatteryTracking(core);
  std::cerr << "[sekaiemu] startup state loaded: "
            << save_state_manager.StatePath() << "\n";
  return true;
}

void ApplyFrameAutomationForHost(std::uint64_t frame_counter,
                                 std::uint64_t save_state_at_frame,
                                 std::uint64_t quit_after_frame,
                                 bool& scheduled_state_saved,
                                 bool& running,
                                 const std::function<void()>& apply_input_script_frame,
                                 const std::function<void()>& save_state_now) {
  apply_input_script_frame();
  if (!scheduled_state_saved && save_state_at_frame > 0 &&
      frame_counter >= save_state_at_frame) {
    save_state_now();
    scheduled_state_saved = true;
  }
  if (quit_after_frame > 0 && frame_counter >= quit_after_frame) {
    std::cerr << "[sekaiemu] quit-after-frame reached: "
              << quit_after_frame << "\n";
    running = false;
  }
}

}  // namespace sekaiemu::spike
