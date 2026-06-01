#include "tracker_runtime.hpp"

#include <limits>
#include <utility>

namespace sekaiemu::spike {

std::string_view ToString(TrackerDisplayMode mode) {
  switch (mode) {
    case TrackerDisplayMode::SplitScreen:
      return "split-screen";
    case TrackerDisplayMode::SeparateWindow:
      return "separate-window";
    case TrackerDisplayMode::PipOverlay:
      return "pip-overlay";
    case TrackerDisplayMode::ToggleScreen:
      return "toggle-screen";
  }
  return "split-screen";
}

TrackerDisplayMode TrackerDisplayModeFromString(std::string_view value) {
  if (value == "separate-window" || value == "separate-windows" || value == "separate") {
    return TrackerDisplayMode::SeparateWindow;
  }
  if (value == "pip-overlay" || value == "pip" || value == "overlay") {
    return TrackerDisplayMode::PipOverlay;
  }
  if (value == "toggle-screen" || value == "toggle") {
    return TrackerDisplayMode::ToggleScreen;
  }
  return TrackerDisplayMode::SplitScreen;
}

void TrackerRuntime::LoadBundle(TrackerBundle bundle) {
  bundle_ = std::move(bundle);
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::ApplySeedMetadata(const nlohmann::json& metadata) {
  authoritative_state_.seed_metadata = metadata;
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::ApplyLocalToggle(std::string toggle_id, bool value) {
  if (!toggle_id.empty()) {
    local_override_state_.toggles[std::move(toggle_id)] = value;
    BumpMutationSerial();
  }
}

void TrackerRuntime::SetActiveTab(std::string tab_id) {
  ui_state_.active_tab_id = std::move(tab_id);
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::SetManualMap(std::string map_id) {
  local_override_state_.manual_map_id = std::move(map_id);
  local_override_state_.auto_follow_map = false;
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::SetAutoMapFollow(bool enabled) {
  local_override_state_.auto_follow_map = enabled;
  if (enabled) {
    local_override_state_.manual_map_id.reset();
  }
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::SetDisplayMode(TrackerDisplayMode mode) {
  if (ui_state_.display_mode == mode) {
    return;
  }
  ui_state_.display_mode = mode;
  BumpMutationSerial();
}

void TrackerRuntime::SetPrimaryScreenVisible(bool visible) {
  if (ui_state_.show_tracker_screen == visible) {
    return;
  }
  ui_state_.show_tracker_screen = visible;
  BumpMutationSerial();
}

void TrackerRuntime::TogglePrimaryScreen() {
  SetPrimaryScreenVisible(!ui_state_.show_tracker_screen);
}

void TrackerRuntime::OpenMapContextMenu() {
  OpenMapContextMenuAt(ui_state_.map_context_menu_x, ui_state_.map_context_menu_y);
}

void TrackerRuntime::OpenMapContextMenuAt(int x, int y) {
  ui_state_.map_context_menu_visible = true;
  ui_state_.map_context_menu_x = x;
  ui_state_.map_context_menu_y = y;
  ui_state_.map_context_menu_selected_index = 0;
  ui_state_.map_context_menu_expanded_map_id.clear();
  BumpMutationSerial();
}

void TrackerRuntime::CloseMapContextMenu() {
  if (!ui_state_.map_context_menu_visible) {
    return;
  }
  ui_state_.map_context_menu_visible = false;
  ui_state_.map_context_menu_expanded_map_id.clear();
  BumpMutationSerial();
}

void TrackerRuntime::SetMapContextMenuSelectedIndex(std::size_t selected_index) {
  if (ui_state_.map_context_menu_selected_index == selected_index) {
    return;
  }
  ui_state_.map_context_menu_selected_index = selected_index;
  BumpMutationSerial();
}

void TrackerRuntime::SetMapContextMenuExpandedMapId(std::string map_id) {
  if (ui_state_.map_context_menu_expanded_map_id == map_id) {
    return;
  }
  ui_state_.map_context_menu_expanded_map_id = std::move(map_id);
  BumpMutationSerial();
}

void TrackerRuntime::Tick(double) { EnsureSelectionConsistency(); }

void TrackerRuntime::BumpMutationSerial() {
  if (mutation_serial_ == std::numeric_limits<std::uint64_t>::max()) {
    mutation_serial_ = 1;
    return;
  }
  ++mutation_serial_;
}

}  // namespace sekaiemu::spike
