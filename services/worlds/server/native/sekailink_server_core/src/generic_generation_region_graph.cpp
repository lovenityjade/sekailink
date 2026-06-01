#include "sekailink_server/generic_generation_region_graph.hpp"

#include "sekailink_server/generic_generation_internal.hpp"
#include "sekailink_server/generic_generation_logic_rules.hpp"
#include "sekailink_server/generic_generation_surface_contracts.hpp"

#include <algorithm>
#include <set>

namespace sekailink_server::generation_internal {

bool region_graph_edge_has_placeholder(const nlohmann::json& edge) {
  return edge.is_object() &&
         json_contains_string_fragment(edge.value("requires", nlohmann::json::object()),
                                       "traversal_declared_not_consumed");
}

std::string region_graph_edge_id(const nlohmann::json& edge) {
  if (!edge.is_object()) {
    return {};
  }
  return first_non_empty({
      json_field_as_string(edge, "id"),
      json_field_as_string(edge, "edge_id"),
      json_field_as_string(edge, "contract_id"),
  });
}

std::string region_graph_edge_from(const nlohmann::json& edge) {
  return first_non_empty({
      json_field_as_string(edge, "from_region"),
      json_field_as_string(edge, "from"),
  });
}

std::string region_graph_edge_to(const nlohmann::json& edge) {
  return first_non_empty({
      json_field_as_string(edge, "to_region"),
      json_field_as_string(edge, "to"),
  });
}

bool region_graph_edges_match(const nlohmann::json& declared_edge, const nlohmann::json& consumed_edge) {
  if (!declared_edge.is_object() || !consumed_edge.is_object()) {
    return false;
  }
  const auto declared_id = region_graph_edge_id(declared_edge);
  const auto consumed_id = region_graph_edge_id(consumed_edge);
  if (!declared_id.empty() && declared_id == consumed_id) {
    return true;
  }
  const auto declared_from = region_graph_edge_from(declared_edge);
  const auto declared_to = region_graph_edge_to(declared_edge);
  return !declared_from.empty() && !declared_to.empty() &&
         declared_from == region_graph_edge_from(consumed_edge) &&
         declared_to == region_graph_edge_to(consumed_edge);
}

bool authorizes_consumed_region_graph_edge(const nlohmann::json& edge) {
  if (!edge.is_object() || region_graph_edge_has_placeholder(edge)) {
    return false;
  }
  const auto status = edge.value("status", std::string{});
  const auto declared_consumed = string_contains(status, "consumed") && !has_not_consumed_candidate_surface(edge);
  const auto proof = native_proof_object(edge);
  if (native_proof_required(edge)) {
    std::set<std::string> required_facts;
    collect_fact_references(edge.value("requires", nlohmann::json{{"op", "true"}}), required_facts);
    const auto produced_facts = proof_fact_set(proof, "produced_facts");
    const auto consumed_facts = proof_fact_set(proof, "consumed_facts");
    const auto proof_complete =
        native_proof_consumed(proof) &&
        missing_fact_membership_count(required_facts, produced_facts) == 0 &&
        missing_fact_membership_count(required_facts, consumed_facts) == 0;
    const auto authorizes_logic =
        explicitly_authorizes_native_logic_solve(edge) || native_proof_authorizes_logic(proof);
    return declared_consumed && authorizes_logic && proof_complete;
  }
  return declared_consumed && explicitly_authorizes_native_logic_solve(edge);
}

namespace {

nlohmann::json append_region_edge_collection(nlohmann::json edges, const nlohmann::json& value) {
  if (value.is_array()) {
    for (const auto& edge : value) {
      if (edge.is_object()) {
        edges.push_back(edge);
      }
    }
  } else if (value.is_object()) {
    for (const auto& [unused, edge] : value.items()) {
      if (edge.is_object()) {
        edges.push_back(edge);
      }
    }
  }
  return edges;
}

}  // namespace

nlohmann::json consumed_region_graph_edges(const nlohmann::json& region_graph) {
  nlohmann::json consumed_edges = nlohmann::json::array();
  if (!region_graph.is_object()) {
    return consumed_edges;
  }
  for (const auto& key : {"consumed_edges", "resolved_edges", "consumed_traversal_edges"}) {
    if (region_graph.contains(key)) {
      consumed_edges = append_region_edge_collection(consumed_edges, region_graph.at(key));
    }
  }
  const auto edge_audit = region_graph.value("edge_audit", nlohmann::json::object());
  if (edge_audit.is_object()) {
    for (const auto& key : {"consumed_edges", "resolved_edges", "consumed_traversal_edges"}) {
      if (edge_audit.contains(key)) {
        consumed_edges = append_region_edge_collection(consumed_edges, edge_audit.at(key));
      }
    }
  }
  return consumed_edges;
}

bool has_authorized_consumed_region_graph_edges(const nlohmann::json& region_graph) {
  const auto edges = consumed_region_graph_edges(region_graph);
  return edges.is_array() &&
         std::any_of(edges.begin(), edges.end(), [](const auto& edge) {
           return authorizes_consumed_region_graph_edge(edge);
         });
}

nlohmann::json consumed_edge_for_placeholder(const nlohmann::json& region_graph,
                                             const nlohmann::json& placeholder_edge) {
  for (const auto& consumed_edge : consumed_region_graph_edges(region_graph)) {
    if (!region_graph_edges_match(placeholder_edge, consumed_edge) ||
        !authorizes_consumed_region_graph_edge(consumed_edge)) {
      continue;
    }
    auto replacement = consumed_edge;
    if (!replacement.contains("from_region") && !replacement.contains("from")) {
      replacement["from_region"] = region_graph_edge_from(placeholder_edge);
    }
    if (!replacement.contains("to_region") && !replacement.contains("to")) {
      replacement["to_region"] = region_graph_edge_to(placeholder_edge);
    }
    if (!replacement.contains("id") && !replacement.contains("edge_id") &&
        !region_graph_edge_id(placeholder_edge).empty()) {
      replacement["edge_id"] = region_graph_edge_id(placeholder_edge);
    }
    return replacement;
  }
  return nlohmann::json::object();
}

nlohmann::json effective_region_graph_edges(const nlohmann::json& region_graph) {
  nlohmann::json edges = nlohmann::json::array();
  if (!region_graph.is_object()) {
    return edges;
  }
  const auto declared_edges = region_graph.value("edges", nlohmann::json::array());
  if (!declared_edges.is_array()) {
    return declared_edges;
  }
  for (const auto& edge : declared_edges) {
    if (region_graph_edge_has_placeholder(edge)) {
      const auto replacement = consumed_edge_for_placeholder(region_graph, edge);
      edges.push_back(replacement.empty() ? edge : replacement);
    } else {
      edges.push_back(edge);
    }
  }
  return edges;
}

nlohmann::json effective_region_graph_object(const nlohmann::json& region_graph) {
  if (!region_graph.is_object()) {
    return nlohmann::json::object();
  }
  auto effective = region_graph;
  effective["edges"] = effective_region_graph_edges(region_graph);
  return effective;
}

bool has_region_graph_placeholder_edges(const nlohmann::json& region_graph) {
  if (!region_graph.is_object()) {
    return false;
  }
  const auto edges = effective_region_graph_edges(region_graph);
  if (!edges.is_array()) {
    return false;
  }
  return std::any_of(edges.begin(), edges.end(), [&](const auto& edge) {
    return region_graph_edge_has_placeholder(edge) &&
           consumed_edge_for_placeholder(region_graph, edge).empty();
  });
}

}  // namespace sekailink_server::generation_internal
