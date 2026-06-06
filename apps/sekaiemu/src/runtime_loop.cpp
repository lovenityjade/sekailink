#include "runtime_loop.hpp"

#include <algorithm>
#include <cmath>

namespace sekaiemu::spike {

namespace {

double ResolvePacingFrameRate(double core_fps) {
  if (!std::isfinite(core_fps) || core_fps <= 1.0) {
    return 60.0;
  }
  const double rounded = std::round(core_fps);
  if (rounded > 1.0 && std::abs(core_fps - rounded) / rounded <= 0.05) {
    return rounded;
  }
  return std::clamp(core_fps, 10.0, 240.0);
}

class FramePacer {
 public:
  explicit FramePacer(double target_frame_rate)
      : frequency_(static_cast<double>(SDL_GetPerformanceFrequency())),
        frame_ticks_(frequency_ / ResolvePacingFrameRate(target_frame_rate)),
        next_tick_(static_cast<double>(SDL_GetPerformanceCounter())) {}

  void WaitForNextFrame() {
    if (frequency_ <= 0.0 || frame_ticks_ <= 0.0) {
      return;
    }
    next_tick_ += frame_ticks_;
    double now = static_cast<double>(SDL_GetPerformanceCounter());
    if (now > next_tick_ + frame_ticks_) {
      next_tick_ = now;
      return;
    }
    while (now < next_tick_) {
      const double remaining_ms = ((next_tick_ - now) * 1000.0) / frequency_;
      if (remaining_ms > 2.0) {
        SDL_Delay(static_cast<Uint32>(remaining_ms - 1.0));
      } else {
        SDL_Delay(0);
      }
      now = static_cast<double>(SDL_GetPerformanceCounter());
    }
  }

 private:
  double frequency_ = 0.0;
  double frame_ticks_ = 0.0;
  double next_tick_ = 0.0;
};

}  // namespace

void PumpRuntimeEvents(RuntimeLoopContext& context) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        *context.running = false;
        break;
      case SDL_TEXTINPUT:
        if (context.chat_typing_active && context.chat_typing_active() && context.on_append_chat_input) {
          context.on_append_chat_input(event.text.text);
        }
        break;
      case SDL_KEYDOWN:
        if (event.key.repeat == 0 &&
            (event.key.keysym.scancode == SDL_SCANCODE_F12 ||
             (event.key.keysym.scancode == SDL_SCANCODE_RETURN &&
              (event.key.keysym.mod & KMOD_ALT) != 0))) {
          if (context.on_toggle_fullscreen) {
            context.on_toggle_fullscreen();
          }
          break;
        }
        if (context.chat_typing_active && context.chat_typing_active()) {
          if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            if (context.on_cancel_chat_input) {
              context.on_cancel_chat_input();
            }
          } else if (event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
                     event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) {
            if (context.on_submit_chat_input) {
              context.on_submit_chat_input();
            }
          } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
            if (context.on_backspace_chat_input) {
              context.on_backspace_chat_input();
            }
          } else if (event.key.keysym.scancode == SDL_SCANCODE_TAB) {
            if (event.key.repeat == 0 && context.on_autocomplete_chat_input) {
              context.on_autocomplete_chat_input();
            }
          } else if (event.key.keysym.scancode == SDL_SCANCODE_V &&
                     (event.key.keysym.mod & KMOD_CTRL) != 0) {
            if (context.on_append_chat_input) {
              char* text = SDL_GetClipboardText();
              if (text) {
                context.on_append_chat_input(text);
                SDL_free(text);
              }
            }
          }
          break;
        }
        if (context.runtime_menu->Visible() && context.input_state->HandleSdlEvent(event, true)) {
          break;
        }
        if (!context.runtime_menu->Visible() && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE &&
            context.tracker_map_menu_visible && context.tracker_map_menu_visible()) {
          if (context.on_close_tracker_map_menu) {
            context.on_close_tracker_map_menu();
          }
          break;
        }
        if (context.runtime_menu->Visible()) {
          if (event.key.repeat == 0 && event.key.keysym.scancode == SDL_SCANCODE_F1) {
            context.runtime_menu->OpenShortcutHelp();
            context.on_menu_visibility_changed(true);
          } else {
            const auto action =
                context.runtime_menu->HandleKey(event.key.keysym.scancode,
                                               *context.core_option_manager,
                                               *context.input_state);
            context.on_apply_menu_action(action);
            context.on_menu_visibility_changed(context.runtime_menu->Visible());
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          context.runtime_menu->Open();
          context.on_menu_visibility_changed(true);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F1) {
          if (event.key.repeat == 0) {
            context.runtime_menu->OpenShortcutHelp();
            context.on_menu_visibility_changed(true);
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
          context.on_save_battery();
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
          context.on_load_battery();
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F5) {
          context.on_dump_memory_snapshot();
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F6) {
          context.on_save_state();
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F7) {
          context.on_load_state();
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F8) {
          if (context.on_cycle_tracker_display_mode) {
            context.on_cycle_tracker_display_mode();
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F9) {
          if (context.on_toggle_tracker_screen) {
            context.on_toggle_tracker_screen();
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_TAB) {
          if (event.key.repeat == 0 && context.on_toggle_tracker_screen) {
            context.on_toggle_tracker_screen();
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_T) {
          if (event.key.repeat == 0 &&
              (!context.chat_overlay_enabled || context.chat_overlay_enabled()) &&
              context.on_open_chat_input) {
            context.on_open_chat_input();
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
          if (context.on_cycle_tracker_tab) {
            context.on_cycle_tracker_tab();
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_F11) {
          if (context.on_toggle_tracker_auto_follow) {
            context.on_toggle_tracker_auto_follow();
          }
        } else {
          context.input_state->HandleSdlEvent(event, false);
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (!context.runtime_menu->Visible() && context.tracker_map_menu_visible &&
            context.tracker_map_menu_visible()) {
          if (event.button.button == SDL_BUTTON_LEFT) {
            if (context.on_activate_tracker_map_menu_at &&
                context.on_activate_tracker_map_menu_at(event.button.x, event.button.y)) {
              break;
            }
            if (!context.on_activate_tracker_map_menu_at && context.on_activate_tracker_map_menu &&
                context.on_activate_tracker_map_menu()) {
              break;
            }
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            if (context.on_open_tracker_map_menu_at &&
                context.on_open_tracker_map_menu_at(event.button.x, event.button.y)) {
              break;
            }
          }
        }
        if (!context.runtime_menu->Visible() && event.button.button == SDL_BUTTON_RIGHT) {
          if (context.on_open_tracker_map_menu_at &&
              context.on_open_tracker_map_menu_at(event.button.x, event.button.y)) {
            break;
          }
          if (context.on_open_tracker_map_menu) {
            context.on_open_tracker_map_menu();
            break;
          }
        }
        if (!context.runtime_menu->Visible() &&
            event.button.button == SDL_BUTTON_LEFT &&
            context.on_click_tracker_at &&
            context.on_click_tracker_at(event.button.x,
                                        event.button.y,
                                        "left")) {
          break;
        }
        break;
      case SDL_MOUSEWHEEL:
        if (!context.runtime_menu->Visible() && context.tracker_map_menu_visible &&
            context.tracker_map_menu_visible() && context.on_step_tracker_map_menu) {
          context.on_step_tracker_map_menu(event.wheel.y < 0 ? 1 : -1);
          break;
        }
        break;
      case SDL_MOUSEMOTION:
        if (!context.runtime_menu->Visible() && context.tracker_map_menu_visible &&
            context.tracker_map_menu_visible() && context.on_hover_tracker_map_menu_at &&
            context.on_hover_tracker_map_menu_at(event.motion.x, event.motion.y)) {
          break;
        }
        if (!context.runtime_menu->Visible() && context.on_hover_tracker_at &&
            context.on_hover_tracker_at(event.motion.x, event.motion.y)) {
          break;
        }
        break;
      case SDL_KEYUP:
        context.input_state->HandleSdlEvent(event, context.runtime_menu->Visible());
        break;
      case SDL_CONTROLLERDEVICEADDED:
      case SDL_CONTROLLERDEVICEREMOVED:
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERBUTTONUP:
      case SDL_CONTROLLERAXISMOTION:
        if (!context.input_state->HandleSdlEvent(event, context.runtime_menu->Visible()) &&
            context.runtime_menu->Visible() && event.type == SDL_CONTROLLERBUTTONDOWN &&
            event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
          const auto action =
              context.runtime_menu->HandleKey(SDL_SCANCODE_RETURN,
                                             *context.core_option_manager,
                                             *context.input_state);
          context.on_apply_menu_action(action);
          context.on_menu_visibility_changed(context.runtime_menu->Visible());
        }
        break;
      default:
        break;
    }
  }
}

int RunRuntimeLoop(RuntimeLoopContext& context) {
  int runtime_exit_code = 0;
  if (context.exit_code) {
    *context.exit_code = 0;
  }
  FramePacer frame_pacer(context.target_frame_rate);
  while (*context.running) {
    ++(*context.frame_counter);
    context.input_state->BeginFrame();
    if (context.on_begin_frame) {
      context.on_begin_frame();
    }
    PumpRuntimeEvents(context);
    if (!context.runtime_menu->Visible()) {
      if (context.on_audio_buffer_status) {
        context.on_audio_buffer_status();
      } else if (context.audio_buffer_status_callback && *context.audio_buffer_status_callback) {
        (*context.audio_buffer_status_callback)(true, 50, false);
      }
      if (!context.core_run_enabled || context.core_run_enabled()) {
        context.on_core_run();
      }
      context.on_tick_memory_server();
      context.on_tick_sklmi_companion();
      context.on_tick_profile_bridge();
      context.on_tick_battery_persistence();
    } else {
      context.on_tick_memory_server();
      context.on_tick_sklmi_companion();
    }
    context.on_update_menu_overlay();
    context.on_present_frame();
    if (context.exit_code) {
      runtime_exit_code = *context.exit_code;
    }
    frame_pacer.WaitForNextFrame();
  }
  return runtime_exit_code;
}

}  // namespace sekaiemu::spike
