#pragma once

#include "bridge_runtime_status.hpp"
#include "core_option_manager.hpp"
#include "input_state.hpp"
#include "runtime_menu.hpp"
#include "save_state_manager.hpp"
#include "tracker_window_presenter.hpp"
#include "video_backend.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace sekaiemu::spike {

class TrackerRuntime;

void PresentFrameForHost(VideoBackend* video_backend,
                         TrackerWindowPresenter& tracker_window_presenter,
                         bool menu_visible,
                         bool& frame_ready);

void UpdateMenuOverlayForHost(VideoBackend* video_backend,
                              RuntimeMenu& runtime_menu,
                              CoreOptionManager& core_option_manager,
                              InputState& input_state,
                              const BridgeRuntimeStatus& bridge_status,
                              TrackerRuntime* tracker_runtime,
                              bool tracker_visual_active,
                              const SaveStateManager& save_state_manager,
                              const VideoGeometry& geometry,
                              const std::string& core_name,
                              const std::string& game_name,
                              TrackerWindowPresenter& tracker_window_presenter,
                              const std::function<void()>& render_tracker_presentation,
                              const std::function<void()>& render_client_core_hud,
                              const std::function<void()>& render_goal_completion,
                              int master_volume_percent,
                              bool chat_overlay_enabled,
                              bool notifications_enabled,
                              bool activity_feed_enabled,
                              bool bridge_terminal_enabled,
                              const std::function<void(std::string_view)>& send_chat_command);

}  // namespace sekaiemu::spike
