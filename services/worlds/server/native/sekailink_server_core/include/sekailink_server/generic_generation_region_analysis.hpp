#pragma once

#include "nlohmann/json.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace sekailink_server::generation_internal {

std::set<std::string> declared_region_ids(const nlohmann::json& region_graph);
std::vector<std::string> invalid_region_graph_reasons(const nlohmann::json& region_graph);
std::string region_reachability_fact(const std::string& region_id);
nlohmann::json find_region_binding_for_location(const nlohmann::json& region_graph,
                                                const std::string& location_id);
void derive_region_reachability_facts(const nlohmann::json& region_graph,
                                      std::set<std::string>& facts);
void derive_region_reachability_facts_from_locations(
    const nlohmann::json& locations,
    std::map<int, std::set<std::string>>& facts_by_slot);

}  // namespace sekailink_server::generation_internal
