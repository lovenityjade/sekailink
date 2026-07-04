#pragma once

#include "core_option_manager.hpp"
#include "input_state.hpp"
#include "libretro_core_api.hpp"
#include "libretro_host.hpp"
#include "runtime_menu.hpp"

#include <SDL.h>
#include <libretro.h>

#include <cstdint>
#include <functional>
#include <string_view>

namespace sekaiemu::spike {

struct RuntimeLoopContext {
  HostOptions* options = nullptr;
  CoreApi* core = nullptr;
  RuntimeMenu* runtime_menu = nullptr;
  CoreOptionManager* core_option_manager = nullptr;
  InputState* input_state = nullptr;
  retro_audio_buffer_status_callback_t* audio_buffer_status_callback = nullptr;
  std::uint64_t* frame_counter = nullptr;
  double target_frame_rate = 0.0;
  bool* running = nullptr;
  int* exit_code = nullptr;

  std::function<void()> on_core_run;
  std::function<void()> on_tick_profile_bridge;
  std::function<void()> on_tick_sklmi_companion;
  std::function<void()> on_tick_memory_server;
  std::function<void()> on_tick_battery_persistence;
  std::function<void()> on_dump_memory_snapshot;
  std::function<void()> on_save_battery;
  std::function<void()> on_load_battery;
  std::function<void()> on_save_state;
  std::function<void()> on_load_state;
  std::function<void()> on_begin_frame;
  std::function<void()> on_emulation_frame_ran;
  std::function<bool()> core_run_enabled;
  std::function<void()> on_audio_buffer_status;
  std::function<void()> on_cycle_tracker_display_mode;
  std::function<void()> on_toggle_tracker_screen;
  std::function<void()> on_cycle_tracker_tab;
  std::function<void()> on_toggle_tracker_auto_follow;
  std::function<void()> on_toggle_fullscreen;
  std::function<void()> on_test_goal_completion;
  std::function<void()> on_open_tracker_map_menu;
  std::function<bool(int, int)> on_open_tracker_map_menu_at;
  std::function<bool()> on_activate_tracker_map_menu;
  std::function<bool(int, int)> on_activate_tracker_map_menu_at;
  std::function<bool(int, int)> on_hover_tracker_map_menu_at;
  std::function<bool(int, int, std::string_view)> on_click_tracker_at;
  std::function<bool(int, int)> on_hover_tracker_at;
  std::function<void(int)> on_step_tracker_map_menu;
  std::function<void()> on_close_tracker_map_menu;
  std::function<bool()> tracker_map_menu_visible;
  std::function<bool()> chat_overlay_enabled;
  std::function<bool()> chat_typing_active;
  std::function<void()> on_open_chat_input;
  std::function<void()> on_cancel_chat_input;
  std::function<void()> on_submit_chat_input;
  std::function<void()> on_backspace_chat_input;
  std::function<void()> on_autocomplete_chat_input;
  std::function<void(std::string_view)> on_append_chat_input;
  std::function<bool(const SDL_Event&)> on_runtime_debug_event;
  std::function<bool(const SDL_Event&)> on_goal_completion_event;
  std::function<void()> on_present_frame;
  std::function<void()> on_update_menu_overlay;
  std::function<void()> on_reset_core;
  std::function<void(RuntimeMenuAction)> on_apply_menu_action;
  std::function<void(bool)> on_menu_visibility_changed;
};

void PumpRuntimeEvents(RuntimeLoopContext& context);
int RunRuntimeLoop(RuntimeLoopContext& context);

}  // namespace sekaiemu::spike
