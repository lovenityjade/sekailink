#include "tracker_runtime.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sekaiemu::spike {
namespace {

std::string TrimCopy(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_file_open_failed:" + path.string());
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::string JsonString(const nlohmann::json& value,
                       std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto it = value.find(key);
    if (it != value.end() && it->is_string()) {
      const auto parsed = TrimCopy(it->get<std::string>());
      if (!parsed.empty()) {
        return parsed;
      }
    }
  }
  return {};
}

void AtomicWriteJson(const std::filesystem::path& path, const nlohmann::json& value) {
  std::filesystem::create_directories(path.parent_path());
  const auto temp_path = path.string() + ".tmp";
  {
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      throw std::runtime_error("tracker_state_open_failed:" + temp_path);
    }
    output << value.dump(2) << '\n';
    if (!output.good()) {
      throw std::runtime_error("tracker_state_write_failed:" + temp_path);
    }
  }
  std::error_code ec;
  std::filesystem::rename(temp_path, path, ec);
  if (ec) {
    std::filesystem::copy_file(temp_path, path, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(temp_path, ec);
  }
  if (ec) {
    throw std::runtime_error("tracker_state_commit_failed:" + path.string());
  }
}

nlohmann::json PersistedStateToJson(const TrackerPersistedState& state) {
  nlohmann::json toggles = nlohmann::json::object();
  for (const auto& [key, value] : state.local_overrides.toggles) {
    toggles[key] = value;
  }
  return {
      {"format_version", state.format_version},
      {"linkedworld_id", state.linkedworld_id},
      {"slot_id", state.slot_id},
      {"seed_id", state.seed_id},
      {"world_instance_id", state.world_instance_id},
      {"local_overrides",
       {{"toggles", toggles},
        {"manual_map_id", state.local_overrides.manual_map_id.value_or("")},
        {"auto_follow_map", state.local_overrides.auto_follow_map}}},
      {"ui_state",
       {{"display_mode", std::string(ToString(state.ui_state.display_mode))},
        {"active_view_id", state.ui_state.active_view_id},
        {"active_tab_id", state.ui_state.active_tab_id},
        {"show_tracker_screen", state.ui_state.show_tracker_screen},
        {"zoom", state.ui_state.zoom},
        {"pan_x", state.ui_state.pan_x},
        {"pan_y", state.ui_state.pan_y}}},
      {"cached_seed_metadata", state.cached_seed_metadata},
      {"cached_server_snapshot", state.cached_server_snapshot},
      {"cached_observed_state", state.cached_observed_state},
  };
}

TrackerPersistedState PersistedStateFromJson(const nlohmann::json& raw) {
  TrackerPersistedState state;
  state.format_version = JsonString(raw, {"format_version", "formatVersion"});
  if (state.format_version.empty()) {
    state.format_version = "1";
  }
  state.linkedworld_id = JsonString(raw, {"linkedworld_id", "linkedworldId"});
  state.slot_id = JsonString(raw, {"slot_id", "slotId"});
  state.seed_id = JsonString(raw, {"seed_id", "seedId"});
  state.world_instance_id = JsonString(raw, {"world_instance_id", "worldInstanceId"});
  const auto overrides = raw.value("local_overrides", nlohmann::json::object());
  if (const auto toggles = overrides.find("toggles"); toggles != overrides.end() && toggles->is_object()) {
    for (auto it = toggles->begin(); it != toggles->end(); ++it) {
      if (it.value().is_boolean()) {
        state.local_overrides.toggles[it.key()] = it.value().get<bool>();
      }
    }
  }
  const auto manual_map = JsonString(overrides, {"manual_map_id", "manualMapId"});
  if (!manual_map.empty()) {
    state.local_overrides.manual_map_id = manual_map;
  }
  state.local_overrides.auto_follow_map =
      overrides.value("auto_follow_map", overrides.value("autoFollowMap", true));

  const auto ui = raw.value("ui_state", nlohmann::json::object());
  state.ui_state.display_mode =
      TrackerDisplayModeFromString(JsonString(ui, {"display_mode", "displayMode"}));
  state.ui_state.active_view_id = JsonString(ui, {"active_view_id", "activeViewId"});
  state.ui_state.active_tab_id = JsonString(ui, {"active_tab_id", "activeTabId"});
  state.ui_state.show_tracker_screen =
      ui.value("show_tracker_screen", ui.value("showTrackerScreen", true));
  state.ui_state.zoom = ui.value("zoom", 1.0);
  state.ui_state.pan_x = ui.value("pan_x", ui.value("panX", 0.0));
  state.ui_state.pan_y = ui.value("pan_y", ui.value("panY", 0.0));
  state.cached_seed_metadata = raw.value("cached_seed_metadata", nlohmann::json::object());
  state.cached_server_snapshot = raw.value("cached_server_snapshot", nlohmann::json::object());
  state.cached_observed_state = raw.value("cached_observed_state", nlohmann::json::object());
  return state;
}

}  // namespace

TrackerPersistedState TrackerRuntime::BuildPersistedState() const {
  TrackerPersistedState state;
  state.linkedworld_id = bundle_ ? bundle_->linkedworld_id : authoritative_state_.linkedworld_id;
  state.slot_id = authoritative_state_.slot_id.empty() ? observed_state_.slot_id
                                                       : authoritative_state_.slot_id;
  state.seed_id = authoritative_state_.seed_id;
  state.world_instance_id = authoritative_state_.world_instance_id;
  state.local_overrides = local_override_state_;
  state.ui_state = ui_state_;
  state.ui_state.map_context_menu_visible = false;
  state.ui_state.pin_context_menu_visible = false;
  state.ui_state.pin_context_menu_pin_id.clear();
  state.ui_state.hover_tooltip_visible = false;
  state.ui_state.hover_tooltip_text.clear();
  state.cached_seed_metadata = authoritative_state_.seed_metadata;
  state.cached_server_snapshot = authoritative_state_.snapshot;
  state.cached_observed_state = observed_state_.runtime_context;
  state.cached_observed_state["current_zone_id"] = observed_state_.current_zone_id;
  state.cached_observed_state["driver_instance_id"] = observed_state_.driver_instance_id;
  state.cached_observed_state["linkedworld_id"] = observed_state_.linkedworld_id;
  state.cached_observed_state["slot_id"] = observed_state_.slot_id;
  state.cached_observed_state["core_profile"] = observed_state_.core_profile;
  return state;
}

void TrackerRuntime::LoadPersistedState(const TrackerPersistedState& state) {
  if (bundle_ && !state.linkedworld_id.empty() && state.linkedworld_id != bundle_->linkedworld_id) {
    throw std::runtime_error("tracker_state_linkedworld_mismatch");
  }
  if (!authoritative_state_.seed_id.empty() && !state.seed_id.empty() &&
      state.seed_id != authoritative_state_.seed_id) {
    throw std::runtime_error("tracker_state_seed_mismatch");
  }
  if (authoritative_state_.seed_id.empty() && !state.seed_id.empty()) {
    authoritative_state_.seed_id = state.seed_id;
  }
  if (authoritative_state_.slot_id.empty() && !state.slot_id.empty()) {
    authoritative_state_.slot_id = state.slot_id;
  }
  if (authoritative_state_.world_instance_id.empty() && !state.world_instance_id.empty()) {
    authoritative_state_.world_instance_id = state.world_instance_id;
  }
  if (authoritative_state_.seed_metadata.empty()) {
    authoritative_state_.seed_metadata = state.cached_seed_metadata;
  }
  if (authoritative_state_.snapshot.empty() && state.cached_server_snapshot.is_object() &&
      !state.cached_server_snapshot.empty()) {
    ApplyServerSnapshot(state.cached_server_snapshot);
  }
  local_override_state_ = state.local_overrides;
  ui_state_ = state.ui_state;
  observed_state_.runtime_context = state.cached_observed_state;
  if (state.cached_observed_state.is_object()) {
    observed_state_.current_zone_id =
        JsonString(state.cached_observed_state, {"current_zone_id", "currentZoneId"});
    observed_state_.driver_instance_id =
        JsonString(state.cached_observed_state, {"driver_instance_id", "driverInstanceId"});
    observed_state_.linkedworld_id =
        JsonString(state.cached_observed_state, {"linkedworld_id", "linkedworldId"});
    observed_state_.slot_id = JsonString(state.cached_observed_state, {"slot_id", "slotId"});
    observed_state_.core_profile =
        JsonString(state.cached_observed_state, {"core_profile", "coreProfile"});
  }
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::SavePersistedState(const std::filesystem::path& path) const {
  AtomicWriteJson(path, PersistedStateToJson(BuildPersistedState()));
}

TrackerPersistedState TrackerRuntime::ReadPersistedState(const std::filesystem::path& path) {
  return PersistedStateFromJson(nlohmann::json::parse(ReadTextFile(path)));
}

}  // namespace sekaiemu::spike
