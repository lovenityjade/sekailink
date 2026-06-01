#include "libretro_tracker_presentation.hpp"

#include "overlay_canvas.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_snapshot_io.hpp"
#include "tracker_window_presenter.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {
namespace {

void RenderTrackerOverlayMode(TrackerRuntime& tracker_runtime,
                              VideoBackend& video_backend,
                              const TrackerOverlayAssetResolver* asset_resolver,
                              const VideoGeometry& geometry,
                              const TrackerResolvedViewState& resolved,
                              TrackerDisplayMode mode,
                              const std::function<void(OverlayCanvas&, int, int)>& draw_extra_overlay) {
  const unsigned game_width = std::max(geometry.width, 256u);
  const unsigned game_height = std::max(geometry.height, 224u);
  unsigned width = std::max(game_width, 640u);
  unsigned height = std::max(game_height, 360u);
  int game_area_width = static_cast<int>(width);
  int game_area_height = static_cast<int>(height);
  OverlayCanvas canvas(width, height);
  canvas.Clear({0, 0, 0, 0});

  TrackerPanelLayout layout;
  bool compact = false;
  std::string header_suffix;

  switch (mode) {
    case TrackerDisplayMode::PipOverlay:
      compact = true;
      layout.width = static_cast<int>(width * 0.42);
      layout.height = static_cast<int>(height * 0.45);
      layout.x = static_cast<int>(width) - layout.width - 12;
      layout.y = static_cast<int>(height) - layout.height - 12;
      header_suffix = "PIP";
      break;
    case TrackerDisplayMode::ToggleScreen:
      compact = false;
      layout.x = 0;
      layout.y = 0;
      layout.width = static_cast<int>(width);
      layout.height = static_cast<int>(height);
      header_suffix = "TRACKER";
      break;
    case TrackerDisplayMode::SplitScreen:
    default:
      const unsigned tracker_sidebar_width = std::max(360u, game_height * 2u);
      video_backend.SetTrackerSidebarLayout(true, tracker_sidebar_width, geometry);
      width = game_width * 3u + tracker_sidebar_width;
      height = game_height * 3u;
      game_area_width = static_cast<int>(game_width * 3u);
      game_area_height = static_cast<int>(height);
      canvas = OverlayCanvas(width, height);
      canvas.Clear({0, 0, 0, 0});
      compact = false;
      layout.width = static_cast<int>(tracker_sidebar_width);
      layout.height = static_cast<int>(height);
      layout.x = static_cast<int>(game_width * 3u);
      layout.y = 0;
      header_suffix = "SPLIT";
      break;
  }

  RenderTrackerPanel(canvas, tracker_runtime, resolved, layout, compact, header_suffix, asset_resolver);
  if (draw_extra_overlay) {
    draw_extra_overlay(canvas, game_area_width, game_area_height);
  }
  video_backend.UploadOverlayFrame(canvas.Data(), canvas.Width(), canvas.Height());
}

void RenderExtraOverlayOnly(VideoBackend& video_backend,
                            const VideoGeometry& geometry,
                            const std::function<void(OverlayCanvas&, int, int)>& draw_extra_overlay) {
  const unsigned game_width = std::max(geometry.width, 256u);
  const unsigned game_height = std::max(geometry.height, 224u);
  const unsigned width = std::max(game_width, 640u);
  const unsigned height = std::max(game_height, 360u);
  OverlayCanvas canvas(width, height);
  canvas.Clear({0, 0, 0, 0});
  if (draw_extra_overlay) {
    draw_extra_overlay(canvas, static_cast<int>(width), static_cast<int>(height));
  }
  video_backend.UploadOverlayFrame(canvas.Data(), canvas.Width(), canvas.Height());
}

}  // namespace

void EmitTrackerCommand(const std::filesystem::path& command_log_path,
                        nlohmann::json command) {
  if (command_log_path.empty()) {
    return;
  }
  std::string error;
  if (!AppendTrackerCommand(command_log_path, std::move(command), error)) {
    std::cerr << "[sekaiemu-libretro-spike] tracker command write failed: "
              << error << "\n";
  }
}

bool CycleTrackerDisplayMode(bool tracker_active,
                             TrackerRuntime& tracker_runtime,
                             TrackerWindowPresenter& tracker_window_presenter,
                             const std::function<void(const char*)>& save_tracker_state) {
  if (!tracker_active) {
    return false;
  }
  const auto current = tracker_runtime.UiState().display_mode;
  TrackerDisplayMode next = TrackerDisplayMode::SplitScreen;
  switch (current) {
    case TrackerDisplayMode::SplitScreen:
      next = TrackerDisplayMode::SeparateWindow;
      break;
    case TrackerDisplayMode::SeparateWindow:
    case TrackerDisplayMode::PipOverlay:
      next = TrackerDisplayMode::ToggleScreen;
      break;
    case TrackerDisplayMode::ToggleScreen:
      next = TrackerDisplayMode::SplitScreen;
      break;
  }
  tracker_runtime.SetDisplayMode(next);
  if (next != TrackerDisplayMode::SeparateWindow) {
    tracker_window_presenter.Shutdown();
  }
  save_tracker_state("display-mode");
  return true;
}

bool ToggleTrackerScreen(bool tracker_active,
                         TrackerRuntime& tracker_runtime,
                         const std::function<void(const char*)>& save_tracker_state) {
  if (!tracker_active) {
    return false;
  }
  tracker_runtime.TogglePrimaryScreen();
  save_tracker_state("toggle-screen");
  return true;
}

bool CycleTrackerTab(bool tracker_active,
                     TrackerRuntime& tracker_runtime,
                     const std::filesystem::path& command_log_path,
                     const std::function<void(const char*)>& save_tracker_state) {
  if (!tracker_active) {
    return false;
  }
  const auto resolved = tracker_runtime.ResolvedViewState();
  if (resolved.visible_tabs.empty()) {
    return false;
  }
  std::size_t current_index = 0;
  for (std::size_t index = 0; index < resolved.visible_tabs.size(); ++index) {
    if (resolved.visible_tabs[index].value("id", std::string()) == resolved.active_tab_id) {
      current_index = index;
      break;
    }
  }
  const auto next_index = (current_index + 1) % resolved.visible_tabs.size();
  const auto next_tab_id = resolved.visible_tabs[next_index].value("id", std::string());
  tracker_runtime.SetActiveTab(next_tab_id);
  EmitTrackerCommand(command_log_path, nlohmann::json{{"cmd", "tracker.set_tab"}, {"tab", next_tab_id}});
  save_tracker_state("tab");
  return true;
}

bool ToggleTrackerAutoFollow(bool tracker_active,
                             TrackerRuntime& tracker_runtime,
                             const std::filesystem::path& command_log_path,
                             const std::function<void(const char*)>& save_tracker_state) {
  if (!tracker_active) {
    return false;
  }
  const bool enabled = !tracker_runtime.LocalOverrideState().auto_follow_map;
  tracker_runtime.SetAutoMapFollow(enabled);
  EmitTrackerCommand(command_log_path, nlohmann::json{{"cmd", "tracker.set_auto_follow"}, {"enabled", enabled}});
  save_tracker_state("auto-map");
  return true;
}

void RenderTrackerPresentation(bool tracker_active,
                               TrackerRuntime& tracker_runtime,
                               TrackerWindowPresenter& tracker_window_presenter,
                               VideoBackend& video_backend,
                               const TrackerOverlayAssetResolver* asset_resolver,
                               const VideoGeometry& geometry,
                               std::uint64_t frame_counter,
                               std::uint64_t& last_render_frame,
                               std::uint64_t render_frame_interval,
                               bool force_render,
                               bool extra_overlay_visible,
                               const std::function<void(OverlayCanvas&, int, int)>& draw_extra_overlay) {
  if (!tracker_active) {
    video_backend.SetTrackerSidebarLayout(false, 0, geometry);
    tracker_window_presenter.Shutdown();
    if (extra_overlay_visible && draw_extra_overlay) {
      RenderExtraOverlayOnly(video_backend, geometry, draw_extra_overlay);
    } else {
      video_backend.ClearOverlay();
    }
    return;
  }

  const bool render_now = force_render ||
                          last_render_frame == 0 ||
                          frame_counter <= last_render_frame ||
                          frame_counter - last_render_frame >= render_frame_interval;
  if (!render_now) {
    return;
  }
  last_render_frame = frame_counter;

  const auto resolved = tracker_runtime.ResolvedViewState();
  switch (tracker_runtime.UiState().display_mode) {
    case TrackerDisplayMode::SeparateWindow:
      video_backend.SetTrackerSidebarLayout(false, 0, geometry);
      tracker_window_presenter.Render(tracker_runtime, resolved, asset_resolver);
      if (extra_overlay_visible && draw_extra_overlay) {
        RenderExtraOverlayOnly(video_backend, geometry, draw_extra_overlay);
      } else {
        video_backend.ClearOverlay();
      }
      break;
    case TrackerDisplayMode::PipOverlay:
    case TrackerDisplayMode::ToggleScreen:
      video_backend.SetTrackerSidebarLayout(false, 0, geometry);
      tracker_window_presenter.Shutdown();
      if (!resolved.show_tracker_screen) {
        if (extra_overlay_visible && draw_extra_overlay) {
          RenderExtraOverlayOnly(video_backend, geometry, draw_extra_overlay);
        } else {
          video_backend.ClearOverlay();
        }
        return;
      }
      RenderTrackerOverlayMode(tracker_runtime,
                               video_backend,
                               asset_resolver,
                               geometry,
                               resolved,
                               TrackerDisplayMode::ToggleScreen,
                               draw_extra_overlay);
      break;
    case TrackerDisplayMode::SplitScreen:
    default:
      tracker_window_presenter.Shutdown();
      RenderTrackerOverlayMode(tracker_runtime,
                               video_backend,
                               asset_resolver,
                               geometry,
                               resolved,
                               TrackerDisplayMode::SplitScreen,
                               draw_extra_overlay);
      break;
  }
}

}  // namespace sekaiemu::spike
