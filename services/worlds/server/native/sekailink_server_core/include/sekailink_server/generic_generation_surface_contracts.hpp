#pragma once

#include "nlohmann/json.hpp"

#include <set>
#include <string>

namespace sekailink_server::generation_internal {

bool has_not_consumed_candidate_surface(const nlohmann::json& value);
bool has_non_authorizing_status(const nlohmann::json& value);
bool authorizes_native_logic_solve(const nlohmann::json& value);
bool explicitly_authorizes_native_logic_solve(const nlohmann::json& value);
bool explicitly_authorizes_native_placement(const nlohmann::json& value);
bool authorizes_native_placement(const nlohmann::json& value);
bool declares_consumed_generation_surface(const nlohmann::json& value);
bool explicitly_consumed_generation_surface(const nlohmann::json& value);
bool explicitly_authorizes_native_placement_surface(const nlohmann::json& value);
bool clears_native_placement_audit_block(const nlohmann::json& value);
nlohmann::json native_proof_object(const nlohmann::json& value);
bool native_proof_required(const nlohmann::json& value);
bool native_proof_consumed(const nlohmann::json& proof);
bool native_proof_authorizes_logic(const nlohmann::json& proof);
bool native_proof_authorizes_placement(const nlohmann::json& proof);
std::set<std::string> proof_fact_set(const nlohmann::json& proof, const std::string& key);
std::size_t missing_fact_membership_count(const std::set<std::string>& required,
                                          const std::set<std::string>& available);
std::set<std::string> facts_referenced_by_rules(const nlohmann::json& rules);
std::set<std::string> facts_referenced_by_location_surface(const nlohmann::json& surface);
bool blocks_native_logic_contract(const nlohmann::json& value);
bool blocks_native_placement_contract(const nlohmann::json& value);
bool authorizes_native_dungeon_key_policy(const nlohmann::json& value);
bool explicitly_authorizes_native_dungeon_key_policy_surface(const nlohmann::json& value);
bool blocks_native_dungeon_key_policy_contract(const nlohmann::json& value);
bool is_explicitly_consumable_logic_surface(const nlohmann::json& value);

}  // namespace sekailink_server::generation_internal
