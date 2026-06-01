#pragma once

#include "input_script.hpp"
#include "input_state.hpp"
#include "libretro_core_api.hpp"
#include "save_state_manager.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace sekaiemu::spike {

bool InitializeInputScriptForHost(const std::filesystem::path& input_script_path,
                                  InputScriptPlayer& input_script,
                                  std::string& last_error);
void ApplyInputScriptFrameForHost(const InputScriptPlayer& input_script,
                                  bool quit_after_end,
                                  std::uint64_t frame_counter,
                                  InputState& input_state,
                                  bool& running);
bool ApplyStartupStateActionsForHost(bool load_state_on_start,
                                     SaveStateManager& save_state_manager,
                                     CoreApi& core,
                                     std::string& last_error);
void ApplyFrameAutomationForHost(std::uint64_t frame_counter,
                                 std::uint64_t save_state_at_frame,
                                 std::uint64_t quit_after_frame,
                                 bool& scheduled_state_saved,
                                 bool& running,
                                 const std::function<void()>& apply_input_script_frame,
                                 const std::function<void()>& save_state_now);

}  // namespace sekaiemu::spike
