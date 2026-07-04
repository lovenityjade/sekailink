#pragma once

#include "nlohmann/json.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sekailink_server {

struct SeedConfigOptionChoice {
  std::string choice_key;
  std::string yaml_value;
  std::string label;
  std::string description;
};

struct SeedConfigOptionDefinition {
  std::string option_key;
  std::string yaml_key;
  std::string label;
  std::string description;
  std::string type;
  nlohmann::json default_value = nullptr;
  bool required = true;
  nlohmann::json visibility_rules = nullptr;
  nlohmann::json validation_rules = nullptr;
  std::vector<SeedConfigOptionChoice> choices;
};

struct SeedConfigValidationIssue {
  std::string option_key;
  std::string code;
  std::string detail;
};

struct SeedConfigValidationResult {
  bool ok = false;
  std::vector<SeedConfigValidationIssue> issues;
  nlohmann::json canonical_values = nlohmann::json::object();
};

struct SeedConfigSyncEntry {
  std::string entity_type;
  std::string entity_id;
  std::string version;
  std::string hash;
  std::string updated_at;
};

SeedConfigValidationResult validate_seed_config_values(
    const std::vector<SeedConfigOptionDefinition>& definitions,
    const nlohmann::json& values);

nlohmann::json seed_config_sync_manifest(
    std::int64_t user_id,
    const std::vector<SeedConfigSyncEntry>& entries,
    const std::optional<std::string>& cursor = std::nullopt);

std::string export_seed_config_yaml(
    const std::vector<SeedConfigOptionDefinition>& definitions,
    const nlohmann::json& canonical_values);

nlohmann::json seed_config_validation_issues_to_json(const std::vector<SeedConfigValidationIssue>& issues);

}  // namespace sekailink_server
