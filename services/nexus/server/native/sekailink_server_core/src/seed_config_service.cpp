#include "sekailink_server/seed_config_service.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace sekailink_server {

namespace {

bool is_integer_number(const nlohmann::json& value) {
  return value.is_number_integer() || value.is_number_unsigned();
}

std::string yaml_scalar(const nlohmann::json& value) {
  if (value.is_boolean()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.is_number_integer() || value.is_number_unsigned() || value.is_number_float()) {
    return value.dump();
  }
  if (value.is_string()) {
    const auto text = value.get<std::string>();
    const bool plain = !text.empty() && text.find_first_of(":#{}[]&,*!|>'\"%@`") == std::string::npos;
    if (plain) {
      return text;
    }
    return value.dump();
  }
  return value.dump();
}

void append_issue(std::vector<SeedConfigValidationIssue>& issues,
                  std::string option_key,
                  std::string code,
                  std::string detail) {
  issues.push_back(SeedConfigValidationIssue{
      .option_key = std::move(option_key),
      .code = std::move(code),
      .detail = std::move(detail),
  });
}

bool choice_allowed(const SeedConfigOptionDefinition& definition, const nlohmann::json& value) {
  if (!value.is_string()) {
    return false;
  }
  const auto text = value.get<std::string>();
  return std::any_of(definition.choices.begin(), definition.choices.end(), [&text](const auto& choice) {
    return choice.choice_key == text || choice.yaml_value == text;
  });
}

std::optional<double> number_rule(const nlohmann::json& rules, const char* key) {
  if (!rules.is_object() || !rules.contains(key) || !rules.at(key).is_number()) {
    return std::nullopt;
  }
  return rules.at(key).get<double>();
}

}  // namespace

SeedConfigValidationResult validate_seed_config_values(
    const std::vector<SeedConfigOptionDefinition>& definitions,
    const nlohmann::json& values) {
  SeedConfigValidationResult result;
  if (!values.is_object()) {
    append_issue(result.issues, "", "invalid_payload", "values must be a JSON object");
    result.ok = false;
    return result;
  }

  std::unordered_map<std::string, const SeedConfigOptionDefinition*> by_key;
  std::unordered_map<std::string, std::string> yaml_to_key;
  by_key.reserve(definitions.size());
  for (const auto& definition : definitions) {
    by_key[definition.option_key] = &definition;
    yaml_to_key[definition.yaml_key] = definition.option_key;
  }

  std::unordered_set<std::string> seen;
  for (const auto& [key, value] : values.items()) {
    auto effective_key = key;
    if (!by_key.contains(effective_key)) {
      const auto yaml_it = yaml_to_key.find(key);
      if (yaml_it != yaml_to_key.end()) {
        effective_key = yaml_it->second;
      }
    }
    const auto it = by_key.find(effective_key);
    if (it == by_key.end()) {
      append_issue(result.issues, key, "unknown_option", "option is not defined by the active game schema");
      continue;
    }
    const auto& definition = *it->second;
    seen.insert(definition.option_key);

    bool type_ok = false;
    if (definition.type == "string") {
      type_ok = value.is_string();
    } else if (definition.type == "integer") {
      type_ok = is_integer_number(value);
    } else if (definition.type == "number") {
      type_ok = value.is_number();
    } else if (definition.type == "boolean") {
      type_ok = value.is_boolean();
    } else if (definition.type == "enum") {
      type_ok = choice_allowed(definition, value);
    } else if (definition.type == "array") {
      type_ok = value.is_array();
    } else if (definition.type == "object") {
      type_ok = value.is_object();
    } else {
      append_issue(result.issues, definition.option_key, "unsupported_type", definition.type);
      continue;
    }

    if (!type_ok) {
      append_issue(result.issues, definition.option_key, "invalid_type", "value does not match option type " + definition.type);
      continue;
    }
    if (definition.type == "integer" || definition.type == "number") {
      const auto numeric = value.get<double>();
      const auto min = number_rule(definition.validation_rules, "range_start");
      const auto max = number_rule(definition.validation_rules, "range_end");
      if (min.has_value() && numeric < *min) {
        append_issue(result.issues, definition.option_key, "below_minimum", "value is below APWorld range_start");
        continue;
      }
      if (max.has_value() && numeric > *max) {
        append_issue(result.issues, definition.option_key, "above_maximum", "value is above APWorld range_end");
        continue;
      }
    }
    result.canonical_values[definition.option_key] = value;
  }

  for (const auto& definition : definitions) {
    if (seen.contains(definition.option_key)) {
      continue;
    }
    if (!definition.default_value.is_null()) {
      result.canonical_values[definition.option_key] = definition.default_value;
      continue;
    }
    if (definition.required) {
      append_issue(result.issues, definition.option_key, "missing_required", "required option is missing");
    }
  }

  result.ok = result.issues.empty();
  return result;
}

nlohmann::json seed_config_sync_manifest(
    std::int64_t user_id,
    const std::vector<SeedConfigSyncEntry>& entries,
    const std::optional<std::string>& cursor) {
  auto payload = nlohmann::json{
      {"format", "sklconf-sync-manifest-v1"},
      {"user_id", user_id},
      {"cursor", cursor.has_value() ? nlohmann::json(*cursor) : nlohmann::json(nullptr)},
      {"entries", nlohmann::json::array()},
  };
  for (const auto& entry : entries) {
    payload["entries"].push_back({
        {"entity_type", entry.entity_type},
        {"entity_id", entry.entity_id},
        {"version", entry.version},
        {"hash", entry.hash},
        {"updated_at", entry.updated_at},
    });
  }
  return payload;
}

std::string export_seed_config_yaml(
    const std::vector<SeedConfigOptionDefinition>& definitions,
    const nlohmann::json& canonical_values) {
  if (!canonical_values.is_object()) {
    throw std::runtime_error("seed_config_export_requires_object_values");
  }
  std::ostringstream out;
  out << "# Generated by SekaiLink Nexus Configs\n";
  for (const auto& definition : definitions) {
    const auto it = canonical_values.find(definition.option_key);
    if (it == canonical_values.end()) {
      continue;
    }
    out << definition.yaml_key << ": " << yaml_scalar(*it) << "\n";
  }
  return out.str();
}

nlohmann::json seed_config_validation_issues_to_json(const std::vector<SeedConfigValidationIssue>& issues) {
  auto out = nlohmann::json::array();
  for (const auto& issue : issues) {
    out.push_back({
        {"option_key", issue.option_key},
        {"code", issue.code},
        {"detail", issue.detail},
    });
  }
  return out;
}

}  // namespace sekailink_server
