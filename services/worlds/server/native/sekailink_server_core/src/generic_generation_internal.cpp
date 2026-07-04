#include "sekailink_server/generic_generation_internal.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sekailink_server::generation_internal {

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("generic_generation_read_failed");
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

void write_json_file(const std::filesystem::path& path, const nlohmann::json& value) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("generic_generation_write_failed");
  }
  stream << value.dump(2) << "\n";
}

std::string sha256_hex(const std::string& value) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest.data());
  std::ostringstream out;
  for (const auto byte : digest) {
    out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  }
  return out.str();
}

std::string utc_timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &time);
#else
  gmtime_r(&time, &utc);
#endif
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string required_string(const nlohmann::json& value, const std::string& key) {
  if (!value.contains(key) || !value.at(key).is_string() || value.at(key).get<std::string>().empty()) {
    throw std::runtime_error("missing_" + key);
  }
  return value.at(key).get<std::string>();
}

std::filesystem::path resolve_generation_ir_path(const std::filesystem::path& linkedworld_root,
                                                 const nlohmann::json& manifest) {
  if (manifest.contains("module_blocks") && manifest.at("module_blocks").contains("generation_ir")) {
    const auto block = manifest.at("module_blocks").at("generation_ir");
    if (block.contains("path") && block.at("path").is_string()) {
      return linkedworld_root / block.at("path").get<std::string>();
    }
  }
  if (manifest.contains("generation") && manifest.at("generation").contains("ir_path") &&
      manifest.at("generation").at("ir_path").is_string()) {
    return linkedworld_root / manifest.at("generation").at("ir_path").get<std::string>();
  }
  return linkedworld_root / "generation" / "generation-ir.json";
}

std::filesystem::path resolve_linkedworld_ref(const std::filesystem::path& linkedworld_root,
                                              const std::string& ref) {
  const auto path = std::filesystem::path(ref);
  if (path.is_absolute()) {
    return path;
  }
  return linkedworld_root / path;
}

std::optional<nlohmann::json> read_json_ref(const std::filesystem::path& linkedworld_root,
                                            const nlohmann::json& catalog,
                                            const std::string& key) {
  if (!catalog.contains(key) || !catalog.at(key).is_string()) {
    return std::nullopt;
  }
  const auto path = resolve_linkedworld_ref(linkedworld_root, catalog.at(key).get<std::string>());
  return nlohmann::json::parse(read_text_file(path));
}

std::string string_from_number_or_string(const nlohmann::json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<std::int64_t>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<std::uint64_t>());
  }
  return {};
}

std::string json_field_as_string(const nlohmann::json& value, const std::string& key) {
  if (!value.is_object() || !value.contains(key)) {
    return {};
  }
  return string_from_number_or_string(value.at(key));
}

std::string first_non_empty(std::initializer_list<std::string> values) {
  for (const auto& value : values) {
    if (!value.empty()) {
      return value;
    }
  }
  return {};
}

std::string base_instance_id(std::string id) {
  const auto marker = id.find('#');
  if (marker != std::string::npos) {
    id.resize(marker);
  }
  return id;
}

std::string slot_entity_key(int slot_id, const std::string& id) {
  return std::to_string(slot_id) + ":" + id;
}

std::vector<std::string> json_string_array(const nlohmann::json& value) {
  std::vector<std::string> strings;
  if (!value.is_array()) {
    return strings;
  }
  for (const auto& entry : value) {
    const auto text = string_from_number_or_string(entry);
    if (!text.empty()) {
      strings.push_back(text);
    }
  }
  return strings;
}

std::set<std::string> json_string_set(const nlohmann::json& value) {
  const auto strings = json_string_array(value);
  return {strings.begin(), strings.end()};
}

bool has_any_tag(const std::set<std::string>& available, const std::vector<std::string>& required) {
  return std::any_of(required.begin(), required.end(), [&](const auto& tag) {
    return available.count(tag) > 0;
  });
}

bool string_contains(const std::string& value, const std::string& needle) {
  return value.find(needle) != std::string::npos;
}

bool truthy_json_flag(const nlohmann::json& value) {
  if (value.is_boolean()) {
    return value.get<bool>();
  }
  if (value.is_string()) {
    const auto text = value.get<std::string>();
    return text == "true" || text == "yes" || text == "1";
  }
  return false;
}

bool object_flag_is_true(const nlohmann::json& object, const std::string& key) {
  return object.is_object() && object.contains(key) && truthy_json_flag(object.at(key));
}

std::optional<bool> object_truthy_flag(const nlohmann::json& object, const std::string& key) {
  if (!object.is_object() || !object.contains(key)) {
    return std::nullopt;
  }
  return truthy_json_flag(object.at(key));
}

std::optional<bool> first_object_truthy_flag(const nlohmann::json& object,
                                             std::initializer_list<const char*> keys) {
  for (const auto* key : keys) {
    if (const auto value = object_truthy_flag(object, key)) {
      return value;
    }
  }
  return std::nullopt;
}

bool json_array_contains_string(const nlohmann::json& value, const std::string& needle) {
  if (!value.is_array()) {
    return false;
  }
  return std::any_of(value.begin(), value.end(), [&](const auto& entry) {
    return entry.is_string() && entry.template get<std::string>() == needle;
  });
}

bool json_contains_string_fragment(const nlohmann::json& value, const std::string& needle) {
  if (value.is_string()) {
    return value.get<std::string>().find(needle) != std::string::npos;
  }
  if (value.is_array()) {
    return std::any_of(value.begin(), value.end(), [&](const auto& child) {
      return json_contains_string_fragment(child, needle);
    });
  }
  if (value.is_object()) {
    return std::any_of(value.begin(), value.end(), [&](const auto& child) {
      return json_contains_string_fragment(child, needle);
    });
  }
  return false;
}

bool json_object_status_contains(const nlohmann::json& value, const std::string& needle) {
  if (!value.is_object()) {
    return false;
  }
  return value.contains("status") && value.at("status").is_string() &&
         string_contains(value.at("status").get<std::string>(), needle);
}

namespace {

void normalize_location_group_catalog(const nlohmann::json& source, nlohmann::json& catalog) {
  nlohmann::json locations = nlohmann::json::array();
  for (const auto& group : source.value("groups", nlohmann::json::array())) {
    for (const auto& location : group.value("locations", nlohmann::json::array())) {
      auto normalized = location;
      normalized["group_id"] = group.value("group_id", std::string{});
      normalized["group_label"] = group.value("label", std::string{});
      normalized["preferred_tab"] = group.value("preferred_tab", std::string{});
      if (!normalized.contains("id")) {
        normalized["id"] = string_from_number_or_string(normalized.value("location_id", nlohmann::json{}));
      }
      if (!normalized.contains("name")) {
        normalized["name"] = normalized.value("location_name", std::string{});
      }
      locations.push_back(std::move(normalized));
    }
  }
  catalog["locations"] = std::move(locations);
  catalog["location_groups_source"] = source;
}

void normalize_item_slot_catalog(const nlohmann::json& source, nlohmann::json& catalog) {
  nlohmann::json items = nlohmann::json::array();
  for (const auto& slot : source.value("slots", nlohmann::json::array())) {
    for (const auto& item : slot.value("items", nlohmann::json::array())) {
      auto normalized = item;
      normalized["tracker_slot_id"] = slot.value("slot_id", std::string{});
      normalized["tracker_slot_label"] = slot.value("label", std::string{});
      normalized["tracker_group_id"] = slot.value("group_id", std::string{});
      normalized["tracker_behavior"] = slot.value("behavior", std::string{});
      if (!normalized.contains("id")) {
        normalized["id"] = string_from_number_or_string(normalized.value("item_id", nlohmann::json{}));
      }
      if (!normalized.contains("name")) {
        normalized["name"] = normalized.value("item_name", std::string{});
      }
      if (!normalized.contains("classification")) {
        normalized["classification"] = "unknown";
      }
      items.push_back(std::move(normalized));
    }
  }
  catalog["items"] = std::move(items);
  catalog["item_slots_source"] = source;
}

void normalize_item_pool_catalog(const nlohmann::json& source, nlohmann::json& catalog) {
  const auto raw_items = source.is_object() && source.contains("items") ? source.at("items") : source;
  nlohmann::json expanded = nlohmann::json::array();
  for (const auto& item : raw_items) {
    const auto count = item.value("count", 1);
    if (count <= 0) {
      continue;
    }
    for (int index = 0; index < count; ++index) {
      auto expanded_item = item;
      expanded_item.erase("count");
      if (!expanded_item.contains("name")) {
        expanded_item["name"] = expanded_item.value("id", std::string{});
      }
      if (!expanded_item.contains("id")) {
        expanded_item["id"] = expanded_item.value("semantic_id", expanded_item.value("name", std::string{}));
      }
      if (count > 1) {
        expanded_item["pool_instance"] = index + 1;
        expanded_item["id"] = expanded_item.value("id", std::string{}) + "#" + std::to_string(index + 1);
      }
      expanded.push_back(std::move(expanded_item));
    }
  }
  catalog["item_pool"] = std::move(expanded);
}

}  // namespace

void expand_catalog_refs(const std::filesystem::path& linkedworld_root, nlohmann::json& catalog) {
  if (!catalog.contains("locations")) {
    if (const auto source = read_json_ref(linkedworld_root, catalog, "locations_ref")) {
      const auto shape = catalog.value("locations_shape", std::string{"array"});
      if (shape == "location_groups") {
        normalize_location_group_catalog(*source, catalog);
      } else if (source->is_object() && source->contains("locations")) {
        catalog["locations"] = source->at("locations");
      } else {
        catalog["locations"] = *source;
      }
    }
  }

  if (!catalog.contains("items")) {
    if (const auto source = read_json_ref(linkedworld_root, catalog, "items_ref")) {
      const auto shape = catalog.value("items_shape", std::string{"array"});
      if (shape == "item_slots") {
        normalize_item_slot_catalog(*source, catalog);
      } else if (source->is_object() && source->contains("items")) {
        catalog["items"] = source->at("items");
      } else {
        catalog["items"] = *source;
      }
    }
  }

  if (!catalog.contains("item_pool")) {
    if (const auto source = read_json_ref(linkedworld_root, catalog, "item_pool_ref")) {
      const auto shape = catalog.value("item_pool_shape", std::string{"item_pool"});
      if (shape == "item_slots") {
        nlohmann::json pool_catalog = nlohmann::json::object();
        normalize_item_slot_catalog(*source, pool_catalog);
        catalog["item_pool"] = pool_catalog.value("items", nlohmann::json::array());
      } else {
        normalize_item_pool_catalog(*source, catalog);
      }
      catalog["item_pool_source"] = *source;
    }
  }
}

nlohmann::json read_generation_rule_refs(const std::filesystem::path& linkedworld_root,
                                         const nlohmann::json& catalog) {
  nlohmann::json rules = nlohmann::json::object();
  if (const auto source = read_json_ref(linkedworld_root, catalog, "logic_rules_ref")) {
    rules["logic_rules"] = *source;
    rules["logic_rules_shape"] = catalog.value("logic_rules_shape", std::string{});
    rules["logic_rules_ref"] = catalog.value("logic_rules_ref", std::string{});
  }
  if (const auto source = read_json_ref(linkedworld_root, catalog, "placement_rules_ref")) {
    auto placement_rules = *source;
    if (placement_rules.contains("fillable_locations_ref") &&
        placement_rules.at("fillable_locations_ref").is_string()) {
      const auto fillable_path =
          resolve_linkedworld_ref(linkedworld_root, placement_rules.at("fillable_locations_ref").get<std::string>());
      placement_rules["fillable_locations_source"] = nlohmann::json::parse(read_text_file(fillable_path));
    }
    rules["placement_rules"] = std::move(placement_rules);
    rules["placement_rules_shape"] = catalog.value("placement_rules_shape", std::string{});
    rules["placement_rules_ref"] = catalog.value("placement_rules_ref", std::string{});
  }
  return rules;
}

void expand_patch_manifest_ref(const std::filesystem::path& linkedworld_root, nlohmann::json& patch) {
  if (!patch.is_object() || !patch.contains("manifest_ref") || !patch.at("manifest_ref").is_string()) {
    return;
  }
  const auto manifest_ref = patch.at("manifest_ref").get<std::string>();
  const auto manifest_path = resolve_linkedworld_ref(linkedworld_root, manifest_ref);
  const auto manifest = nlohmann::json::parse(read_text_file(manifest_path));
  patch["manifest_ref_resolved"] = manifest_path.string();
  patch["manifest"] = manifest;

  const auto manifest_patch = manifest.value("patch", nlohmann::json::object());
  if (!patch.contains("mode") && manifest_patch.contains("mode") && manifest_patch.at("mode").is_string()) {
    patch["mode"] = manifest_patch.at("mode");
  }
  if (!patch.contains("artifact_kind") && manifest_patch.contains("artifact_kind")) {
    patch["artifact_kind"] = manifest_patch.at("artifact_kind");
  }
  if (!patch.contains("patch_file_extension") && manifest_patch.contains("artifact_extension")) {
    patch["patch_file_extension"] = manifest_patch.at("artifact_extension");
  }
}

std::string package_hash_material(const std::map<std::string, nlohmann::json>& files) {
  nlohmann::json material = nlohmann::json::object();
  for (const auto& [name, value] : files) {
    material[name] = value;
  }
  return material.dump();
}

}  // namespace sekailink_server::generation_internal
