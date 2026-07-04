#pragma once

#include "overlay_canvas.hpp"
#include "tracker_runtime.hpp"
#include "video_backend.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {

class TrackerOverlayAssetResolver;
class TrackerWindowPresenter;

void EmitTrackerCommand(const std::filesystem::path& command_log_path,
                        nlohmann::json command);

bool CycleTrackerDisplayMode(bool tracker_active,
                             TrackerRuntime& tracker_runtime,
                             TrackerWindowPresenter& tracker_window_presenter,
                             const std::function<void(const char*)>& save_tracker_state);

bool ToggleTrackerScreen(bool tracker_active,
                         TrackerRuntime& tracker_runtime,
                         const std::function<void(const char*)>& save_tracker_state);

bool CycleTrackerTab(bool tracker_active,
                     TrackerRuntime& tracker_runtime,
                     const std::filesystem::path& command_log_path,
                     const std::function<void(const char*)>& save_tracker_state);

bool ToggleTrackerAutoFollow(bool tracker_active,
                             TrackerRuntime& tracker_runtime,
                             const std::filesystem::path& command_log_path,
                             const std::function<void(const char*)>& save_tracker_state);

void RenderTrackerPresentation(bool tracker_active,
                               TrackerRuntime& tracker_runtime,
                               TrackerWindowPresenter& tracker_window_presenter,
                               VideoBackend& video_backend,
                               const TrackerOverlayAssetResolver* asset_resolver,
                               const VideoGeometry& geometry,
                               std::uint64_t frame_counter,
                               std::uint64_t& last_render_frame,
                               std::uint64_t render_frame_interval,
                               bool force_render = false,
                               bool extra_overlay_visible = false,
                               const std::function<void(OverlayCanvas&, int, int)>& draw_extra_overlay = {});

}  // namespace sekaiemu::spike
