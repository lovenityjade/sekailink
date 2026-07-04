#pragma once

#include "nlohmann/json.hpp"

#include <set>
#include <string>
#include <vector>

namespace sekailink_server::generation_internal {

void push_unique(std::vector<std::string>& values, const std::string& value);
nlohmann::json merge_grants(const nlohmann::json& left, const nlohmann::json& right);
bool evaluate_logic_expr(const nlohmann::json& expression, const std::set<std::string>& facts);
std::vector<std::string> starting_facts_from_logic_rules(const nlohmann::json& logic_rules);
nlohmann::json find_rule_for_id(const nlohmann::json& rules, const std::string& id);
nlohmann::json find_rules_for_id(const nlohmann::json& rules, const std::string& id);
std::string location_rule_id(const nlohmann::json& rule);
nlohmann::json logic_location_rules_array(const nlohmann::json& logic_rules);
nlohmann::json rules_from_location_rule_collection(const nlohmann::json& value);
nlohmann::json logic_location_refinement_rules_array(const nlohmann::json& logic_rules);
nlohmann::json logic_segmented_location_rules_array(const nlohmann::json& logic_rules);
nlohmann::json logic_item_effects_object(const nlohmann::json& logic_rules);
nlohmann::json logic_region_graph_object(const nlohmann::json& logic_rules);
void collect_fact_references(const nlohmann::json& expression, std::set<std::string>& facts);
void collect_grant_references(const nlohmann::json& effect, std::set<std::string>& facts);

}  // namespace sekailink_server::generation_internal
