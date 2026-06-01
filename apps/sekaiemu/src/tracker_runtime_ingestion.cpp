#include "tracker_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
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

std::string JsonScalarToString(const nlohmann::json& value) {
  if (value.is_string()) {
    return TrimCopy(value.get<std::string>());
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? "ON" : "OFF";
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<std::int64_t>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<std::uint64_t>());
  }
  if (value.is_number_float()) {
    std::ostringstream out;
    out << value.get<double>();
    return out.str();
  }
  if (value.is_null()) {
    return {};
  }
  return value.dump();
}

std::optional<std::size_t> JsonArrayIndexFromToken(std::string_view token) {
  if (token.empty()) {
    return std::nullopt;
  }
  std::string_view numeric = token;
  if (token.front() == '[' && token.back() == ']' && token.size() > 2) {
    numeric = token.substr(1, token.size() - 2);
  }
  if (!std::all_of(numeric.begin(), numeric.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::stoul(std::string(numeric)));
}

const nlohmann::json* JsonValueAtPath(const nlohmann::json& root, std::string_view path) {
  if (path.empty()) {
    return &root;
  }
  const nlohmann::json* current = &root;
  std::size_t start = 0;
  while (start < path.size()) {
    const std::size_t dot = path.find('.', start);
    const std::string_view token =
        path.substr(start, dot == std::string_view::npos ? path.size() - start : dot - start);
    if (current->is_array()) {
      const auto index = JsonArrayIndexFromToken(token);
      if (!index.has_value() || *index >= current->size()) {
        return nullptr;
      }
      current = &(*current)[*index];
    } else if (current->is_object()) {
      const std::string key(token);
      const auto it = current->find(key);
      if (it == current->end()) {
        return nullptr;
      }
      current = &*it;
    } else {
      return nullptr;
    }
    if (dot == std::string_view::npos) {
      return current;
    }
    start = dot + 1;
  }
  return current;
}

const nlohmann::json* JsonValueAtAnyPath(const nlohmann::json& root,
                                         std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    if (const auto* value = JsonValueAtPath(root, path); value != nullptr) {
      return value;
    }
  }
  return nullptr;
}

std::string JsonStringAtAnyPath(const nlohmann::json& root,
                                std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    if (const auto* value = JsonValueAtPath(root, path); value != nullptr) {
      const auto rendered = JsonScalarToString(*value);
      if (!rendered.empty()) {
        return rendered;
      }
    }
  }
  return {};
}

std::optional<bool> JsonBoolAtAnyPath(const nlohmann::json& root,
                                      std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    const auto* value = JsonValueAtPath(root, path);
    if (value == nullptr) {
      continue;
    }
    if (value->is_boolean()) {
      return value->get<bool>();
    }
    if (value->is_number_integer()) {
      return value->get<std::int64_t>() != 0;
    }
    if (value->is_string()) {
      auto rendered = TrimCopy(value->get<std::string>());
      std::transform(rendered.begin(), rendered.end(), rendered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
      });
      if (rendered == "true" || rendered == "yes" || rendered == "on" || rendered == "auto" ||
          rendered == "1") {
        return true;
      }
      if (rendered == "false" || rendered == "no" || rendered == "off" || rendered == "manual" ||
          rendered == "0") {
        return false;
      }
    }
  }
  return std::nullopt;
}

std::unordered_set<std::int64_t> JsonInt64Set(const nlohmann::json& value) {
  std::unordered_set<std::int64_t> out;
  if (!value.is_array()) {
    return out;
  }
  for (const auto& entry : value) {
    if (entry.is_number_integer()) {
      out.insert(entry.get<std::int64_t>());
      continue;
    }
    if (entry.is_string()) {
      try {
        out.insert(std::stoll(entry.get<std::string>()));
      } catch (const std::exception&) {
      }
      continue;
    }
    if (!entry.is_object()) {
      continue;
    }
    for (const char* key : {"location_id", "locationId", "item_id", "itemId", "canonical_id", "canonicalId", "id"}) {
      const auto it = entry.find(key);
      if (it == entry.end()) {
        continue;
      }
      if (it->is_number_integer()) {
        out.insert(it->get<std::int64_t>());
        break;
      }
      if (it->is_string()) {
        try {
          out.insert(std::stoll(it->get<std::string>()));
          break;
        } catch (const std::exception&) {
        }
      }
    }
  }
  return out;
}

std::unordered_set<std::int64_t> JsonInt64SetAtAnyPath(const nlohmann::json& root,
                                                       std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    const auto* value = JsonValueAtPath(root, path);
    if (value != nullptr && value->is_array()) {
      return JsonInt64Set(*value);
    }
  }
  return {};
}

void MergeObjectIfMissing(nlohmann::json& destination, const nlohmann::json& source) {
  if (!destination.is_object() || !source.is_object()) {
    return;
  }
  for (auto it = source.begin(); it != source.end(); ++it) {
    if (!destination.contains(it.key())) {
      destination[it.key()] = it.value();
    }
  }
}

nlohmann::json ExtractSeedMetadata(const nlohmann::json& snapshot) {
  nlohmann::json metadata = nlohmann::json::object();
  if (const auto* seed = JsonValueAtAnyPath(snapshot, {"seed_metadata", "seedMetadata"});
      seed != nullptr && seed->is_object()) {
    metadata = *seed;
  }
  if (const auto* settings = JsonValueAtAnyPath(snapshot, {"seed_settings", "seedSettings"});
      settings != nullptr && settings->is_object()) {
    metadata["settings"] = *settings;
    MergeObjectIfMissing(metadata, *settings);
  }
  if (const auto* settings = JsonValueAtPath(snapshot, "settings");
      settings != nullptr && settings->is_object()) {
    metadata["settings"] = *settings;
  }
  if (const auto* slot_data = JsonValueAtAnyPath(snapshot, {"slot_data", "slotData"});
      slot_data != nullptr && slot_data->is_object() && !metadata.contains("slot_data")) {
    metadata["slot_data"] = *slot_data;
  }
  if (const auto* slot_data = JsonValueAtPath(metadata, "slotData");
      slot_data != nullptr && slot_data->is_object() && !metadata.contains("slot_data")) {
    metadata["slot_data"] = *slot_data;
  }
  if (!metadata.contains("tracker_pack")) {
    if (const auto* pack = JsonValueAtAnyPath(snapshot, {"status.pack", "status.tracker_pack"});
        pack != nullptr && pack->is_string()) {
      metadata["tracker_pack"] = *pack;
    }
  }
  if (!metadata.contains("tracker_variant")) {
    if (const auto* variant = JsonValueAtAnyPath(snapshot, {"status.variant", "status.tracker_variant"});
        variant != nullptr && variant->is_string()) {
      metadata["tracker_variant"] = *variant;
    }
  }
  return metadata;
}

nlohmann::json NormalizeServerSnapshot(const nlohmann::json& snapshot) {
  if (!snapshot.is_object()) {
    return snapshot;
  }
  nlohmann::json normalized = snapshot;
  if (const auto* nested = JsonValueAtAnyPath(snapshot, {"snapshot", "server_snapshot", "serverSnapshot"});
      nested != nullptr && nested->is_object()) {
    normalized = *nested;
    MergeObjectIfMissing(normalized, snapshot);
  }
  return normalized;
}

}  // namespace

void TrackerRuntime::ApplyServerSnapshot(const nlohmann::json& snapshot) {
  const auto normalized_snapshot = NormalizeServerSnapshot(snapshot);
  authoritative_state_.snapshot = normalized_snapshot;
  authoritative_state_.world_instance_id =
      JsonStringAtAnyPath(normalized_snapshot, {"world_instance_id", "worldInstanceId"});
  authoritative_state_.linkedworld_id =
      JsonStringAtAnyPath(normalized_snapshot, {"linkedworld_id", "linkedworldId"});
  authoritative_state_.slot_id =
      JsonStringAtAnyPath(normalized_snapshot, {"slot_id", "slotId", "slot"});
  authoritative_state_.seed_id =
      JsonStringAtAnyPath(normalized_snapshot, {"seed_id", "seedId", "seed", "seed_name", "seedName"});
  authoritative_state_.active_map_hint = JsonStringAtAnyPath(
      normalized_snapshot,
      {"active_map_id",
       "active_map",
       "activeMapId",
       "activeMap",
       "tracker_state.active_map_id",
       "trackerState.activeMapId",
       "ui_state.active_map_id",
       "uiState.activeMapId",
       "map.active",
       "map.active_id",
       "map.activeId"});
  authoritative_state_.active_tab_hint = JsonStringAtAnyPath(
      normalized_snapshot,
      {"active_tab_id",
       "active_tab",
       "activeTabId",
       "activeTab",
       "tracker_state.active_tab_id",
       "trackerState.activeTabId",
       "ui_state.active_tab_id",
       "uiState.activeTabId",
       "tab.active",
       "tab.active_id",
       "tab.activeId"});
  authoritative_state_.auto_follow_map_hint = JsonBoolAtAnyPath(
      normalized_snapshot,
      {"auto_follow_map",
       "autoFollowMap",
       "autotab",
       "auto_tab",
       "autoTab",
       "tracker_state.auto_follow_map",
       "trackerState.autoFollowMap",
       "ui_state.auto_follow_map",
       "uiState.autoFollowMap"});
  if (authoritative_state_.auto_follow_map_hint.has_value() &&
      !local_override_state_.manual_map_id.has_value()) {
    local_override_state_.auto_follow_map = *authoritative_state_.auto_follow_map_hint;
  }
  authoritative_state_.checked_locations = JsonInt64SetAtAnyPath(
      normalized_snapshot,
      {"checked_locations", "checkedLocations", "locations.checked", "location_state.checked"});
  authoritative_state_.missing_locations = JsonInt64SetAtAnyPath(
      normalized_snapshot,
      {"missing_locations", "missingLocations", "locations.missing", "location_state.missing"});
  authoritative_state_.received_items = JsonInt64SetAtAnyPath(
      normalized_snapshot,
      {"received_items", "receivedItems", "items.received", "items.received_items", "item_state.received"});
  const auto seed_metadata = ExtractSeedMetadata(normalized_snapshot);
  if (!seed_metadata.empty()) {
    ApplySeedMetadata(seed_metadata);
    return;
  }
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::ApplySklmiEvent(const nlohmann::json& event) {
  observed_state_.driver_instance_id =
      JsonString(event, {"driver_instance_id", "driverInstanceId"});
  if (const auto linkedworld_id = JsonString(event, {"linkedworld_id", "linkedworldId"});
      !linkedworld_id.empty()) {
    observed_state_.linkedworld_id = linkedworld_id;
  }
  if (const auto slot_id = JsonString(event, {"slot_id", "slotId"}); !slot_id.empty()) {
    observed_state_.slot_id = slot_id;
  }
  if (const auto core_profile = JsonString(event, {"core_profile", "coreProfile"});
      !core_profile.empty()) {
    observed_state_.core_profile = core_profile;
  }
  const auto event_type = JsonString(event, {"event_type", "eventType", "type"});
  if (event_type == "location_checked") {
    if (event.contains("location_id") && event["location_id"].is_number_integer()) {
      observed_state_.locally_checked_locations.insert(event["location_id"].get<std::int64_t>());
    } else if (event.contains("canonical_id") && event["canonical_id"].is_number_integer()) {
      observed_state_.locally_checked_locations.insert(event["canonical_id"].get<std::int64_t>());
    }
  } else if (event_type == "item_received") {
    if (event.contains("item_id") && event["item_id"].is_number_integer()) {
      observed_state_.locally_received_items.insert(event["item_id"].get<std::int64_t>());
    } else if (event.contains("canonical_id") && event["canonical_id"].is_number_integer()) {
      observed_state_.locally_received_items.insert(event["canonical_id"].get<std::int64_t>());
    }
  } else if (event_type == "runtime_context" || event_type == "zone_changed") {
    if (event.contains("runtime_context") && event["runtime_context"].is_object()) {
      ApplyRuntimeContext(event["runtime_context"]);
      return;
    }
    ApplyRuntimeContext(event);
    return;
  }

  if (event_type == "location_checked" || event_type == "item_received") {
    TrackerRecentEvent recent;
    recent.event_type = event_type;
    recent.key = JsonString(event, {"event_key", "eventKey", "key"});
    recent.label = JsonString(
        event, {"label", "mapped_value", "mappedValue", "location_name", "locationName", "item_name", "itemName", "value"});
    recent.timestamp = JsonString(event, {"timestamp", "time"});
    if (event.contains("location_id") && event["location_id"].is_number_integer()) {
      recent.canonical_id = event["location_id"].get<std::int64_t>();
    } else if (event.contains("item_id") && event["item_id"].is_number_integer()) {
      recent.canonical_id = event["item_id"].get<std::int64_t>();
    } else if (event.contains("canonical_id") && event["canonical_id"].is_number_integer()) {
      recent.canonical_id = event["canonical_id"].get<std::int64_t>();
    }
    const bool duplicate_last =
        !observed_state_.recent_events.empty() &&
        observed_state_.recent_events.back().event_type == recent.event_type &&
        observed_state_.recent_events.back().key == recent.key &&
        observed_state_.recent_events.back().label == recent.label;
    if (!duplicate_last) {
      observed_state_.recent_events.push_back(std::move(recent));
      constexpr std::size_t kMaxRecentEvents = 16;
      if (observed_state_.recent_events.size() > kMaxRecentEvents) {
        observed_state_.recent_events.erase(
            observed_state_.recent_events.begin(),
            observed_state_.recent_events.begin() +
                static_cast<std::ptrdiff_t>(observed_state_.recent_events.size() - kMaxRecentEvents));
      }
    }
  }
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

void TrackerRuntime::ApplyRuntimeContext(const nlohmann::json& context) {
  observed_state_.runtime_context = context;
  const auto zone_id =
      JsonString(context, {"zone_id", "zoneId", "zone", "scene_id", "sceneId"});
  if (!zone_id.empty()) {
    observed_state_.current_zone_id = zone_id;
  }
  EnsureSelectionConsistency();
  BumpMutationSerial();
}

}  // namespace sekaiemu::spike
