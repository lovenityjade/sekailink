#include "sekailink_server/generic_generation_region_analysis.hpp"

#include "sekailink_server/generic_generation_internal.hpp"
#include "sekailink_server/generic_generation_logic_rules.hpp"
#include "sekailink_server/generic_generation_region_graph.hpp"

namespace sekailink_server::generation_internal {

namespace {

void collect_region_id_declarations(const nlohmann::json& values, std::set<std::string>& region_ids) {
  if (!values.is_array()) {
    return;
  }
  for (const auto& value : values) {
    const auto region_id = value.is_object()
                               ? first_non_empty({
                                     json_field_as_string(value, "id"),
                                     json_field_as_string(value, "region_id"),
                                     json_field_as_string(value, "region"),
                                 })
                               : string_from_number_or_string(value);
    if (!region_id.empty()) {
      region_ids.insert(region_id);
    }
  }
}

}  // namespace

std::set<std::string> declared_region_ids(const nlohmann::json& region_graph) {
  std::set<std::string> region_ids;
  collect_region_id_declarations(region_graph.value("regions", nlohmann::json::array()), region_ids);
  collect_region_id_declarations(region_graph.value("nodes", nlohmann::json::array()), region_ids);
  collect_region_id_declarations(region_graph.value("region_ids", nlohmann::json::array()), region_ids);
  return region_ids;
}

std::vector<std::string> invalid_region_graph_reasons(const nlohmann::json& region_graph) {
  std::vector<std::string> reasons;
  if (!region_graph.is_object() || region_graph.empty()) {
    return reasons;
  }

  const auto starting_regions = json_string_array(region_graph.value("starting_regions", nlohmann::json::array()));
  if (starting_regions.empty()) {
    push_unique(reasons, "region_graph_missing_starting_regions");
  }

  const auto region_ids = declared_region_ids(region_graph);
  if (region_ids.empty()) {
    push_unique(reasons, "region_graph_missing_declared_regions");
  }
  for (const auto& starting_region : starting_regions) {
    if (!region_ids.empty() && region_ids.count(starting_region) == 0) {
      push_unique(reasons, "region_graph_unknown_starting_region");
    }
  }

  const auto edges = effective_region_graph_edges(region_graph);
  if (!edges.is_array()) {
    push_unique(reasons, "region_graph_invalid_edges");
  } else {
    for (const auto& edge : edges) {
      if (!edge.is_object()) {
        push_unique(reasons, "region_graph_invalid_edges");
        continue;
      }
      const auto from_region = first_non_empty({
          json_field_as_string(edge, "from_region"),
          json_field_as_string(edge, "from"),
      });
      const auto to_region = first_non_empty({
          json_field_as_string(edge, "to_region"),
          json_field_as_string(edge, "to"),
      });
      if (from_region.empty() || to_region.empty()) {
        push_unique(reasons, "region_graph_invalid_edge_refs");
      }
      const auto unknown_edge_region = (!from_region.empty() && region_ids.count(from_region) == 0) ||
                                       (!to_region.empty() && region_ids.count(to_region) == 0);
      if (!region_ids.empty() && unknown_edge_region) {
        push_unique(reasons, "region_graph_unknown_edge_region");
      }
    }
  }

  const auto bindings = region_graph.value("location_region_bindings", nlohmann::json::array());
  if (!bindings.is_array()) {
    push_unique(reasons, "region_graph_invalid_location_region_bindings");
  } else {
    for (const auto& binding : bindings) {
      if (!binding.is_object()) {
        push_unique(reasons, "region_graph_invalid_location_region_bindings");
        continue;
      }
      const auto region_id = first_non_empty({
          json_field_as_string(binding, "region_id"),
          json_field_as_string(binding, "region"),
      });
      if (region_id.empty()) {
        push_unique(reasons, "region_graph_invalid_location_region_binding_refs");
      } else if (!region_ids.empty() && region_ids.count(region_id) == 0) {
        push_unique(reasons, "region_graph_unknown_location_region");
      }
    }
  }

  if (region_graph.contains("edge_audit")) {
    if (!region_graph.at("edge_audit").is_object()) {
      push_unique(reasons, "region_graph_invalid_edge_audit");
    } else {
      const auto& edge_audit = region_graph.at("edge_audit");
      if (edge_audit.contains("edge_blocker_requirements") &&
          !(edge_audit.at("edge_blocker_requirements").is_array() ||
            edge_audit.at("edge_blocker_requirements").is_object())) {
        push_unique(reasons, "region_graph_invalid_edge_blocker_requirements");
      }
      if (edge_audit.contains("missing_generation_contract_surfaces") &&
          !edge_audit.at("missing_generation_contract_surfaces").is_object()) {
        push_unique(reasons, "region_graph_invalid_missing_generation_contract_surfaces");
      }
    }
  }

  if (region_graph.contains("fact_names")) {
    const auto declared_facts = json_string_set(region_graph.at("fact_names"));
    if (!region_graph.at("fact_names").is_array()) {
      push_unique(reasons, "region_graph_invalid_fact_names");
    } else {
      std::set<std::string> referenced_facts;
      const auto starting_state = region_graph.value("starting_state", nlohmann::json::object());
      for (const auto& fact : json_string_array(starting_state.value("facts", nlohmann::json::array()))) {
        referenced_facts.insert(fact);
      }
      if (edges.is_array()) {
        for (const auto& edge : edges) {
          collect_fact_references(edge.value("requires", nlohmann::json{{"op", "true"}}), referenced_facts);
        }
      }
      for (const auto& location : logic_location_rules_array(region_graph)) {
        collect_fact_references(location.value("requires", nlohmann::json{{"op", "true"}}), referenced_facts);
      }
      const auto effects = logic_item_effects_object(region_graph);
      for (const auto& [unused, effect] : effects.items()) {
        collect_grant_references(effect, referenced_facts);
      }
      for (const auto& fact : referenced_facts) {
        if (declared_facts.count(fact) == 0) {
          push_unique(reasons, "region_graph_unknown_fact_name");
          break;
        }
      }
    }
  }
  return reasons;
}

std::string region_reachability_fact(const std::string& region_id) {
  return region_id.empty() ? std::string{} : "predicate.region.can_reach:" + region_id;
}

nlohmann::json find_region_binding_for_location(const nlohmann::json& region_graph,
                                                const std::string& location_id) {
  const auto bindings = region_graph.value("location_region_bindings", nlohmann::json::array());
  if (!bindings.is_array() || location_id.empty()) {
    return nlohmann::json::object();
  }
  for (const auto& binding : bindings) {
    if (!binding.is_object()) {
      continue;
    }
    const auto bound_location = first_non_empty({
        json_field_as_string(binding, "location_id"),
        json_field_as_string(binding, "location"),
        json_field_as_string(binding, "id"),
    });
    if (bound_location == location_id) {
      return binding;
    }
  }
  return nlohmann::json::object();
}

void derive_region_reachability_facts(const nlohmann::json& region_graph,
                                      std::set<std::string>& facts) {
  if (!region_graph.is_object()) {
    return;
  }
  for (const auto& region : json_string_array(region_graph.value("starting_regions", nlohmann::json::array()))) {
    facts.insert(region);
    facts.insert(region_reachability_fact(region));
  }

  const auto edges = effective_region_graph_edges(region_graph);
  if (!edges.is_array()) {
    return;
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& edge : edges) {
      if (!edge.is_object()) {
        continue;
      }
      const auto from_region = first_non_empty({
          json_field_as_string(edge, "from_region"),
          json_field_as_string(edge, "from"),
      });
      const auto to_region = first_non_empty({
          json_field_as_string(edge, "to_region"),
          json_field_as_string(edge, "to"),
      });
      if (from_region.empty() || to_region.empty() || facts.count(region_reachability_fact(from_region)) == 0) {
        continue;
      }
      if (!evaluate_logic_expr(edge.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
        continue;
      }
      const auto inserted_region = facts.insert(to_region).second;
      const auto inserted_fact = facts.insert(region_reachability_fact(to_region)).second;
      changed = changed || inserted_region || inserted_fact;
    }
  }
}

void derive_region_reachability_facts_from_locations(
    const nlohmann::json& locations,
    std::map<int, std::set<std::string>>& facts_by_slot) {
  if (!locations.is_array()) {
    return;
  }
  for (const auto& location : locations) {
    const auto slot_id = location.value("slot_id", 0);
    derive_region_reachability_facts(location.value("region_graph", nlohmann::json::object()),
                                     facts_by_slot[slot_id]);
  }
}

}  // namespace sekailink_server::generation_internal
