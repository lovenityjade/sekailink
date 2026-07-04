#pragma once

#include "nlohmann/json.hpp"

#include <map>
#include <set>
#include <string>

namespace sekailink_server::generation_internal {

bool placement_allows_item(const nlohmann::json& location, const nlohmann::json& item);
nlohmann::json combine_requires_all(const nlohmann::json& left, const nlohmann::json& right);
nlohmann::json find_preplacement_for_location(const nlohmann::json& preplacements,
                                              const std::string& location_id);
void apply_location_logic_rule(nlohmann::json& location, const nlohmann::json& rule);
nlohmann::json enrich_location_with_generation_rules(const nlohmann::json& location,
                                                     const nlohmann::json& logic_rules,
                                                     const nlohmann::json& placement_rules);
nlohmann::json enrich_item_with_generation_rules(const nlohmann::json& item,
                                                 const nlohmann::json& logic_rules);
nlohmann::json collect_item_grants(const nlohmann::json& item,
                                   int receiving_slot,
                                   std::map<std::string, std::size_t>& progressive_levels_by_slot);
nlohmann::json deterministic_ordered_array(const nlohmann::json& source,
                                           const std::string& rng_seed,
                                           const std::string& seed_id,
                                           const std::string& prefix);
nlohmann::json json_without_index(const nlohmann::json& source, std::size_t remove_index);
bool has_static_placement_matching(const nlohmann::json& checks, const nlohmann::json& items);
bool all_items_are_non_advancement(const nlohmann::json& items);
bool should_run_exact_static_matching_guard(const nlohmann::json& remaining_checks,
                                            const nlohmann::json& remaining_items);
nlohmann::json placement_failure_diagnostics(
    const nlohmann::json& remaining_checks,
    const nlohmann::json& remaining_items,
    const std::map<int, std::set<std::string>>& facts_by_slot);

}  // namespace sekailink_server::generation_internal
