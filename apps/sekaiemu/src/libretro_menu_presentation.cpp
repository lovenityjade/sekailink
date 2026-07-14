#include "libretro_menu_presentation.hpp"

#include "imgui_tracker_renderer.hpp"
#include "overlay_canvas.hpp"
#include "runtime_activity_feed_imgui.hpp"
#include "runtime_menu_save_slots.hpp"

#include <algorithm>

namespace sekaiemu::spike {
namespace {

std::pair<unsigned, unsigned> MenuCanvasDimensions(const VideoGeometry& geometry) {
  constexpr unsigned kMenuMinimumWidth = 960;
  constexpr unsigned kMenuMinimumHeight = 720;

  const unsigned base_width = geometry.width == 0 ? 640u : geometry.width;
  const unsigned base_height = geometry.height == 0 ? 480u : geometry.height;

  const unsigned scale_x = std::max(1u, (kMenuMinimumWidth + base_width - 1) / base_width);
  const unsigned scale_y = std::max(1u, (kMenuMinimumHeight + base_height - 1) / base_height);
  const unsigned scale = std::max(scale_x, scale_y);
  return {base_width * scale, base_height * scale};
}

}  // namespace

void PresentFrameForHost(VideoBackend* video_backend,
                         TrackerWindowPresenter& tracker_window_presenter,
                         bool menu_visible,
                         bool& frame_ready) {
  if (!video_backend || (!frame_ready && !menu_visible)) {
    return;
  }

  video_backend->Present();
  tracker_window_presenter.Present();
  frame_ready = false;
}

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
                              const std::function<void(std::string_view)>& send_chat_command) {
  if (!video_backend) {
    return;
  }
  if (!runtime_menu.Visible()) {
    if (tracker_runtime != nullptr) {
      const auto mode = tracker_runtime->UiState().display_mode;
      const bool show_tracker_screen = tracker_visual_active && tracker_runtime->UiState().show_tracker_screen;
      const unsigned sidebar_width =
          show_tracker_screen && mode == TrackerDisplayMode::SplitScreen
              ? std::max(360u, (geometry.height == 0 ? 224u : geometry.height) * 2u)
              : 0u;
      video_backend->SetTrackerSidebarLayout(show_tracker_screen && mode == TrackerDisplayMode::SplitScreen,
                                             sidebar_width,
                                             geometry);
      video_backend->SetImGuiDrawCallback([tracker_runtime,
                                           tracker_visual_active,
                                           &runtime_menu,
                                           &send_chat_command,
                                           render_client_core_hud,
                                           render_goal_completion]() {
        if (tracker_visual_active && tracker_runtime->UiState().show_tracker_screen) {
          RenderTrackerImGui(*tracker_runtime);
        }
        RenderRuntimeContextMenuImGui(runtime_menu, tracker_runtime, send_chat_command);
        if (render_client_core_hud) {
          render_client_core_hud();
        }
        if (render_goal_completion) {
          render_goal_completion();
        }
      });
    } else {
      video_backend->SetTrackerSidebarLayout(false, 0, geometry);
      video_backend->SetImGuiDrawCallback([&runtime_menu,
                                           &send_chat_command,
                                           render_client_core_hud,
                                           render_goal_completion]() {
        RenderRuntimeContextMenuImGui(runtime_menu, nullptr, send_chat_command);
        if (render_client_core_hud) {
          render_client_core_hud();
        }
        if (render_goal_completion) {
          render_goal_completion();
        }
      });
    }
    video_backend->ClearOverlay();
    if (tracker_visual_active && render_tracker_presentation) {
      render_tracker_presentation();
    }
    return;
  }

  tracker_window_presenter.Shutdown();
  video_backend->SetTrackerSidebarLayout(false, 0, geometry);
  const auto save_slots = LoadSaveStateSlotMenuInfos(save_state_manager);
  video_backend->ClearOverlay();
  const BridgeRuntimeStatus bridge_status_snapshot = bridge_status;
  video_backend->SetImGuiDrawCallback([&runtime_menu,
                                       &core_option_manager,
                                       &input_state,
                                       bridge_status_snapshot,
                                       tracker_runtime,
                                       tracker_visual_active,
                                       save_slots,
                                       core_name,
                                       game_name,
                                       master_volume_percent,
                                       chat_overlay_enabled,
                                       notifications_enabled,
                                       activity_feed_enabled,
                                       bridge_terminal_enabled]() mutable {
    runtime_menu.RenderImGui(core_option_manager,
                             input_state,
                             bridge_status_snapshot,
                             tracker_visual_active ? tracker_runtime : nullptr,
                             save_slots,
                             core_name,
                             game_name,
                             master_volume_percent,
                             chat_overlay_enabled,
                             notifications_enabled,
                             activity_feed_enabled,
                             bridge_terminal_enabled);
  });
}

}  // namespace sekaiemu::spike
