#include "libretro_host_internal.hpp"

#include "libretro_bridge_host_helpers.hpp"
#include "libretro_menu_actions.hpp"
#include "libretro_save_host_helpers.hpp"
#include "libretro_state_slot_actions.hpp"
#include "libretro_tracker_host_state.hpp"
#include "libretro_tracker_presentation.hpp"
#include "libretro_video_host_helpers.hpp"
#include "tracker_map_context_menu.hpp"
#include "tracker_overlay_renderer.hpp"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sekaiemu::spike {
namespace {

struct TrackerInteractionGeometry {
  unsigned overlay_width = 0;
  unsigned overlay_height = 0;
  unsigned window_width = 0;
  unsigned window_height = 0;
  TrackerPanelLayout layout;
  bool compact = false;
};

struct OverlayPoint {
  int x = 0;
  int y = 0;
};

TrackerInteractionGeometry BuildTrackerInteractionGeometry(const TrackerRuntime& runtime,
                                                           const VideoGeometry& geometry,
                                                           unsigned separate_width,
                                                           unsigned separate_height) {
  const unsigned game_width = std::max(geometry.width, 256u);
  const unsigned game_height = std::max(geometry.height, 224u);
  TrackerInteractionGeometry result;
  result.overlay_width = std::max(game_width, 640u);
  result.overlay_height = std::max(game_height, 360u);
  result.window_width = game_width * 3u;
  result.window_height = game_height * 3u;

  switch (runtime.UiState().display_mode) {
    case TrackerDisplayMode::SeparateWindow:
      result.overlay_width = std::max(360u, separate_width == 0 ? 520u : separate_width);
      result.overlay_height = std::max(360u, separate_height == 0 ? 760u : separate_height);
      result.window_width = result.overlay_width;
      result.window_height = result.overlay_height;
      result.layout =
          TrackerPanelLayout{0, 0, static_cast<int>(result.overlay_width), static_cast<int>(result.overlay_height)};
      break;
    case TrackerDisplayMode::PipOverlay:
      result.compact = true;
      result.layout.width = static_cast<int>(result.overlay_width * 0.42);
      result.layout.height = static_cast<int>(result.overlay_height * 0.45);
      result.layout.x = static_cast<int>(result.overlay_width) - result.layout.width - 12;
      result.layout.y = static_cast<int>(result.overlay_height) - result.layout.height - 12;
      break;
    case TrackerDisplayMode::ToggleScreen:
      result.layout =
          TrackerPanelLayout{0, 0, static_cast<int>(result.overlay_width), static_cast<int>(result.overlay_height)};
      break;
    case TrackerDisplayMode::SplitScreen:
    default: {
      const unsigned tracker_sidebar_width = std::max(360u, game_height * 2u);
      result.overlay_width = game_width * 3u + tracker_sidebar_width;
      result.overlay_height = game_height * 3u;
      result.window_width = result.overlay_width;
      result.window_height = result.overlay_height;
      result.layout = TrackerPanelLayout{static_cast<int>(game_width * 3u),
                                         0,
                                         static_cast<int>(tracker_sidebar_width),
                                         static_cast<int>(result.overlay_height)};
      break;
    }
  }
  return result;
}

std::optional<OverlayPoint> MouseToOverlayPoint(const TrackerInteractionGeometry& geometry,
                                                int mouse_x,
                                                int mouse_y) {
  if (geometry.layout.width <= 0 || geometry.layout.height <= 0 ||
      geometry.window_width == 0 || geometry.window_height == 0) {
    return std::nullopt;
  }
  return OverlayPoint{
      static_cast<int>((static_cast<long long>(mouse_x) *
                        static_cast<long long>(geometry.overlay_width)) /
                       static_cast<long long>(geometry.window_width)),
      static_cast<int>((static_cast<long long>(mouse_y) *
                        static_cast<long long>(geometry.overlay_height)) /
                       static_cast<long long>(geometry.window_height)),
  };
}

bool PointInsideTrackerPanel(OverlayPoint point, const TrackerPanelLayout& layout) {
  return point.x >= layout.x && point.x < layout.x + layout.width &&
         point.y >= layout.y && point.y < layout.y + layout.height;
}

int TrackerMenuBodyY(const TrackerInteractionGeometry& geometry) {
  const int header_height = geometry.compact ? 24 : 44;
  return geometry.layout.y + header_height + 6;
}

std::string FindVisibleTabForMap(const TrackerResolvedViewState& resolved, std::string_view map_id) {
  if (map_id.empty()) {
    return {};
  }
  for (const auto& tab : resolved.visible_tabs) {
    if (!tab.is_object()) {
      continue;
    }
    if (tab.value("map_id", std::string{}) == map_id) {
      return tab.value("id", std::string{});
    }
  }
  return {};
}

std::optional<std::size_t> FindMatchingMenuEntry(
    const std::vector<TrackerMapContextMenuEntry>& entries,
    const TrackerMapContextMenuEntry& target) {
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto& entry = entries[index];
    if (entry.kind == target.kind && entry.map_id == target.map_id &&
        entry.tab_id == target.tab_id && entry.label == target.label) {
      return index;
    }
  }
  return std::nullopt;
}

}  // namespace

VideoGeometry LibretroHost::Impl::CurrentVideoGeometry() const {
  VideoGeometry geometry;
  geometry.width = av_info.geometry.base_width ? av_info.geometry.base_width : 640u;
  geometry.height = av_info.geometry.base_height ? av_info.geometry.base_height : 480u;
  geometry.pixel_format = pixel_format;
  return geometry;
}

bool LibretroHost::Impl::EnsureHardwareVideoBackend(const VideoGeometry& geometry,
                                                    std::string& error) {
  return EnsureHardwareVideoBackendForHost(hw_render_requested,
                                           hw_render_callback,
                                           hw_render_context_ready,
                                           video_backend,
                                           geometry,
                                           error);
}

bool LibretroHost::Impl::PrepareHardwareVideoBackend(const VideoGeometry& geometry,
                                                     std::string& error) {
  return PrepareHardwareVideoBackendForHost(hw_render_requested,
                                            hw_render_callback,
                                            hw_render_context_ready,
                                            video_backend,
                                            geometry,
                                            error);
}

void LibretroHost::Impl::MaybeUpdateVideoBackendGeometry() {
  sekaiemu::spike::MaybeUpdateVideoBackendGeometry(video_backend.get(), CurrentVideoGeometry());
}

void LibretroHost::Impl::SaveBatteryNow() {
  sekaiemu::spike::SaveBatteryNow(save_state_manager, core);
}

const TrackerOverlayAssetResolver* LibretroHost::Impl::TrackerAssetResolver() const {
  return tracker_asset_resolver_.HasRoots() ? &tracker_asset_resolver_ : nullptr;
}

void LibretroHost::Impl::SaveTrackerState(const char* reason) {
  SaveTrackerStateForHost(tracker_runtime_,
                          tracker_active_,
                          tracker_state_path_,
                          frame_counter,
                          tracker_last_save_frame_,
                          tracker_dirty_,
                          reason);
}

void LibretroHost::Impl::CycleTrackerDisplayMode() {
  const bool changed = sekaiemu::spike::CycleTrackerDisplayMode(
      tracker_active_,
      tracker_runtime_,
      tracker_window_presenter_,
      [this](const char* reason) { SaveTrackerState(reason); });
  tracker_dirty_ = changed || tracker_dirty_;
  if (changed) {
    SaveFrontendSettingsNow();
  }
}

void LibretroHost::Impl::ToggleTrackerScreen() {
  const bool changed = sekaiemu::spike::ToggleTrackerScreen(
      tracker_active_,
      tracker_runtime_,
      [this](const char* reason) { SaveTrackerState(reason); });
  tracker_dirty_ = changed || tracker_dirty_;
  if (changed) {
    SaveFrontendSettingsNow();
  }
}

void LibretroHost::Impl::CycleTrackerTab() {
  tracker_dirty_ = sekaiemu::spike::CycleTrackerTab(
                       tracker_active_,
                       tracker_runtime_,
                       tracker_command_log_path_,
                       [this](const char* reason) { SaveTrackerState(reason); }) ||
                   tracker_dirty_;
}

void LibretroHost::Impl::ToggleTrackerAutoFollow() {
  const bool changed = sekaiemu::spike::ToggleTrackerAutoFollow(
      tracker_active_,
      tracker_runtime_,
      tracker_command_log_path_,
      [this](const char* reason) { SaveTrackerState(reason); });
  tracker_dirty_ = changed || tracker_dirty_;
  if (changed) {
    SaveFrontendSettingsNow();
  }
}

void LibretroHost::Impl::OpenTrackerMapMenu() {
  if (!tracker_active_) {
    return;
  }
  tracker_runtime_.OpenMapContextMenu();
  tracker_dirty_ = true;
}

bool LibretroHost::Impl::OpenTrackerMapMenuAt(int mouse_x, int mouse_y) {
  if (!tracker_active_) {
    return false;
  }
  const auto geometry = BuildTrackerInteractionGeometry(tracker_runtime_,
                                                        CurrentVideoGeometry(),
                                                        tracker_window_presenter_.Width(),
                                                        tracker_window_presenter_.Height());
  const auto point = MouseToOverlayPoint(geometry, mouse_x, mouse_y);
  if (!point.has_value() || !PointInsideTrackerPanel(*point, geometry.layout)) {
    return false;
  }
  tracker_runtime_.OpenMapContextMenuAt(point->x, point->y);
  tracker_dirty_ = true;
  return true;
}

bool LibretroHost::Impl::ActivateTrackerMapMenu() {
  if (!tracker_active_ || !tracker_runtime_.UiState().map_context_menu_visible) {
    return false;
  }
  const auto resolved = tracker_runtime_.ResolvedViewState();
  const auto entries = BuildTrackerMapContextMenuEntries(tracker_runtime_, resolved);
  const auto selected = tracker_runtime_.UiState().map_context_menu_selected_index;
  if (selected >= entries.size()) {
    tracker_runtime_.CloseMapContextMenu();
    tracker_dirty_ = true;
    return true;
  }
  const auto& entry = entries[selected];
  if (entry.kind == TrackerMapContextMenuEntryKind::ActualMap) {
    tracker_runtime_.SetAutoMapFollow(true);
    EmitTrackerCommand(nlohmann::json{{"cmd", "tracker.set_auto_follow"}, {"enabled", true}});
    tracker_runtime_.CloseMapContextMenu();
    tracker_dirty_ = true;
    return true;
  }
  if (entry.expandable) {
    const auto& current_expanded = tracker_runtime_.UiState().map_context_menu_expanded_map_id;
    tracker_runtime_.SetMapContextMenuExpandedMapId(current_expanded == entry.map_id ? std::string{} : entry.map_id);
    if (const auto next_selected = FindMatchingMenuEntry(
            BuildTrackerMapContextMenuEntries(tracker_runtime_, tracker_runtime_.ResolvedViewState()),
            entry);
        next_selected.has_value()) {
      tracker_runtime_.SetMapContextMenuSelectedIndex(*next_selected);
    }
    tracker_dirty_ = true;
    return true;
  }
  if (!entry.map_id.empty()) {
    const auto tab_id = !entry.tab_id.empty() ? entry.tab_id : FindVisibleTabForMap(resolved, entry.map_id);
    if (!tab_id.empty()) {
      tracker_runtime_.SetActiveTab(tab_id);
    }
    tracker_runtime_.SetManualMap(entry.map_id);
    nlohmann::json command{{"cmd", "tracker.set_map"}, {"map", entry.map_id}};
    if (!tab_id.empty()) {
      command["tab"] = tab_id;
    }
    EmitTrackerCommand(std::move(command));
  }
  tracker_runtime_.CloseMapContextMenu();
  tracker_dirty_ = true;
  return true;
}

bool LibretroHost::Impl::ActivateTrackerMapMenuAt(int mouse_x, int mouse_y) {
  if (!tracker_active_ || !tracker_runtime_.UiState().map_context_menu_visible) {
    return false;
  }
  const auto resolved = tracker_runtime_.ResolvedViewState();
  const auto geometry = BuildTrackerInteractionGeometry(tracker_runtime_,
                                                        CurrentVideoGeometry(),
                                                        tracker_window_presenter_.Width(),
                                                        tracker_window_presenter_.Height());
  const auto point = MouseToOverlayPoint(geometry, mouse_x, mouse_y);
  if (!point.has_value()) {
    return false;
  }
  const auto hit = HitTestTrackerMapContextMenu(tracker_runtime_,
                                                resolved,
                                                geometry.layout,
                                                TrackerMenuBodyY(geometry),
                                                point->x,
                                                point->y);
  if (!hit.has_value()) {
    tracker_runtime_.CloseMapContextMenu();
    tracker_dirty_ = true;
    return true;
  }
  tracker_runtime_.SetMapContextMenuSelectedIndex(*hit);
  return ActivateTrackerMapMenu();
}

bool LibretroHost::Impl::HoverTrackerMapMenuAt(int mouse_x, int mouse_y) {
  if (!tracker_active_ || !tracker_runtime_.UiState().map_context_menu_visible) {
    return false;
  }
  const auto resolved = tracker_runtime_.ResolvedViewState();
  const auto geometry = BuildTrackerInteractionGeometry(tracker_runtime_,
                                                        CurrentVideoGeometry(),
                                                        tracker_window_presenter_.Width(),
                                                        tracker_window_presenter_.Height());
  const auto point = MouseToOverlayPoint(geometry, mouse_x, mouse_y);
  if (!point.has_value()) {
    return false;
  }
  const auto hit = HitTestTrackerMapContextMenu(tracker_runtime_,
                                                resolved,
                                                geometry.layout,
                                                TrackerMenuBodyY(geometry),
                                                point->x,
                                                point->y);
  if (!hit.has_value()) {
    return false;
  }
  const auto before = tracker_runtime_.MutationSerial();
  const auto entries = BuildTrackerMapContextMenuEntries(tracker_runtime_, resolved);
  if (*hit < entries.size()) {
    const auto& entry = entries[*hit];
    tracker_runtime_.SetMapContextMenuSelectedIndex(*hit);
    if (entry.expandable && !entry.expanded) {
      tracker_runtime_.SetMapContextMenuExpandedMapId(entry.map_id);
    } else if (entry.kind == TrackerMapContextMenuEntryKind::ActualMap ||
               (entry.kind == TrackerMapContextMenuEntryKind::Map && !entry.expandable && entry.depth == 0)) {
      tracker_runtime_.SetMapContextMenuExpandedMapId({});
    }
    if (const auto next_selected = FindMatchingMenuEntry(
            BuildTrackerMapContextMenuEntries(tracker_runtime_, tracker_runtime_.ResolvedViewState()),
            entry);
        next_selected.has_value()) {
      tracker_runtime_.SetMapContextMenuSelectedIndex(*next_selected);
    }
  }
  tracker_dirty_ = tracker_dirty_ || tracker_runtime_.MutationSerial() != before;
  return true;
}

void LibretroHost::Impl::StepTrackerMapMenu(int delta) {
  if (!tracker_active_ || !tracker_runtime_.UiState().map_context_menu_visible) {
    return;
  }
  const auto resolved = tracker_runtime_.ResolvedViewState();
  const std::size_t count = BuildTrackerMapContextMenuEntries(tracker_runtime_, resolved).size();
  if (count == 0) {
    return;
  }
  const auto current = tracker_runtime_.UiState().map_context_menu_selected_index % count;
  const auto next = static_cast<std::size_t>(
      (static_cast<int>(current) + delta + static_cast<int>(count)) % static_cast<int>(count));
  tracker_runtime_.SetMapContextMenuSelectedIndex(next);
  tracker_dirty_ = true;
}

void LibretroHost::Impl::CloseTrackerMapMenu() {
  tracker_runtime_.CloseMapContextMenu();
  tracker_dirty_ = true;
}

void LibretroHost::Impl::EmitTrackerCommand(nlohmann::json command) {
  sekaiemu::spike::EmitTrackerCommand(tracker_command_log_path_, std::move(command));
}

void LibretroHost::Impl::LoadBatteryNow() {
  sekaiemu::spike::LoadBatteryNow(save_state_manager, core);
}

void LibretroHost::Impl::SaveStateNow(int slot) {
  SaveStateSlotNow(save_state_manager,
                   core,
                   tracker_runtime_,
                   options.game_path.filename().string(),
                   frame_counter,
                   slot,
                   pending_state_screenshot_slot_,
                   pending_state_metadata_);
}

void LibretroHost::Impl::LoadStateNow(int slot) {
  LoadStateSlotNow(save_state_manager, core, slot);
}

void LibretroHost::Impl::TickBatteryPersistence() {
  sekaiemu::spike::TickBatteryPersistence(save_state_manager, core, options.probe_only, frame_counter);
}

void LibretroHost::Impl::SaveBatteryOnShutdown() {
  sekaiemu::spike::SaveBatteryOnShutdown(save_state_manager,
                                         core,
                                         game_loaded_for_shutdown,
                                         options.probe_only);
}

void LibretroHost::Impl::ApplyMenuAction(RuntimeMenuAction action) {
  switch (action) {
    case RuntimeMenuAction::DecreaseMasterVolume:
      AdjustMasterVolume(-5);
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    case RuntimeMenuAction::IncreaseMasterVolume:
      AdjustMasterVolume(5);
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    case RuntimeMenuAction::ToggleNotifications:
      ToggleNotifications();
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    case RuntimeMenuAction::CycleTrackerDisplayMode:
      CycleTrackerDisplayMode();
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    case RuntimeMenuAction::ToggleTrackerScreen:
      ToggleTrackerScreen();
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    case RuntimeMenuAction::ToggleTrackerAutoFollow:
      ToggleTrackerAutoFollow();
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    case RuntimeMenuAction::ToggleBridgeTerminal:
      ToggleBridgeTerminal();
      PersistRuntimeMenuSettingsIfNeeded();
      return;
    default:
      break;
  }
  ApplyRuntimeMenuAction(action,
                         runtime_menu,
                         core_option_manager,
                         running,
                         [this]() { core.retro_reset(); },
                         [this]() { SaveBatteryNow(); },
                         [this]() { LoadBatteryNow(); },
                         [this](int slot) { SaveStateNow(slot); },
                         [this](int slot) { LoadStateNow(slot); },
                         [this]() { RestartBridgeRuntime(); },
                         [this]() { ToggleChatOverlay(); });
  PersistRuntimeMenuSettingsIfNeeded();
}

void LibretroHost::Impl::RestartBridgeRuntime() {
  RestartSklmiCompanionForHost(options,
                               runtime_memory_server,
                               sklmi_companion_runtime,
                               active_sklmi_bridge_spec,
                               resolved_sklmi_runtime_binary,
                               resolved_sklmi_manifest_directory,
                               bridge_owner,
                               bridge_runtime_last_error,
                               tracker_snapshot_path_,
                               tracker_command_log_path_,
                               tracker_snapshot_reader_);
}

void LibretroHost::Impl::ToggleBridgeTerminal() {
  bridge_terminal_presenter_.SetEnabled(!bridge_terminal_presenter_.Enabled());
  bridge_terminal_last_mutation_serial_ = 0;
  bridge_terminal_last_render_frame_ = 0;
  SaveFrontendSettingsNow();
}

}  // namespace sekaiemu::spike
