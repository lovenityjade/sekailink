#include "sekailink_server/generic_generation_surface_contracts.hpp"

#include "sekailink_server/generic_generation_internal.hpp"
#include "sekailink_server/generic_generation_logic_rules.hpp"

#include <algorithm>

namespace sekailink_server::generation_internal {

bool has_not_consumed_candidate_surface(const nlohmann::json& value) {
  if (!value.is_object()) {
    return false;
  }
  return json_object_status_contains(value, "not-consumed") ||
         json_object_status_contains(value, "candidate") ||
         json_array_contains_string(value.value("flags", nlohmann::json::array()), "candidate-not-consumed");
}

bool has_non_authorizing_status(const nlohmann::json& value) {
  return json_object_status_contains(value, "not-authorizing");
}

bool authorizes_native_logic_solve(const nlohmann::json& value) {
  if (has_non_authorizing_status(value)) {
    return false;
  }
  if (const auto explicit_authorizes =
          first_object_truthy_flag(value,
                                   {"authorizes_native_logic_solve",
                                    "authorizes_native_location_reachability",
                                    "authorizes_native_logic",
                                    "authorizes_native_reachability"})) {
    return *explicit_authorizes;
  }
  if (!value.is_object() || !value.contains("authorizes") || !value.at("authorizes").is_object()) {
    return true;
  }
  const auto& authorizes = value.at("authorizes");
  if (authorizes.contains("native_logic_solve")) {
    return truthy_json_flag(authorizes.at("native_logic_solve"));
  }
  if (authorizes.contains("native_location_reachability")) {
    return truthy_json_flag(authorizes.at("native_location_reachability"));
  }
  return true;
}

bool explicitly_authorizes_native_logic_solve(const nlohmann::json& value) {
  if (const auto explicit_authorizes =
          first_object_truthy_flag(value,
                                   {"authorizes_native_logic_solve",
                                    "authorizes_native_location_reachability",
                                    "authorizes_native_logic",
                                    "authorizes_native_reachability"})) {
    return *explicit_authorizes;
  }
  if (!value.is_object() || !value.contains("authorizes") || !value.at("authorizes").is_object()) {
    return false;
  }
  const auto& authorizes = value.at("authorizes");
  if (authorizes.contains("native_logic_solve")) {
    return truthy_json_flag(authorizes.at("native_logic_solve"));
  }
  if (authorizes.contains("native_location_reachability")) {
    return truthy_json_flag(authorizes.at("native_location_reachability"));
  }
  return false;
}

bool explicitly_authorizes_native_placement(const nlohmann::json& value) {
  if (const auto explicit_authorizes =
          first_object_truthy_flag(value, {"authorizes_native_placement", "authorizes_native_place_items"})) {
    return *explicit_authorizes;
  }
  if (!value.is_object() || !value.contains("authorizes") || !value.at("authorizes").is_object()) {
    return false;
  }
  const auto& authorizes = value.at("authorizes");
  return authorizes.contains("native_placement") && truthy_json_flag(authorizes.at("native_placement"));
}

bool authorizes_native_placement(const nlohmann::json& value) {
  if (has_non_authorizing_status(value)) {
    return false;
  }
  if (const auto explicit_authorizes =
          first_object_truthy_flag(value, {"authorizes_native_placement", "authorizes_native_place_items"})) {
    return *explicit_authorizes;
  }
  if (!value.is_object() || !value.contains("authorizes") || !value.at("authorizes").is_object()) {
    return true;
  }
  const auto& authorizes = value.at("authorizes");
  if (authorizes.contains("native_placement")) {
    return truthy_json_flag(authorizes.at("native_placement"));
  }
  return true;
}

bool declares_consumed_generation_surface(const nlohmann::json& value) {
  if (!value.is_object() || has_not_consumed_candidate_surface(value)) {
    return false;
  }
  if (object_flag_is_true(value, "consumed")) {
    return true;
  }
  const auto status = value.value("status", std::string{});
  return string_contains(status, "consumed") || string_contains(status, "consumable");
}

bool explicitly_consumed_generation_surface(const nlohmann::json& value) {
  if (!value.is_object() || has_not_consumed_candidate_surface(value)) {
    return false;
  }
  if (object_flag_is_true(value, "consumed")) {
    return true;
  }
  return string_contains(value.value("status", std::string{}), "consumed");
}

bool explicitly_authorizes_native_placement_surface(const nlohmann::json& value) {
  if (!value.is_object()) {
    return false;
  }
  if (value.contains("authorizes_native_placement")) {
    return truthy_json_flag(value.at("authorizes_native_placement"));
  }
  return explicitly_authorizes_native_placement(value);
}

bool clears_native_placement_audit_block(const nlohmann::json& value) {
  return declares_consumed_generation_surface(value) && explicitly_authorizes_native_placement(value);
}

nlohmann::json native_proof_object(const nlohmann::json& value) {
  if (!value.is_object()) {
    return nlohmann::json::object();
  }
  if (value.contains("native_proof") && value.at("native_proof").is_object()) {
    return value.at("native_proof");
  }
  if (value.contains("proof") && value.at("proof").is_object()) {
    return value.at("proof");
  }
  return nlohmann::json::object();
}

bool native_proof_required(const nlohmann::json& value) {
  return value.is_object() &&
         (object_flag_is_true(value, "proof_required") || !native_proof_object(value).empty());
}

bool native_proof_consumed(const nlohmann::json& proof) {
  if (!proof.is_object() || has_not_consumed_candidate_surface(proof)) {
    return false;
  }
  return object_flag_is_true(proof, "consumed") ||
         string_contains(proof.value("status", std::string{}), "consumed") ||
         string_contains(proof.value("status", std::string{}), "complete");
}

bool native_proof_authorizes_logic(const nlohmann::json& proof) {
  return explicitly_authorizes_native_logic_solve(proof) ||
         explicitly_authorizes_native_logic_solve(proof.value("authorization", nlohmann::json::object()));
}

bool native_proof_authorizes_placement(const nlohmann::json& proof) {
  return explicitly_authorizes_native_placement(proof) ||
         explicitly_authorizes_native_placement(proof.value("authorization", nlohmann::json::object()));
}

std::set<std::string> proof_fact_set(const nlohmann::json& proof, const std::string& key) {
  std::set<std::string> facts;
  for (const auto& fact : json_string_array(proof.value(key, nlohmann::json::array()))) {
    facts.insert(fact);
  }
  return facts;
}

std::size_t missing_fact_membership_count(const std::set<std::string>& required,
                                          const std::set<std::string>& available) {
  return static_cast<std::size_t>(std::count_if(required.begin(), required.end(), [&](const auto& fact) {
    return available.count(fact) == 0;
  }));
}

std::set<std::string> facts_referenced_by_rules(const nlohmann::json& rules) {
  std::set<std::string> facts;
  if (!rules.is_array()) {
    return facts;
  }
  for (const auto& rule : rules) {
    collect_fact_references(rule.value("requires", nlohmann::json{{"op", "true"}}), facts);
  }
  return facts;
}

std::set<std::string> facts_referenced_by_location_surface(const nlohmann::json& surface) {
  auto facts = facts_referenced_by_rules(rules_from_location_rule_collection(surface));
  const auto segments = surface.is_object()
      ? surface.value("segments", nlohmann::json::array())
      : nlohmann::json::array();
  if (segments.is_array()) {
    for (const auto& segment : segments) {
      collect_fact_references(segment.value("requires", nlohmann::json{{"op", "true"}}), facts);
    }
  }
  return facts;
}

bool blocks_native_logic_contract(const nlohmann::json& value) {
  if (value.is_array()) {
    return std::any_of(value.begin(), value.end(), [](const auto& child) {
      return blocks_native_logic_contract(child);
    });
  }
  if (!value.is_object()) {
    return false;
  }
  if (has_not_consumed_candidate_surface(value) || !authorizes_native_logic_solve(value)) {
    return true;
  }
  for (auto it = value.begin(); it != value.end(); ++it) {
    const auto key = it.key();
    if (key == "authorizes" || key == "requires" || key == "proof" || key == "native_proof" ||
        key == "authorization") {
      continue;
    }
    if (blocks_native_logic_contract(*it)) {
      return true;
    }
  }
  return false;
}

bool blocks_native_placement_contract(const nlohmann::json& value) {
  if (value.is_array()) {
    return std::any_of(value.begin(), value.end(), [](const auto& child) {
      return blocks_native_placement_contract(child);
    });
  }
  if (!value.is_object()) {
    return false;
  }
  if (has_not_consumed_candidate_surface(value) || !authorizes_native_placement(value)) {
    return true;
  }
  for (auto it = value.begin(); it != value.end(); ++it) {
    const auto key = it.key();
    if (key == "authorizes" || key == "requires" || key == "proof" || key == "native_proof" ||
        key == "authorization") {
      continue;
    }
    if (blocks_native_placement_contract(*it)) {
      return true;
    }
  }
  return false;
}

bool authorizes_native_dungeon_key_policy(const nlohmann::json& value) {
  if (!authorizes_native_placement(value)) {
    return false;
  }
  if (!value.is_object() || !value.contains("authorizes") || !value.at("authorizes").is_object()) {
    return true;
  }
  const auto& authorizes = value.at("authorizes");
  for (const auto& key : {"native_item_pool", "native_dungeon_key_policy", "native_key_policy"}) {
    if (authorizes.contains(key) && !truthy_json_flag(authorizes.at(key))) {
      return false;
    }
  }
  return true;
}

bool explicitly_authorizes_native_dungeon_key_policy_surface(const nlohmann::json& value) {
  const auto top_level_item_pool =
      first_object_truthy_flag(value, {"authorizes_native_item_pool", "authorizes_native_pool"});
  const auto top_level_key_policy =
      first_object_truthy_flag(value, {"authorizes_native_dungeon_key_policy",
                                       "authorizes_native_key_policy"});
  if (top_level_item_pool && top_level_key_policy) {
    return explicitly_authorizes_native_placement(value) && *top_level_item_pool && *top_level_key_policy;
  }
  if (!value.is_object() || !value.contains("authorizes") || !value.at("authorizes").is_object()) {
    return false;
  }
  const auto& authorizes = value.at("authorizes");
  const auto key_policy_authorizes =
      (authorizes.contains("native_dungeon_key_policy") &&
       truthy_json_flag(authorizes.at("native_dungeon_key_policy"))) ||
      (authorizes.contains("native_key_policy") &&
       truthy_json_flag(authorizes.at("native_key_policy")));
  return explicitly_authorizes_native_placement(value) &&
         authorizes.contains("native_item_pool") &&
         truthy_json_flag(authorizes.at("native_item_pool")) &&
         key_policy_authorizes;
}

bool blocks_native_dungeon_key_policy_contract(const nlohmann::json& value) {
  if (value.is_array()) {
    return std::any_of(value.begin(), value.end(), [](const auto& child) {
      return blocks_native_dungeon_key_policy_contract(child);
    });
  }
  if (!value.is_object()) {
    return false;
  }
  if (has_not_consumed_candidate_surface(value) || !authorizes_native_dungeon_key_policy(value)) {
    return true;
  }
  for (auto it = value.begin(); it != value.end(); ++it) {
    const auto key = it.key();
    if (key == "authorizes" || key == "requires" || key == "proof" || key == "native_proof" ||
        key == "authorization") {
      continue;
    }
    if (blocks_native_dungeon_key_policy_contract(*it)) {
      return true;
    }
  }
  return false;
}

bool is_explicitly_consumable_logic_surface(const nlohmann::json& value) {
  if (!value.is_object() || has_not_consumed_candidate_surface(value) || has_non_authorizing_status(value)) {
    return false;
  }
  const auto status = value.value("status", std::string{});
  const auto proof = native_proof_object(value);
  const auto authorizes = explicitly_authorizes_native_logic_solve(value) ||
                          (native_proof_consumed(proof) && native_proof_authorizes_logic(proof));
  return (string_contains(status, "consumable") || string_contains(status, "consumed")) && authorizes;
}

}  // namespace sekailink_server::generation_internal
