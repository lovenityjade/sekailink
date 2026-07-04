#include "tracker_runtime.hpp"
#include "tracker_runtime_snapshot_values.hpp"

#include <algorithm>

namespace sekaiemu::spike {

using tracker_runtime_values::JsonScalarToString;
using tracker_runtime_values::JsonString;
using tracker_runtime_values::JsonValueAtPath;
using tracker_runtime_values::SnapshotArraySize;
using tracker_runtime_values::SnapshotEntryLabel;
using tracker_runtime_values::SnapshotEntryString;
using tracker_runtime_values::SnapshotLastArrayEntry;

namespace {

bool BindingAppliesToId(const nlohmann::json& binding,
                        const char* enable_key,
                        const char* disable_key,
                        std::string_view id) {
  if (id.empty()) {
    return false;
  }
  if (binding.contains(disable_key) && binding[disable_key].is_array()) {
    for (const auto& entry : binding[disable_key]) {
      if (entry.is_string() && entry.get<std::string>() == id) {
        return false;
      }
    }
  }
  if (binding.contains(enable_key) && binding[enable_key].is_array()) {
    bool matched = false;
    for (const auto& entry : binding[enable_key]) {
      if (entry.is_string() && entry.get<std::string>() == id) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      return false;
    }
  }
  return true;
}

}  // namespace

TrackerResolvedViewState TrackerRuntime::ResolvedViewState() const {
  TrackerResolvedViewState resolved;
  resolved.active_view_id = ui_state_.active_view_id;
  resolved.active_tab_id =
      !ui_state_.active_tab_id.empty() ? ui_state_.active_tab_id : authoritative_state_.active_tab_hint;
  resolved.active_map_id = ResolvedMapId();
  resolved.current_zone_id = observed_state_.current_zone_id;
  resolved.auto_follow_map = ResolvedAutoFollowMap();
  resolved.show_tracker_screen = ui_state_.show_tracker_screen;
  resolved.seed_metadata = authoritative_state_.seed_metadata;
  for (const auto& [key, value] : local_override_state_.toggles) {
    resolved.toggles[key] = value;
  }
  if (!bundle_) {
    return resolved;
  }
  for (const auto& tab : bundle_->tabs) {
    if (!TabVisible(tab)) {
      continue;
    }
    resolved.visible_tabs.push_back({{"id", tab.id},
                                     {"label", tab.label},
                                     {"view_id", tab.view_id},
                                     {"map_id", tab.map_id},
                                     {"is_active", tab.id == resolved.active_tab_id}});
  }
  for (const auto& map : bundle_->maps) {
    if (!MapVisible(map)) {
      continue;
    }
    auto visible_map = nlohmann::json{{"id", map.id},
                                      {"label", map.label},
                                      {"image", map.image},
                                      {"image_transparent", map.image_transparent},
                                      {"image_loaded", map.raster_image.has_value()},
                                      {"image_error", map.raster_load_error},
                                      {"is_active", map.id == resolved.active_map_id}};
    const int menu_depth = map.raw.value("menu_depth", map.raw.value("menuDepth", 0));
    if (menu_depth > 0) {
      visible_map["menu_depth"] = menu_depth;
    }
    const auto menu_parent = JsonString(map.raw, {"menu_parent", "menuParent"});
    if (!menu_parent.empty()) {
      visible_map["menu_parent"] = menu_parent;
    }
    for (const char* key : {"menu_children", "menuChildren", "children"}) {
      const auto found = map.raw.find(key);
      if (found != map.raw.end() && found->is_array()) {
        visible_map["menu_children"] = *found;
        break;
      }
    }
    resolved.visible_maps.push_back(std::move(visible_map));
  }

  auto resolve_session_value = [&](std::string_view key) -> std::string {
    const std::size_t checked_count = std::max<std::size_t>(
        SnapshotArraySize(authoritative_state_.snapshot,
                          {"checked_locations", "checkedLocations", "locations.checked", "location_state.checked"}),
        authoritative_state_.checked_locations.size());
    const std::size_t missing_count = std::max<std::size_t>(
        SnapshotArraySize(authoritative_state_.snapshot,
                          {"missing_locations", "missingLocations", "locations.missing", "location_state.missing"}),
        authoritative_state_.missing_locations.size());
    const std::size_t received_count = std::max<std::size_t>(
        SnapshotArraySize(authoritative_state_.snapshot,
                          {"received_items", "receivedItems", "items.received", "items.received_items", "item_state.received"}),
        authoritative_state_.received_items.size());
    const std::size_t total_known = std::max<std::size_t>(checked_count + missing_count, checked_count);
    if (key == "linkedworld_id") {
      return !authoritative_state_.linkedworld_id.empty() ? authoritative_state_.linkedworld_id
                                                          : observed_state_.linkedworld_id;
    }
    if (key == "slot_id") {
      return !authoritative_state_.slot_id.empty() ? authoritative_state_.slot_id : observed_state_.slot_id;
    }
    if (key == "seed_id") {
      return authoritative_state_.seed_id;
    }
    if (key == "world_instance_id") {
      return authoritative_state_.world_instance_id;
    }
    if (key == "current_zone_id") {
      return observed_state_.current_zone_id;
    }
    if (key == "active_tab_id") {
      return resolved.active_tab_id;
    }
    if (key == "active_map_id") {
      return resolved.active_map_id;
    }
    if (key == "display_mode") {
      return std::string(ToString(ui_state_.display_mode));
    }
    if (key == "auto_follow_map") {
      return ResolvedAutoFollowMap() ? "ON" : "OFF";
    }
    if (key == "checked_count") {
      return std::to_string(checked_count);
    }
    if (key == "missing_count") {
      return std::to_string(missing_count);
    }
    if (key == "received_count") {
      return std::to_string(received_count);
    }
    if (key == "local_checked_count") {
      return std::to_string(observed_state_.locally_checked_locations.size());
    }
    if (key == "local_received_count") {
      return std::to_string(observed_state_.locally_received_items.size());
    }
    if (key == "known_total_count") {
      return std::to_string(total_known);
    }
    if (key == "check_progress") {
      return total_known == 0 ? std::to_string(checked_count)
                              : std::to_string(checked_count) + "/" + std::to_string(total_known);
    }
    if (key == "completion_percent") {
      if (total_known == 0) {
        return "0%";
      }
      return std::to_string((checked_count * 100) / total_known) + "%";
    }
    if (key == "recent_event_count") {
      return std::to_string(observed_state_.recent_events.size());
    }
    if (key == "presentation_mode") {
      return "side-by-side";
    }
    if (key == "last_received_label") {
      for (auto it = observed_state_.recent_events.rbegin(); it != observed_state_.recent_events.rend(); ++it) {
        if (it->event_type == "item_received" && !it->label.empty()) {
          return it->label;
        }
      }
      return SnapshotEntryLabel(authoritative_state_.snapshot,
                                {"received_items", "receivedItems", "items.received", "items.received_items", "item_state.received"},
                                {"item_name", "itemName", "mapped_value", "mappedValue", "label", "value"},
                                {"item_id", "itemId", "canonical_id", "canonicalId", "id"},
                                {"item_names"});
    }
    if (key == "last_received_from") {
      if (const auto* entry = SnapshotLastArrayEntry(authoritative_state_.snapshot,
                                                     {"received_items", "receivedItems", "items.received", "items.received_items", "item_state.received"});
          entry != nullptr && entry->is_object()) {
        return JsonString(*entry, {"sender_alias", "senderAlias", "from_alias", "fromAlias", "sender_slot", "senderSlot"});
      }
      return {};
    }
    if (key == "last_check_label") {
      for (auto it = observed_state_.recent_events.rbegin(); it != observed_state_.recent_events.rend(); ++it) {
        if (it->event_type == "location_checked" && !it->label.empty()) {
          return it->label;
        }
      }
      return SnapshotEntryLabel(authoritative_state_.snapshot,
                                {"checked_locations", "checkedLocations", "locations.checked", "location_state.checked"},
                                {"location_name", "locationName", "mapped_value", "mappedValue", "label", "value"},
                                {"location_id", "locationId", "canonical_id", "canonicalId", "id"},
                                {"location_names", "location_mapping"});
    }
    if (key == "last_received_timestamp") {
      for (auto it = observed_state_.recent_events.rbegin(); it != observed_state_.recent_events.rend(); ++it) {
        if (it->event_type == "item_received" && !it->timestamp.empty()) {
          return it->timestamp;
        }
      }
      return SnapshotEntryString(authoritative_state_.snapshot,
                                 {"received_items", "receivedItems", "items.received", "items.received_items", "item_state.received"},
                                 {"timestamp", "received_at", "receivedAt", "ts"});
    }
    if (key == "last_check_timestamp") {
      for (auto it = observed_state_.recent_events.rbegin(); it != observed_state_.recent_events.rend(); ++it) {
        if (it->event_type == "location_checked" && !it->timestamp.empty()) {
          return it->timestamp;
        }
      }
      return SnapshotEntryString(authoritative_state_.snapshot,
                                 {"checked_locations", "checkedLocations", "locations.checked", "location_state.checked"},
                                 {"timestamp", "checked_at", "checkedAt", "ts"});
    }
    return {};
  };

  auto resolve_field_value = [&](const TrackerInfoFieldDefinition& field) -> std::string {
    const auto source = field.source;
    if (source == "session") {
      const auto value = resolve_session_value(field.key);
      return !value.empty() ? value : field.fallback;
    }
    const nlohmann::json* root = nullptr;
    if (source == "seed" || source == "seed_metadata") {
      root = &authoritative_state_.seed_metadata;
    } else if (source == "runtime" || source == "runtime_context") {
      root = &observed_state_.runtime_context;
    } else if (source == "snapshot" || source == "server" || source == "server_snapshot") {
      root = &authoritative_state_.snapshot;
    }
    if (root != nullptr) {
      if (const auto* value = JsonValueAtPath(*root, field.key)) {
        const auto rendered = JsonScalarToString(*value);
        if (!rendered.empty()) {
          return rendered;
        }
      }
    }
    return field.fallback;
  };

  for (const auto& panel : bundle_->info_panels) {
    nlohmann::json fields = nlohmann::json::array();
    for (const auto& field : panel.fields) {
      const auto value = resolve_field_value(field);
      if (value.empty() && field.hide_when_empty) {
        continue;
      }
      fields.push_back({
          {"id", field.id},
          {"label", field.label},
          {"value", value.empty() ? std::string("UNKNOWN") : value},
          {"source", field.source},
          {"key", field.key},
      });
    }
    if (fields.empty()) {
      continue;
    }
    resolved.info_panels.push_back({
        {"id", panel.id},
        {"label", panel.label},
        {"surface", panel.surface},
        {"priority", panel.priority},
        {"fields", fields},
    });
  }

  for (const auto& event : observed_state_.recent_events) {
    resolved.recent_events.push_back({
        {"event_type", event.event_type},
        {"key", event.key},
        {"label", event.label},
        {"timestamp", event.timestamp},
        {"canonical_id", event.canonical_id.has_value() ? nlohmann::json(*event.canonical_id) : nlohmann::json()},
    });
  }
  return resolved;
}

void TrackerRuntime::EnsureSelectionConsistency() {
  if (!bundle_) {
    return;
  }
  if (ui_state_.active_view_id.empty() || bundle_->FindView(ui_state_.active_view_id) == nullptr) {
    ui_state_.active_view_id = bundle_->default_view_id;
  }
  if (!authoritative_state_.active_tab_hint.empty()) {
    const auto* hinted_tab = bundle_->FindTab(authoritative_state_.active_tab_hint);
    if (hinted_tab != nullptr) {
      if (!hinted_tab->view_id.empty()) {
        ui_state_.active_view_id = hinted_tab->view_id;
      }
      if (TabVisible(*hinted_tab)) {
        ui_state_.active_tab_id = hinted_tab->id;
      }
    }
  }
  if (ui_state_.active_tab_id.empty() || bundle_->FindTab(ui_state_.active_tab_id) == nullptr ||
      !TabVisible(*bundle_->FindTab(ui_state_.active_tab_id))) {
    for (const auto& tab : bundle_->tabs) {
      if (TabVisible(tab) && (tab.view_id.empty() || tab.view_id == ui_state_.active_view_id)) {
        ui_state_.active_tab_id = tab.id;
        break;
      }
    }
    if (ui_state_.active_tab_id.empty()) {
      ui_state_.active_tab_id = bundle_->default_tab_id;
    }
  }
  if (local_override_state_.manual_map_id &&
      bundle_->FindMap(*local_override_state_.manual_map_id) == nullptr) {
    local_override_state_.manual_map_id.reset();
  }
  if (ResolvedAutoFollowMap() && authoritative_state_.active_tab_hint.empty()) {
    auto followed_map = ResolveHintedMapId();
    if (followed_map.empty()) {
      followed_map = ResolveZoneMapId();
    }
    if (!followed_map.empty()) {
      ui_state_.previous_map_id = followed_map;
    }
    if (const auto tab_id = FindVisibleTabForMap(followed_map); !tab_id.empty()) {
      ui_state_.active_tab_id = tab_id;
      if (const auto* tab = bundle_->FindTab(tab_id); tab != nullptr && !tab->view_id.empty()) {
        ui_state_.active_view_id = tab->view_id;
      }
    }
  }
}

bool TrackerRuntime::SeedBindingMatches(const nlohmann::json& binding) const {
  const auto option = JsonString(binding, {"option", "key"});
  if (option.empty()) {
    return true;
  }
  if (!authoritative_state_.seed_metadata.contains(option)) {
    return false;
  }
  const auto expected = binding.find("equals");
  if (expected == binding.end()) {
    return true;
  }
  return authoritative_state_.seed_metadata.at(option) == *expected;
}

bool TrackerRuntime::TabVisible(const TrackerTabDefinition& tab) const {
  if (!bundle_) {
    return false;
  }
  if (!tab.view_id.empty() && !ui_state_.active_view_id.empty() &&
      tab.view_id != ui_state_.active_view_id) {
    return false;
  }
  if (!tab.zones.empty() && !observed_state_.current_zone_id.empty() &&
      std::find(tab.zones.begin(), tab.zones.end(), observed_state_.current_zone_id) ==
          tab.zones.end()) {
    return false;
  }
  for (const auto& binding : bundle_->seed_option_bindings) {
    if (!binding.is_object() || !SeedBindingMatches(binding)) {
      continue;
    }
    if (!BindingAppliesToId(binding, "enable_tabs", "disable_tabs", tab.id)) {
      return false;
    }
  }
  return true;
}

bool TrackerRuntime::MapVisible(const TrackerMapDefinition& map) const {
  if (!bundle_) {
    return false;
  }
  for (const auto& binding : bundle_->seed_option_bindings) {
    if (!binding.is_object() || !SeedBindingMatches(binding)) {
      continue;
    }
    if (!BindingAppliesToId(binding, "enable_maps", "disable_maps", map.id)) {
      return false;
    }
  }
  return true;
}

bool TrackerRuntime::ResolvedAutoFollowMap() const {
  return local_override_state_.auto_follow_map;
}

std::string TrackerRuntime::ResolveZoneMapId() const {
  if (!bundle_ || observed_state_.current_zone_id.empty()) {
    return {};
  }
  const auto it = bundle_->zone_map_bindings.find(observed_state_.current_zone_id);
  if (it == bundle_->zone_map_bindings.end() || bundle_->FindMap(it->second) == nullptr) {
    return {};
  }
  return it->second;
}

std::string TrackerRuntime::ResolveHintedMapId() const {
  if (!bundle_ || authoritative_state_.active_map_hint.empty()) {
    return {};
  }
  const auto* map = bundle_->FindMap(authoritative_state_.active_map_hint);
  if (map == nullptr || !MapVisible(*map)) {
    return {};
  }
  return map->id;
}

std::string TrackerRuntime::ResolvedMapId() const {
  if (!bundle_) {
    if (!ResolvedAutoFollowMap() && local_override_state_.manual_map_id) {
      return *local_override_state_.manual_map_id;
    }
    return authoritative_state_.active_map_hint;
  }
  if (!ResolvedAutoFollowMap() && local_override_state_.manual_map_id &&
      bundle_->FindMap(*local_override_state_.manual_map_id) != nullptr) {
    return *local_override_state_.manual_map_id;
  }
  if (ResolvedAutoFollowMap()) {
    if (const auto hinted_map = ResolveHintedMapId(); !hinted_map.empty()) {
      return hinted_map;
    }
  }
  if (const auto zone_map = ResolveZoneMapId(); !zone_map.empty()) {
    return zone_map;
  }
  if (ResolvedAutoFollowMap() && !ui_state_.previous_map_id.empty()) {
    if (const auto* previous_map = bundle_->FindMap(ui_state_.previous_map_id);
        previous_map != nullptr && MapVisible(*previous_map)) {
      return previous_map->id;
    }
  }
  if (const auto* active_tab = bundle_->FindTab(ui_state_.active_tab_id);
      active_tab != nullptr && !active_tab->map_id.empty()) {
    return active_tab->map_id;
  }
  return bundle_->default_map_id;
}

std::string TrackerRuntime::FindVisibleTabForMap(std::string_view map_id) const {
  if (!bundle_ || map_id.empty()) {
    return {};
  }
  for (const auto& tab : bundle_->tabs) {
    if (tab.map_id == map_id && TabVisible(tab)) {
      return tab.id;
    }
  }
  return {};
}

}  // namespace sekaiemu::spike
