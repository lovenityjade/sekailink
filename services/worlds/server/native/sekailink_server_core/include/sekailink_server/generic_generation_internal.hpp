#pragma once

#include "nlohmann/json.hpp"

#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace sekailink_server::generation_internal {

std::string read_text_file(const std::filesystem::path& path);
void write_json_file(const std::filesystem::path& path, const nlohmann::json& value);
std::string sha256_hex(const std::string& value);
std::string utc_timestamp_now();
std::string required_string(const nlohmann::json& value, const std::string& key);

std::filesystem::path resolve_generation_ir_path(const std::filesystem::path& linkedworld_root,
                                                 const nlohmann::json& manifest);
std::filesystem::path resolve_linkedworld_ref(const std::filesystem::path& linkedworld_root,
                                              const std::string& ref);
std::optional<nlohmann::json> read_json_ref(const std::filesystem::path& linkedworld_root,
                                            const nlohmann::json& catalog,
                                            const std::string& key);

std::string string_from_number_or_string(const nlohmann::json& value);
std::string json_field_as_string(const nlohmann::json& value, const std::string& key);
std::string first_non_empty(std::initializer_list<std::string> values);
std::string base_instance_id(std::string id);
std::string slot_entity_key(int slot_id, const std::string& id);
std::vector<std::string> json_string_array(const nlohmann::json& value);
std::set<std::string> json_string_set(const nlohmann::json& value);
bool has_any_tag(const std::set<std::string>& available, const std::vector<std::string>& required);
bool string_contains(const std::string& value, const std::string& needle);
bool truthy_json_flag(const nlohmann::json& value);
bool object_flag_is_true(const nlohmann::json& object, const std::string& key);
std::optional<bool> object_truthy_flag(const nlohmann::json& object, const std::string& key);
std::optional<bool> first_object_truthy_flag(const nlohmann::json& object,
                                             std::initializer_list<const char*> keys);
bool json_array_contains_string(const nlohmann::json& value, const std::string& needle);
bool json_contains_string_fragment(const nlohmann::json& value, const std::string& needle);
bool json_object_status_contains(const nlohmann::json& value, const std::string& needle);

void expand_catalog_refs(const std::filesystem::path& linkedworld_root, nlohmann::json& catalog);
nlohmann::json read_generation_rule_refs(const std::filesystem::path& linkedworld_root,
                                         const nlohmann::json& catalog);
void expand_patch_manifest_ref(const std::filesystem::path& linkedworld_root, nlohmann::json& patch);

std::string package_hash_material(const std::map<std::string, nlohmann::json>& files);

}  // namespace sekailink_server::generation_internal
