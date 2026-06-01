#include "sekailink_server/room_linkedworld.hpp"

#include <set>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sekailink_server {

namespace {

std::optional<std::string> optional_string(const nlohmann::json& value, const char* key) {
  if (!value.contains(key) || value.at(key).is_null() || !value.at(key).is_string()) {
    return std::nullopt;
  }
  return value.at(key).get<std::string>();
}

void set_optional(nlohmann::json& out, const char* key, const std::optional<std::string>& value) {
  out[key] = value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

nlohmann::json object_or_empty(const nlohmann::json& input, const char* key) {
  if (!input.contains(key) || input.at(key).is_null() || !input.at(key).is_object()) {
    return nlohmann::json::object();
  }
  return input.at(key);
}

nlohmann::json array_or_empty(const nlohmann::json& input, const char* key) {
  if (!input.contains(key) || input.at(key).is_null() || !input.at(key).is_array()) {
    return nlohmann::json::array();
  }
  return input.at(key);
}

std::optional<std::string> first_string(
    const nlohmann::json& first,
    const char* first_key,
    const nlohmann::json& second = nlohmann::json::object(),
    const char* second_key = "") {
  if (const auto value = optional_string(first, first_key); value.has_value()) {
    return value;
  }
  if (second_key[0] != '\0') {
    return optional_string(second, second_key);
  }
  return std::nullopt;
}

void push_unique(std::vector<std::string>& values, std::set<std::string>& seen, std::string value) {
  if (value.empty() || seen.contains(value)) {
    return;
  }
  seen.insert(value);
  values.push_back(std::move(value));
}

nlohmann::json key_array_from_metadata_entries(const nlohmann::json& entries) {
  std::vector<std::string> keys;
  std::set<std::string> seen;
  for (const auto& entry : entries) {
    if (entry.is_string()) {
      push_unique(keys, seen, entry.get<std::string>());
    } else if (entry.is_object()) {
      if (const auto key = optional_string(entry, "key"); key.has_value()) {
        push_unique(keys, seen, *key);
      }
    }
  }
  return keys;
}

nlohmann::json slot_data_visible_fields(const nlohmann::json& slot_data_contract, const nlohmann::json& preset) {
  std::vector<std::string> fields;
  std::set<std::string> seen;
  const auto room_metadata = object_or_empty(preset, "room_metadata");
  for (const auto& field : array_or_empty(room_metadata, "slot_data_recommended_visible_fields")) {
    if (field.is_string()) {
      push_unique(fields, seen, field.get<std::string>());
    }
  }
  for (const auto& panel : array_or_empty(slot_data_contract, "panels")) {
    for (const auto& field : array_or_empty(panel, "fields")) {
      if (const auto key = optional_string(field, "slot_data_key"); key.has_value()) {
        push_unique(fields, seen, *key);
      }
    }
  }
  return fields;
}

nlohmann::json bridge_summary(const nlohmann::json& bridge) {
  const auto sklmi = object_or_empty(bridge, "sklmi");
  const auto checks = array_or_empty(sklmi, "checks");
  const auto actions = array_or_empty(sklmi, "actions");
  std::set<std::string> event_types;
  std::size_t room_controlled_action_count = 0;
  for (const auto& check : checks) {
    if (const auto event_type = optional_string(check, "event_type"); event_type.has_value()) {
      event_types.insert(*event_type);
    }
  }
  for (const auto& action : actions) {
    if (action.value("room_controlled", false)) {
      ++room_controlled_action_count;
    }
    if (const auto event_type = optional_string(action, "event_type"); event_type.has_value()) {
      event_types.insert(*event_type);
    }
  }
  return {
      {"bridge_id", optional_string(sklmi, "bridge_id").value_or("")},
      {"core_profile", optional_string(sklmi, "core_profile").value_or("")},
      {"driver_instance_id", optional_string(sklmi, "driver_instance_id").value_or("")},
      {"check_count", checks.size()},
      {"room_controlled_action_count", room_controlled_action_count},
      {"official_event_types", std::vector<std::string>(event_types.begin(), event_types.end())},
  };
}

nlohmann::json item_semantics_from_bridge(const nlohmann::json& bridge) {
  const auto actions = array_or_empty(object_or_empty(bridge, "sklmi"), "actions");
  nlohmann::json semantics = nlohmann::json::array();
  for (const auto& action : actions) {
    if (!action.value("room_controlled", false) || !action.contains("item_id")) {
      continue;
    }
    nlohmann::json semantic = {
        {"item_id", action.at("item_id")},
        {"item_name", action.value("item_name", "")},
    };
    set_optional(semantic, "event_key", optional_string(action, "event_key"));
    set_optional(semantic, "mapped_value", optional_string(action, "mapped_value"));
    set_optional(semantic, "tracker_semantic_id", optional_string(action, "tracker_semantic_id"));
    if (semantic.at("tracker_semantic_id").is_null() && !semantic.at("event_key").is_null()) {
      semantic["tracker_semantic_id"] = semantic.at("event_key");
    }
    semantics.push_back(std::move(semantic));
  }
  return semantics;
}

}  // namespace

nlohmann::json build_linkedworld_room_surface(const nlohmann::json& linkedworld_payload) {
  if (linkedworld_payload.contains("linkedworld_surface") &&
      linkedworld_payload.at("linkedworld_surface").is_object()) {
    return linkedworld_payload.at("linkedworld_surface");
  }

  const auto manifest = object_or_empty(linkedworld_payload, "manifest");
  const auto room_metadata = object_or_empty(linkedworld_payload, "room_metadata");
  const auto slot_data_contract = object_or_empty(linkedworld_payload, "slot_data_contract");
  const auto preset = object_or_empty(linkedworld_payload, "preset");
  const auto bridge = object_or_empty(linkedworld_payload, "bridge");
  const auto patch = object_or_empty(linkedworld_payload, "patch");
  const auto patch_contract = object_or_empty(patch, "declarative_contract");
  const auto patch_server_dispatch = object_or_empty(patch_contract, "server_dispatch");
  const auto runtime_requirements = object_or_empty(manifest, "runtime_requirements");
  const auto module_blocks = object_or_empty(manifest, "module_blocks");
  const auto manifest_room_surface = object_or_empty(manifest, "room_surface");
  const auto preset_runtime = object_or_empty(preset, "runtime");
  const auto preset_room_metadata = object_or_empty(preset, "room_metadata");

  nlohmann::json surface = {
      {"source", "linkedworld"},
      {"runtime_requirements", runtime_requirements},
      {"bridge", bridge_summary(bridge)},
      {"item_semantics", item_semantics_from_bridge(bridge)},
      {"room_metadata_required_keys",
       !room_metadata.empty() ? key_array_from_metadata_entries(array_or_empty(room_metadata, "required_keys"))
                              : key_array_from_metadata_entries(array_or_empty(manifest_room_surface, "required_runtime_metadata"))},
      {"room_metadata_recommended_optional_keys",
       !room_metadata.empty() ? key_array_from_metadata_entries(array_or_empty(room_metadata, "recommended_optional_keys"))
                              : key_array_from_metadata_entries(array_or_empty(preset_room_metadata, "recommended_optional"))},
      {"slot_data_visible_fields", slot_data_visible_fields(slot_data_contract, preset)},
      {"presentation",
       {
           {"mode", preset_runtime.value("presentation_mode", "")},
           {"left_panel", preset_runtime.value("left_panel", "")},
           {"right_panel", preset_runtime.value("right_panel", "")},
           {"tracker_focus", preset_runtime.value("tracker_focus", nlohmann::json::array())},
       }},
      {"delivery",
       {
           {"patch_mode", patch_contract.value("patch_mode", std::string{})},
           {"patch_type", patch_contract.value("patch_type", std::string{})},
           {"server_dispatch_enabled", patch_server_dispatch.value("enabled", false)},
           {"server_dispatch_target", patch_server_dispatch.value("target", std::string{})},
           {"server_dispatch_transport", patch_server_dispatch.value("transport", std::string{})},
           {"server_dispatch_payload_ref", patch_server_dispatch.value("payload_ref", std::string{})},
           {"server_dispatch_ack_required", patch_server_dispatch.value("ack_required", false)},
       }},
      {"refs",
       {
           {"bridge", manifest_room_surface.value("bridge_ref", "")},
           {"metadata", object_or_empty(module_blocks, "metadata").value("path", "")},
           {"patch", object_or_empty(module_blocks, "patch").value("path", "")},
           {"preset", object_or_empty(module_blocks, "preset_default").value("path", "")},
           {"room_metadata", preset_runtime.value("room_metadata_ref", "")},
           {"slot_data", preset_runtime.value("slot_data_ref", "")},
       }},
  };

  set_optional(surface, "linkedworld_id",
               first_string(manifest, "linkedworld_id", room_metadata, "linkedworld_id")
                   .value_or(first_string(manifest, "game_id", bridge, "id").value_or("")));
  set_optional(surface, "module_id", optional_string(manifest, "module_id"));
  set_optional(surface, "game_id", first_string(manifest, "game_id", bridge, "id"));
  set_optional(surface, "display_name", optional_string(manifest, "display_name"));
  set_optional(surface, "version", optional_string(manifest, "version"));
  set_optional(surface, "family_system_id", optional_string(manifest, "family_system_id"));
  set_optional(surface, "runtime_host", optional_string(runtime_requirements, "host"));
  set_optional(surface, "runtime_memory_interface", optional_string(runtime_requirements, "memory_interface"));
  set_optional(surface, "runtime_system", optional_string(runtime_requirements, "system"));
  set_optional(surface, "runner", optional_string(runtime_requirements, "runner"));
  set_optional(surface, "delivery_mode", first_string(patch_contract, "patch_mode", runtime_requirements, "memory_interface"));
  set_optional(surface, "tracker_pack", first_string(manifest, "default_tracker_pack", preset_runtime, "tracker_pack_path"));
  set_optional(surface, "tracker_variant", first_string(manifest, "default_tracker_variant", preset_runtime, "tracker_variant"));
  set_optional(surface, "room_metadata_contract_id", optional_string(room_metadata, "contract_id"));
  set_optional(surface, "slot_data_contract_id", optional_string(slot_data_contract, "contract_id"));
  set_optional(surface, "preset_id", optional_string(preset, "preset_id"));

  return surface;
}

}  // namespace sekailink_server
