#include "libretro_menu_presentation.hpp"

#include "overlay_canvas.hpp"
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
                              const TrackerRuntime* tracker_runtime,
                              const SaveStateManager& save_state_manager,
                              const VideoGeometry& geometry,
                              const std::string& core_name,
                              const std::string& game_name,
                              TrackerWindowPresenter& tracker_window_presenter,
                              const std::function<void()>& render_tracker_presentation,
                              int master_volume_percent,
                              bool chat_overlay_enabled,
                              bool notifications_enabled,
                              bool bridge_terminal_enabled) {
  if (!video_backend) {
    return;
  }
  if (!runtime_menu.Visible()) {
    render_tracker_presentation();
    return;
  }

  tracker_window_presenter.Shutdown();
  const auto [menu_width, menu_height] = MenuCanvasDimensions(geometry);
  OverlayCanvas canvas(menu_width, menu_height);
  const auto save_slots = LoadSaveStateSlotMenuInfos(save_state_manager);
  runtime_menu.Render(canvas,
                      core_option_manager,
                      input_state,
                      bridge_status,
                      tracker_runtime,
                      save_slots,
                      core_name,
                      game_name,
                      master_volume_percent,
                      chat_overlay_enabled,
                      notifications_enabled,
                      bridge_terminal_enabled);
  video_backend->UploadOverlayFrame(canvas.Data(), canvas.Width(), canvas.Height());
}

}  // namespace sekaiemu::spike
