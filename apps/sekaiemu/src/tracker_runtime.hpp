#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "tracker_raster_image.hpp"

namespace sekaiemu::spike {

enum class TrackerDisplayMode {
  SplitScreen,
  SeparateWindow,
  PipOverlay,
  ToggleScreen,
};

std::string_view ToString(TrackerDisplayMode mode);
TrackerDisplayMode TrackerDisplayModeFromString(std::string_view value);

struct TrackerMapDefinition {
  std::string id;
  std::string label;
  std::string image;
  bool image_transparent = false;
  std::vector<std::string> zones;
  std::filesystem::path resolved_image_path;
  std::optional<TrackerRasterImage> raster_image;
  std::string raster_load_error;
  nlohmann::json raw = nlohmann::json::object();
};

struct TrackerTabDefinition {
  std::string id;
  std::string label;
  std::string view_id;
  std::string map_id;
  std::vector<std::string> zones;
  nlohmann::json raw = nlohmann::json::object();
};

struct TrackerViewDefinition {
  std::string id;
  std::string label;
  std::vector<std::string> tab_ids;
  nlohmann::json raw = nlohmann::json::object();
};

struct TrackerInfoFieldDefinition {
  std::string id;
  std::string label;
  std::string source;
  std::string key;
  std::string fallback;
  bool hide_when_empty = false;
  nlohmann::json raw = nlohmann::json::object();
};

struct TrackerInfoPanelDefinition {
  std::string id;
  std::string label;
  std::string surface = "details";
  int priority = 100;
  std::vector<TrackerInfoFieldDefinition> fields;
  nlohmann::json raw = nlohmann::json::object();
};

struct TrackerBundle {
  std::string contract_version = "1";
  std::string linkedworld_id;
  std::string display_name;
  std::string default_view_id;
  std::string default_tab_id;
  std::string default_map_id;
  std::filesystem::path bundle_root;
  std::vector<TrackerMapDefinition> maps;
  std::vector<TrackerTabDefinition> tabs;
  std::vector<TrackerViewDefinition> views;
  std::vector<TrackerInfoPanelDefinition> info_panels;
  std::unordered_map<std::string, std::string> zone_map_bindings;
  nlohmann::json seed_option_bindings = nlohmann::json::array();
  nlohmann::json raw = nlohmann::json::object();

  static TrackerBundle LoadFromPath(const std::filesystem::path& bundle_path);
  static TrackerBundle LoadFromDirectory(const std::filesystem::path& bundle_root);

  const TrackerMapDefinition* FindMap(std::string_view id) const;
  const TrackerTabDefinition* FindTab(std::string_view id) const;
  const TrackerViewDefinition* FindView(std::string_view id) const;
};

struct TrackerAuthoritativeState {
  std::string world_instance_id;
  std::string linkedworld_id;
  std::string slot_id;
  std::string seed_id;
  std::string active_map_hint;
  std::string active_tab_hint;
  std::optional<bool> auto_follow_map_hint;
  std::unordered_set<std::int64_t> checked_locations;
  std::unordered_set<std::int64_t> missing_locations;
  std::unordered_set<std::int64_t> received_items;
  nlohmann::json snapshot = nlohmann::json::object();
  nlohmann::json seed_metadata = nlohmann::json::object();
};

struct TrackerRecentEvent {
  std::string event_type;
  std::string key;
  std::string label;
  std::string timestamp;
  std::optional<std::int64_t> canonical_id;
};

struct TrackerObservedState {
  std::string driver_instance_id;
  std::string linkedworld_id;
  std::string slot_id;
  std::string core_profile;
  std::string current_zone_id;
  std::unordered_set<std::int64_t> locally_checked_locations;
  std::unordered_set<std::int64_t> locally_received_items;
  std::vector<TrackerRecentEvent> recent_events;
  nlohmann::json runtime_context = nlohmann::json::object();
};

struct TrackerLocalOverrideState {
  std::unordered_map<std::string, bool> toggles;
  std::optional<std::string> manual_map_id;
  bool auto_follow_map = true;
};

struct TrackerUiState {
  TrackerDisplayMode display_mode = TrackerDisplayMode::SplitScreen;
  std::string active_view_id;
  std::string active_tab_id;
  bool show_tracker_screen = true;
  bool map_context_menu_visible = false;
  int map_context_menu_x = 0;
  int map_context_menu_y = 0;
  std::size_t map_context_menu_selected_index = 0;
  std::string map_context_menu_expanded_map_id;
  double zoom = 1.0;
  double pan_x = 0.0;
  double pan_y = 0.0;
};

struct TrackerPersistedState {
  std::string format_version = "1";
  std::string linkedworld_id;
  std::string slot_id;
  std::string seed_id;
  std::string world_instance_id;
  TrackerLocalOverrideState local_overrides;
  TrackerUiState ui_state;
  nlohmann::json cached_seed_metadata = nlohmann::json::object();
  nlohmann::json cached_server_snapshot = nlohmann::json::object();
  nlohmann::json cached_observed_state = nlohmann::json::object();
};

struct TrackerResolvedViewState {
  std::string active_view_id;
  std::string active_tab_id;
  std::string active_map_id;
  std::string current_zone_id;
  bool auto_follow_map = true;
  bool show_tracker_screen = true;
  nlohmann::json toggles = nlohmann::json::object();
  std::vector<nlohmann::json> visible_tabs;
  std::vector<nlohmann::json> visible_maps;
  std::vector<nlohmann::json> info_panels;
  std::vector<nlohmann::json> recent_events;
  nlohmann::json seed_metadata = nlohmann::json::object();
};

class TrackerRuntime {
 public:
  void LoadBundle(TrackerBundle bundle);

  void ApplySeedMetadata(const nlohmann::json& metadata);
  void ApplyServerSnapshot(const nlohmann::json& snapshot);
  void ApplySklmiEvent(const nlohmann::json& event);
  void ApplyRuntimeContext(const nlohmann::json& context);
  void ApplyLocalToggle(std::string toggle_id, bool value);
  void SetActiveTab(std::string tab_id);
  void SetManualMap(std::string map_id);
  void SetAutoMapFollow(bool enabled);
  void SetDisplayMode(TrackerDisplayMode mode);
  void SetPrimaryScreenVisible(bool visible);
  void TogglePrimaryScreen();
  void OpenMapContextMenu();
  void OpenMapContextMenuAt(int x, int y);
  void CloseMapContextMenu();
  void SetMapContextMenuSelectedIndex(std::size_t selected_index);
  void SetMapContextMenuExpandedMapId(std::string map_id);
  void Tick(double dt);

  const TrackerBundle* Bundle() const { return bundle_ ? &*bundle_ : nullptr; }
  const TrackerAuthoritativeState& AuthoritativeState() const { return authoritative_state_; }
  const TrackerObservedState& ObservedState() const { return observed_state_; }
  const TrackerLocalOverrideState& LocalOverrideState() const { return local_override_state_; }
  const TrackerUiState& UiState() const { return ui_state_; }
  std::uint64_t MutationSerial() const { return mutation_serial_; }
  TrackerResolvedViewState ResolvedViewState() const;

  TrackerPersistedState BuildPersistedState() const;
  void LoadPersistedState(const TrackerPersistedState& state);
  void SavePersistedState(const std::filesystem::path& path) const;
  static TrackerPersistedState ReadPersistedState(const std::filesystem::path& path);

 private:
  void BumpMutationSerial();
  void EnsureSelectionConsistency();
  bool SeedBindingMatches(const nlohmann::json& binding) const;
  bool TabVisible(const TrackerTabDefinition& tab) const;
  bool MapVisible(const TrackerMapDefinition& map) const;
  bool ResolvedAutoFollowMap() const;
  std::string ResolveZoneMapId() const;
  std::string ResolveHintedMapId() const;
  std::string ResolvedMapId() const;
  std::string FindVisibleTabForMap(std::string_view map_id) const;

  std::optional<TrackerBundle> bundle_;
  TrackerAuthoritativeState authoritative_state_;
  TrackerObservedState observed_state_;
  TrackerLocalOverrideState local_override_state_;
  TrackerUiState ui_state_;
  std::uint64_t mutation_serial_ = 1;
};

}  // namespace sekaiemu::spike
