#include "sekailink_server/generic_generation.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

bool truthy_json_flag(const nlohmann::json& value) {
  if (value.is_boolean()) {
    return value.get<bool>();
  }
  if (value.is_string()) {
    const auto text = value.get<std::string>();
    return text == "true" || text == "yes" || text == "1";
  }
  return false;
}

std::size_t json_array_size(const nlohmann::json& value) {
  return value.is_array() ? value.size() : 0;
}

std::size_t json_collection_size(const nlohmann::json& value) {
  if (value.is_array() || value.is_object()) {
    return value.size();
  }
  return 0;
}

std::size_t contract_requirement_count(const nlohmann::json& value) {
  if (value.is_array()) {
    return value.size();
  }
  if (value.is_object() && value.contains("requirements")) {
    return json_collection_size(value.at("requirements"));
  }
  return json_collection_size(value);
}

std::string string_from_number_or_string(const nlohmann::json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<std::int64_t>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<std::uint64_t>());
  }
  return {};
}

std::string first_non_empty(std::initializer_list<std::string> values) {
  for (const auto& value : values) {
    if (!value.empty()) {
      return value;
    }
  }
  return {};
}

nlohmann::json object_child_or(const nlohmann::json& object,
                               const std::string& key,
                               const nlohmann::json& fallback) {
  return object.is_object() ? object.value(key, fallback) : fallback;
}

std::string json_field_as_string(const nlohmann::json& value, const std::string& key) {
  if (!value.is_object() || !value.contains(key)) {
    return {};
  }
  return string_from_number_or_string(value.at(key));
}

std::string object_status(const nlohmann::json& object) {
  return object.is_object() ? object.value("status", std::string{}) : std::string{};
}

std::vector<std::string> json_string_array(const nlohmann::json& value) {
  std::vector<std::string> out;
  if (!value.is_array()) {
    return out;
  }
  for (const auto& item : value) {
    const auto text = string_from_number_or_string(item);
    if (!text.empty()) {
      out.push_back(text);
    }
  }
  return out;
}

std::set<std::string> json_string_set(const nlohmann::json& value) {
  const auto strings = json_string_array(value);
  return {strings.begin(), strings.end()};
}

nlohmann::json nested_collection(const nlohmann::json& object,
                                 const std::string& key,
                                 const std::string& nested_key,
                                 const nlohmann::json& fallback) {
  const auto value = object_child_or(object, key, fallback);
  if (value.is_object() && value.contains(nested_key)) {
    return value.at(nested_key);
  }
  return value;
}

std::size_t location_rule_collection_size(const nlohmann::json& value) {
  if (value.is_array()) {
    return value.size();
  }
  if (!value.is_object()) {
    return 0;
  }
  for (const auto& key : {"rules", "refinements", "locations", "per_location",
                          "by_location", "rules_by_location", "location_rules"}) {
    if (!value.contains(key)) {
      continue;
    }
    const auto& child = value.at(key);
    if (child.is_array()) {
      return child.size();
    }
    if (child.is_object()) {
      std::size_t nested_count = 0;
      for (const auto& [unused, nested] : child.items()) {
        if (nested.is_array()) {
          nested_count += nested.size();
        } else if (nested.is_object()) {
          nested_count += nested.contains("requires") ? 1 : location_rule_collection_size(nested);
        }
      }
      if (nested_count > 0) {
        return nested_count;
      }
    }
  }
  std::size_t count = 0;
  for (const auto& [key, rule] : value.items()) {
    if (key == "schema_version" || key == "status" || key == "authorizes" ||
        key == "coverage" || key == "segments" || key == "rules" ||
        key == "refinements" || key == "locations" || key == "per_location" ||
        key == "by_location" || key == "rules_by_location" || key == "location_rules") {
      continue;
    }
    if (rule.is_array()) {
      count += rule.size();
    } else if (rule.is_object()) {
      ++count;
    }
  }
  return count;
}

bool object_flag_is_true(const nlohmann::json& object, const std::string& key) {
  return object.is_object() && object.contains(key) && truthy_json_flag(object.at(key));
}

std::optional<bool> object_truthy_flag(const nlohmann::json& object, const std::string& key) {
  if (!object.is_object() || !object.contains(key)) {
    return std::nullopt;
  }
  return truthy_json_flag(object.at(key));
}

std::optional<bool> first_object_truthy_flag(const nlohmann::json& object,
                                             std::initializer_list<const char*> keys) {
  for (const auto* key : keys) {
    if (const auto value = object_truthy_flag(object, key)) {
      return value;
    }
  }
  return std::nullopt;
}

bool json_object_status_contains(const nlohmann::json& value, const std::string& needle) {
  if (!value.is_object()) {
    return false;
  }
  return value.contains("status") && value.at("status").is_string() &&
         value.at("status").get<std::string>().find(needle) != std::string::npos;
}

bool json_array_contains_string(const nlohmann::json& value, const std::string& needle) {
  if (!value.is_array()) {
    return false;
  }
  return std::any_of(value.begin(), value.end(), [&](const auto& child) {
    return child.is_string() && child.template get<std::string>() == needle;
  });
}

bool has_not_consumed_candidate_surface(const nlohmann::json& value) {
  if (!value.is_object()) {
    return false;
  }
  return json_object_status_contains(value, "not-consumed") ||
         json_object_status_contains(value, "candidate") ||
         json_array_contains_string(value.value("flags", nlohmann::json::array()), "candidate-not-consumed");
}

bool authorizes_native_logic_solve(const nlohmann::json& value) {
  if (json_object_status_contains(value, "not-authorizing")) {
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
  if (json_object_status_contains(value, "not-authorizing")) {
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
  return status.find("consumed") != std::string::npos ||
         status.find("consumable") != std::string::npos;
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
         proof.value("status", std::string{}).find("consumed") != std::string::npos ||
         proof.value("status", std::string{}).find("complete") != std::string::npos;
}

bool authorizes_native_dungeon_key_policy(const nlohmann::json& value) {
  if (!authorizes_native_placement(value)) {
    return false;
  }
  for (const auto* key : {"authorizes_native_item_pool", "authorizes_native_pool",
                          "authorizes_native_dungeon_key_policy", "authorizes_native_key_policy"}) {
    if (value.is_object() && value.contains(key) && !truthy_json_flag(value.at(key))) {
      return false;
    }
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

bool authorization_key_satisfied_by_alias(const nlohmann::json& value, const std::string& key) {
  if (key == "native_logic_solve") {
    return explicitly_authorizes_native_logic_solve(value);
  }
  if (key == "native_location_reachability") {
    if (const auto explicit_authorizes =
            first_object_truthy_flag(value,
                                     {"authorizes_native_location_reachability",
                                      "authorizes_native_reachability"})) {
      return *explicit_authorizes;
    }
    return explicitly_authorizes_native_logic_solve(value);
  }
  if (key == "native_placement") {
    return explicitly_authorizes_native_placement(value);
  }
  return false;
}

nlohmann::json missing_authorization_keys(const nlohmann::json& value,
                                          std::initializer_list<const char*> keys) {
  nlohmann::json missing = nlohmann::json::array();
  if (!value.is_object() || value.empty()) {
    return missing;
  }
  if (!value.contains("authorizes") || !value.at("authorizes").is_object()) {
    for (const auto* key : keys) {
      if (!authorization_key_satisfied_by_alias(value, key)) {
        missing.push_back(key);
      }
    }
    return missing;
  }
  const auto& authorizes = value.at("authorizes");
  for (const auto* key : keys) {
    if ((!authorizes.contains(key) || !truthy_json_flag(authorizes.at(key))) &&
        !authorization_key_satisfied_by_alias(value, key)) {
      missing.push_back(key);
    }
  }
  return missing;
}

bool has_explicit_authorization_key(const nlohmann::json& value, const std::string& key) {
  return authorization_key_satisfied_by_alias(value, key) ||
         (value.is_object() && value.contains("authorizes") && value.at("authorizes").is_object() &&
          value.at("authorizes").contains(key));
}

void collect_fact_references(const nlohmann::json& expression, std::set<std::string>& facts) {
  if (expression.is_string()) {
    facts.insert(expression.get<std::string>());
    return;
  }
  if (!expression.is_object()) {
    return;
  }
  const auto op = expression.value("op", expression.value("type", std::string{}));
  if (op == "fact") {
    const auto fact = expression.value("id", std::string{});
    if (!fact.empty()) {
      facts.insert(fact);
    }
    return;
  }
  if (op == "not") {
    collect_fact_references(expression.value("value", expression.value("arg", nlohmann::json{})), facts);
    return;
  }
  const auto args = expression.contains("args") ? expression.at("args") : expression.value("of", nlohmann::json::array());
  if (args.is_array()) {
    for (const auto& child : args) {
      collect_fact_references(child, facts);
    }
  }
  for (const auto& fact : json_string_array(expression.value("facts", nlohmann::json::array()))) {
    facts.insert(fact);
  }
}

std::set<std::string> proof_fact_set(const nlohmann::json& proof, const std::string& key) {
  return json_string_set(proof.value(key, nlohmann::json::array()));
}

std::size_t missing_fact_membership_count(const std::set<std::string>& required,
                                          const std::set<std::string>& available) {
  return static_cast<std::size_t>(std::count_if(required.begin(), required.end(), [&](const auto& fact) {
    return available.count(fact) == 0;
  }));
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

bool json_contains_string_fragment(const nlohmann::json& value, const std::string& needle) {
  if (value.is_string()) {
    return value.get<std::string>().find(needle) != std::string::npos;
  }
  if (value.is_array()) {
    return std::any_of(value.begin(), value.end(), [&](const auto& child) {
      return json_contains_string_fragment(child, needle);
    });
  }
  if (value.is_object()) {
    return std::any_of(value.begin(), value.end(), [&](const auto& child) {
      return json_contains_string_fragment(child, needle);
    });
  }
  return false;
}

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
  const auto declared_consumed = status.find("consumed") != std::string::npos &&
                                 !has_not_consumed_candidate_surface(edge);
  const auto proof = native_proof_object(edge);
  if (native_proof_required(edge)) {
    std::set<std::string> required_facts;
    collect_fact_references(edge.value("requires", nlohmann::json{{"op", "true"}}), required_facts);
    const auto proof_complete =
        native_proof_consumed(proof) &&
        missing_fact_membership_count(required_facts, proof_fact_set(proof, "produced_facts")) == 0 &&
        missing_fact_membership_count(required_facts, proof_fact_set(proof, "consumed_facts")) == 0;
    const auto authorizes_logic =
        explicitly_authorizes_native_logic_solve(edge) || explicitly_authorizes_native_logic_solve(proof);
    return declared_consumed && authorizes_logic && proof_complete;
  }
  return declared_consumed && explicitly_authorizes_native_logic_solve(edge);
}

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

bool placeholder_has_consumed_edge(const nlohmann::json& region_graph, const nlohmann::json& placeholder_edge) {
  for (const auto& consumed_edge : consumed_region_graph_edges(region_graph)) {
    if (region_graph_edges_match(placeholder_edge, consumed_edge) &&
        authorizes_consumed_region_graph_edge(consumed_edge)) {
      return true;
    }
  }
  return false;
}

std::size_t count_region_graph_declared_placeholder_edges(const nlohmann::json& region_graph) {
  const auto edges = object_child_or(region_graph, "edges", nlohmann::json::array());
  if (!edges.is_array()) {
    return 0;
  }
  return static_cast<std::size_t>(std::count_if(edges.begin(), edges.end(), [](const auto& edge) {
    return region_graph_edge_has_placeholder(edge);
  }));
}

std::size_t count_authorized_consumed_region_graph_edges(const nlohmann::json& region_graph) {
  const auto consumed_edges = consumed_region_graph_edges(region_graph);
  return static_cast<std::size_t>(std::count_if(consumed_edges.begin(), consumed_edges.end(), [](const auto& edge) {
    return authorizes_consumed_region_graph_edge(edge);
  }));
}

std::size_t count_region_graph_placeholder_edges(const nlohmann::json& region_graph) {
  const auto edges = object_child_or(region_graph, "edges", nlohmann::json::array());
  if (!edges.is_array()) {
    return 0;
  }
  return static_cast<std::size_t>(std::count_if(edges.begin(), edges.end(), [&](const auto& edge) {
    if (!edge.is_object()) {
      return false;
    }
    return region_graph_edge_has_placeholder(edge) && !placeholder_has_consumed_edge(region_graph, edge);
  }));
}

std::string placeholder_contract_family(const nlohmann::json& edge) {
  const auto explicit_family = first_non_empty({
      json_field_as_string(edge, "contract_family"),
      json_field_as_string(edge, "family"),
      json_field_as_string(edge, "blocker_family"),
  });
  if (!explicit_family.empty()) {
    return explicit_family;
  }
  if (region_graph_edge_has_placeholder(edge)) {
    return "region_traversal";
  }
  return "unknown";
}

nlohmann::json unresolved_placeholder_edge_details(const nlohmann::json& region_graph) {
  nlohmann::json details = nlohmann::json::array();
  const auto edges = object_child_or(region_graph, "edges", nlohmann::json::array());
  if (!edges.is_array()) {
    return details;
  }
  for (const auto& edge : edges) {
    if (!region_graph_edge_has_placeholder(edge) || placeholder_has_consumed_edge(region_graph, edge)) {
      continue;
    }
    details.push_back({
        {"edge_id", region_graph_edge_id(edge)},
        {"from_region", region_graph_edge_from(edge)},
        {"to_region", region_graph_edge_to(edge)},
        {"contract_family", placeholder_contract_family(edge)},
    });
  }
  return details;
}

nlohmann::json authorized_consumed_region_graph_edge_ids(const nlohmann::json& region_graph) {
  nlohmann::json ids = nlohmann::json::array();
  for (const auto& edge : consumed_region_graph_edges(region_graph)) {
    if (!authorizes_consumed_region_graph_edge(edge)) {
      continue;
    }
    auto id = region_graph_edge_id(edge);
    if (id.empty()) {
      id = region_graph_edge_from(edge) + "->" + region_graph_edge_to(edge);
    }
    ids.push_back(id);
  }
  return ids;
}

std::size_t replacement_edge_missing_produced_fact_count(const nlohmann::json& region_graph) {
  std::size_t missing = 0;
  for (const auto& edge : consumed_region_graph_edges(region_graph)) {
    if (!edge.is_object() || !native_proof_required(edge)) {
      continue;
    }
    std::set<std::string> required_facts;
    collect_fact_references(edge.value("requires", nlohmann::json{{"op", "true"}}), required_facts);
    missing += missing_fact_membership_count(required_facts,
                                             proof_fact_set(native_proof_object(edge), "produced_facts"));
  }
  return missing;
}

std::size_t replacement_edge_missing_consumed_fact_count(const nlohmann::json& region_graph) {
  std::size_t missing = 0;
  for (const auto& edge : consumed_region_graph_edges(region_graph)) {
    if (!edge.is_object() || !native_proof_required(edge)) {
      continue;
    }
    std::set<std::string> required_facts;
    collect_fact_references(edge.value("requires", nlohmann::json{{"op", "true"}}), required_facts);
    missing += missing_fact_membership_count(required_facts,
                                             proof_fact_set(native_proof_object(edge), "consumed_facts"));
  }
  return missing;
}

void collect_contract_families(const nlohmann::json& value,
                               const std::string& fallback,
                               std::set<std::string>& families) {
  if (value.is_array()) {
    for (const auto& child : value) {
      collect_contract_families(child, fallback, families);
    }
    return;
  }
  if (!value.is_object()) {
    return;
  }
  const auto family = first_non_empty({
      json_field_as_string(value, "contract_family"),
      json_field_as_string(value, "family"),
      json_field_as_string(value, "blocker_family"),
      json_field_as_string(value, "contract_id"),
      json_field_as_string(value, "blocker_key"),
  });
  if (!family.empty()) {
    families.insert(family);
  } else if (!fallback.empty() && !value.empty()) {
    families.insert(fallback);
  }
  for (const auto& [key, child] : value.items()) {
    if (key == "authorizes" || key == "requires") {
      continue;
    }
    collect_contract_families(child, fallback, families);
  }
}

nlohmann::json contract_families_for_surface(const nlohmann::json& value, const std::string& fallback) {
  std::set<std::string> families;
  collect_contract_families(value, fallback, families);
  nlohmann::json out = nlohmann::json::array();
  for (const auto& family : families) {
    out.push_back(family);
  }
  return out;
}

nlohmann::json rules_from_location_rule_collection(const nlohmann::json& value) {
  if (value.is_array()) {
    return value;
  }
  if (!value.is_object()) {
    return nlohmann::json::array();
  }
  for (const auto& key : {"rules", "refinements", "locations"}) {
    if (value.contains(key) && value.at(key).is_array()) {
      return value.at(key);
    }
  }
  nlohmann::json rules = nlohmann::json::array();
  for (const auto& [location_id, rule] : value.items()) {
    if (location_id == "schema_version" || location_id == "status" || location_id == "authorizes" ||
        location_id == "coverage" || location_id == "segments" || location_id == "rules" ||
        location_id == "refinements" || location_id == "locations" || location_id == "proof" ||
        location_id == "native_proof" || location_id == "proof_required") {
      continue;
    }
    if (rule.is_object()) {
      auto normalized = rule;
      if (!normalized.contains("id") && !normalized.contains("location_id")) {
        normalized["location_id"] = location_id;
      }
      rules.push_back(std::move(normalized));
    }
  }
  return rules;
}

std::size_t location_surface_missing_fact_proof_count(const nlohmann::json& surface) {
  if (!native_proof_required(surface)) {
    return 0;
  }
  const auto proof = native_proof_object(surface);
  std::set<std::string> referenced_facts;
  for (const auto& rule : rules_from_location_rule_collection(surface)) {
    collect_fact_references(rule.value("requires", nlohmann::json{{"op", "true"}}), referenced_facts);
  }
  const auto segments = surface.is_object()
      ? surface.value("segments", nlohmann::json::array())
      : nlohmann::json::array();
  if (segments.is_array()) {
    for (const auto& segment : segments) {
      collect_fact_references(segment.value("requires", nlohmann::json{{"op", "true"}}), referenced_facts);
    }
  }
  return missing_fact_membership_count(referenced_facts, proof_fact_set(proof, "produced_facts")) +
         missing_fact_membership_count(referenced_facts, proof_fact_set(proof, "consumed_facts"));
}

std::size_t segmentation_missing_coverage_count(const nlohmann::json& segmentation) {
  const auto proof = native_proof_object(segmentation);
  const auto coverage = proof.is_object() && proof.contains("coverage")
      ? proof.value("coverage", nlohmann::json::object())
      : object_child_or(segmentation, "coverage", nlohmann::json::object());
  const auto uncovered = object_child_or(coverage, "uncovered_location_ids", nlohmann::json::array());
  if (uncovered.is_array() && !uncovered.empty()) {
    return uncovered.size();
  }
  const auto expected = coverage.value("expected_location_count", 0);
  const auto covered = coverage.value("covered_location_count",
                                      coverage.value("primary_segment_count",
                                                     coverage.value("segmented_location_count", 0)));
  if (expected > 0 && covered < expected) {
    return static_cast<std::size_t>(expected - covered);
  }
  return 0;
}

std::size_t dungeon_key_policy_missing_self_lock_proof_count(const nlohmann::json& binding) {
  if (!native_proof_required(binding)) {
    return 0;
  }
  const auto proof = native_proof_object(binding);
  const auto self_lock = proof.value("self_lock_proof", proof.value("self_lock", nlohmann::json::object()));
  if (self_lock.is_object() && self_lock.contains("missing_count")) {
    return static_cast<std::size_t>(std::max(0, self_lock.value("missing_count", 0)));
  }
  const auto proved_locations = proof_fact_set(self_lock, "proved_location_ids");
  const auto rules = binding.value("blocked_big_chest_rules", binding.value("requirements", nlohmann::json::array()));
  std::set<std::string> required_locations;
  if (rules.is_array()) {
    for (const auto& rule : rules) {
      const auto location_id = first_non_empty({
          json_field_as_string(rule, "location_id"),
          json_field_as_string(rule, "location"),
          json_field_as_string(rule, "id"),
      });
      if (!location_id.empty()) {
        required_locations.insert(location_id);
      }
    }
  }
  return missing_fact_membership_count(required_locations, proved_locations);
}

std::size_t dungeon_key_policy_missing_small_key_proof_count(const nlohmann::json& policy) {
  if (!native_proof_required(policy)) {
    return 0;
  }
  const auto proof = native_proof_object(policy);
  const auto small_key = proof.value("small_key_proof", proof.value("small_keys", nlohmann::json::object()));
  if (small_key.is_object() && small_key.contains("missing_count")) {
    return static_cast<std::size_t>(std::max(0, small_key.value("missing_count", 0)));
  }
  const auto proved_items = proof_fact_set(small_key, "proved_item_ids");
  const auto pool = policy.value("separate_key_pool", nlohmann::json::array());
  std::set<std::string> required_items;
  if (pool.is_array()) {
    for (const auto& item : pool) {
      const auto role = item.value("role", std::string{});
      const auto type = item.value("type", std::string{});
      if (role.find("small_key") != std::string::npos || type.find("small_key") != std::string::npos) {
        const auto item_id = first_non_empty({
            json_field_as_string(item, "id"),
            json_field_as_string(item, "item_id"),
        });
        if (!item_id.empty()) {
          required_items.insert(item_id);
        }
      }
    }
  }
  return missing_fact_membership_count(required_items, proved_items);
}

bool evaluate_logic_expr(const nlohmann::json& expression, const std::set<std::string>& facts) {
  if (expression.is_null()) {
    return true;
  }
  if (expression.is_boolean()) {
    return expression.get<bool>();
  }
  if (expression.is_string()) {
    return facts.count(expression.get<std::string>()) > 0;
  }
  if (!expression.is_object()) {
    return false;
  }
  const auto op = expression.value("op", expression.value("type", std::string{"true"}));
  if (op == "true") {
    return true;
  }
  if (op == "false") {
    return false;
  }
  if (op == "fact") {
    return facts.count(expression.value("id", std::string{})) > 0;
  }
  if (op == "not") {
    return !evaluate_logic_expr(expression.value("value", expression.value("arg", nlohmann::json{})), facts);
  }
  const auto args = expression.contains("args") ? expression.at("args") : expression.value("of", nlohmann::json::array());
  if (op == "all") {
    return args.is_array() && std::all_of(args.begin(), args.end(), [&](const auto& child) {
      return evaluate_logic_expr(child, facts);
    });
  }
  if (op == "any") {
    return args.is_array() && std::any_of(args.begin(), args.end(), [&](const auto& child) {
      return evaluate_logic_expr(child, facts);
    });
  }
  if (op == "count_at_least") {
    const auto needed = expression.value("count", 0);
    auto count = 0;
    for (const auto& fact : json_string_array(expression.value("facts", nlohmann::json::array()))) {
      if (facts.count(fact) > 0) {
        ++count;
      }
    }
    return count >= needed;
  }
  return false;
}

std::vector<std::string> starting_facts_from_logic_rules(const nlohmann::json& logic_rules) {
  if (!logic_rules.is_object()) {
    return {};
  }
  const auto starting_state = logic_rules.value("starting_state", nlohmann::json::object());
  std::vector<std::string> facts = json_string_array(starting_state.value("facts", nlohmann::json::array()));
  for (const auto& [key, value] : starting_state.items()) {
    if (key == "facts") {
      continue;
    }
    const auto text = string_from_number_or_string(value);
    if (!text.empty()) {
      facts.push_back(key + ":" + text);
    }
  }
  return facts;
}

std::string region_reachability_fact(const std::string& region_id) {
  return region_id.empty() ? std::string{} : "predicate.region.can_reach:" + region_id;
}

nlohmann::json effective_region_graph_edges(const nlohmann::json& region_graph) {
  nlohmann::json edges = nlohmann::json::array();
  const auto declared_edges = object_child_or(region_graph, "edges", nlohmann::json::array());
  if (!declared_edges.is_array()) {
    return declared_edges;
  }
  for (const auto& edge : declared_edges) {
    if (!region_graph_edge_has_placeholder(edge)) {
      edges.push_back(edge);
      continue;
    }
    nlohmann::json replacement = nlohmann::json::object();
    for (const auto& consumed_edge : consumed_region_graph_edges(region_graph)) {
      if (region_graph_edges_match(edge, consumed_edge) && authorizes_consumed_region_graph_edge(consumed_edge)) {
        replacement = consumed_edge;
        if (!replacement.contains("from_region") && !replacement.contains("from")) {
          replacement["from_region"] = region_graph_edge_from(edge);
        }
        if (!replacement.contains("to_region") && !replacement.contains("to")) {
          replacement["to_region"] = region_graph_edge_to(edge);
        }
        break;
      }
    }
    edges.push_back(replacement.empty() ? edge : replacement);
  }
  return edges;
}

nlohmann::json effective_region_graph_object(nlohmann::json region_graph) {
  if (region_graph.is_object()) {
    region_graph["edges"] = effective_region_graph_edges(region_graph);
  }
  return region_graph;
}

void derive_region_reachability_facts(const nlohmann::json& region_graph, std::set<std::string>& facts) {
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
      const auto from_region = region_graph_edge_from(edge);
      const auto to_region = region_graph_edge_to(edge);
      if (from_region.empty() || to_region.empty() || facts.count(region_reachability_fact(from_region)) == 0) {
        continue;
      }
      if (!evaluate_logic_expr(edge.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
        continue;
      }
      changed = facts.insert(to_region).second || changed;
      changed = facts.insert(region_reachability_fact(to_region)).second || changed;
    }
  }
}

std::set<std::string> native_proof_execution_facts(const nlohmann::json& logic_rules,
                                                   const nlohmann::json& proof) {
  std::set<std::string> facts;
  for (const auto& fact : starting_facts_from_logic_rules(logic_rules)) {
    facts.insert(fact);
  }
  for (const auto& fact : proof_fact_set(proof, "produced_facts")) {
    facts.insert(fact);
  }
  for (const auto& fact : proof_fact_set(proof, "consumed_facts")) {
    facts.insert(fact);
  }
  return facts;
}

nlohmann::json native_replacement_edge_execution_report(const nlohmann::json& logic_rules,
                                                        const nlohmann::json& region_graph) {
  std::size_t total = 0;
  std::size_t passed = 0;
  for (const auto& edge : consumed_region_graph_edges(region_graph)) {
    if (!edge.is_object() || !native_proof_required(edge)) {
      continue;
    }
    ++total;
    auto executable_edge = edge;
    const auto declared_edges = object_child_or(region_graph, "edges", nlohmann::json::array());
    if (declared_edges.is_array()) {
      for (const auto& declared_edge : declared_edges) {
        if (region_graph_edges_match(declared_edge, edge)) {
          if (!executable_edge.contains("from_region") && !executable_edge.contains("from")) {
            executable_edge["from_region"] = region_graph_edge_from(declared_edge);
          }
          if (!executable_edge.contains("to_region") && !executable_edge.contains("to")) {
            executable_edge["to_region"] = region_graph_edge_to(declared_edge);
          }
          break;
        }
      }
    }
    const auto proof = native_proof_object(executable_edge);
    auto facts = native_proof_execution_facts(logic_rules, proof);
    derive_region_reachability_facts(effective_region_graph_object(region_graph), facts);
    const auto from_region = region_graph_edge_from(executable_edge);
    const auto to_region = region_graph_edge_to(executable_edge);
    const auto ok = authorizes_consumed_region_graph_edge(executable_edge) &&
                    !from_region.empty() && facts.count(region_reachability_fact(from_region)) > 0 &&
                    evaluate_logic_expr(executable_edge.value("requires", nlohmann::json{{"op", "true"}}), facts) &&
                    !to_region.empty() && facts.count(region_reachability_fact(to_region)) > 0;
    if (ok) {
      ++passed;
    }
  }
  const auto failed = total - passed;
  return {
      {"status", total == 0 ? "not_required" : (failed == 0 ? "passed" : "failed")},
      {"required_count", total},
      {"passed_count", passed},
      {"failed_count", failed},
  };
}

nlohmann::json native_location_surface_execution_report(const nlohmann::json& logic_rules,
                                                        const nlohmann::json& region_graph,
                                                        const nlohmann::json& surface) {
  if (!native_proof_required(surface)) {
    return {{"status", "not_required"}, {"required_count", 0}, {"passed_count", 0}, {"failed_count", 0}};
  }
  const auto proof = native_proof_object(surface);
  auto facts = native_proof_execution_facts(logic_rules, proof);
  derive_region_reachability_facts(region_graph, facts);
  std::size_t total = 0;
  std::size_t failed = 0;
  for (const auto& rule : rules_from_location_rule_collection(surface)) {
    ++total;
    if (!evaluate_logic_expr(rule.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
      ++failed;
    }
  }
  const auto segments = object_child_or(surface, "segments", nlohmann::json::array());
  if (segments.is_array()) {
    for (const auto& segment : segments) {
      if (!segment.is_object()) {
        continue;
      }
      const auto locations = segment.contains("location_ids")
          ? segment.at("location_ids")
          : segment.value("locations", nlohmann::json::array());
      const auto segment_count = locations.is_array() && !locations.empty() ? locations.size() : 1;
      total += segment_count;
      if (!evaluate_logic_expr(segment.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
        failed += segment_count;
      }
      for (const auto& rule : rules_from_location_rule_collection(segment)) {
        ++total;
        if (!evaluate_logic_expr(rule.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
          ++failed;
        }
      }
    }
  }
  return {
      {"status", failed == 0 ? "passed" : "failed"},
      {"required_count", total},
      {"passed_count", total >= failed ? total - failed : 0},
      {"failed_count", failed},
  };
}

std::set<std::string> ids_from_json_array(const nlohmann::json& values) {
  std::set<std::string> ids;
  if (!values.is_array()) {
    return ids;
  }
  for (const auto& value : values) {
    const auto id = value.is_object()
        ? first_non_empty({json_field_as_string(value, "id"), json_field_as_string(value, "location_id"),
                           json_field_as_string(value, "location")})
        : string_from_number_or_string(value);
    if (!id.empty()) {
      ids.insert(id);
    }
  }
  return ids;
}

nlohmann::json filter_fillable_locations(const nlohmann::json& locations,
                                         const nlohmann::json& placement_rules) {
  if (!locations.is_array()) {
    return nlohmann::json::array();
  }
  const auto fillable_ids = ids_from_json_array(placement_rules.value("fillable_locations", nlohmann::json::array()));
  const auto reserved_ids = ids_from_json_array(placement_rules.value("reserved_locations", nlohmann::json::array()));
  nlohmann::json out = nlohmann::json::array();
  for (const auto& location : locations) {
    const auto id = first_non_empty({json_field_as_string(location, "id"), json_field_as_string(location, "location_id")});
    if (reserved_ids.count(id) > 0 || (!fillable_ids.empty() && fillable_ids.count(id) == 0)) {
      continue;
    }
    out.push_back(location);
  }
  return out;
}

bool placement_allows_item(const nlohmann::json& location, const nlohmann::json& item) {
  std::set<std::string> item_tags = json_string_set(item.value("tags", nlohmann::json::array()));
  for (const auto& key : {"role", "type", "classification"}) {
    const auto value = item.value(key, std::string{});
    if (!value.empty()) {
      item_tags.insert(value);
    }
  }
  const auto allow_tags = json_string_array(location.value("allow_item_tags", nlohmann::json::array()));
  if (!allow_tags.empty() && std::none_of(allow_tags.begin(), allow_tags.end(), [&](const auto& tag) {
        return item_tags.count(tag) > 0;
      })) {
    return false;
  }
  const auto forbid_tags = json_string_array(location.value("forbid_item_tags", nlohmann::json::array()));
  return std::none_of(forbid_tags.begin(), forbid_tags.end(), [&](const auto& tag) {
    return item_tags.count(tag) > 0;
  });
}

nlohmann::json location_with_placement_rule(nlohmann::json location, const nlohmann::json& placement_rules) {
  const auto id = first_non_empty({json_field_as_string(location, "id"), json_field_as_string(location, "location_id")});
  const auto constraints = placement_rules.value("location_constraints", nlohmann::json::array());
  if (constraints.is_array()) {
    for (const auto& rule : constraints) {
      if (json_field_as_string(rule, "id") == id || json_field_as_string(rule, "location_id") == id) {
        if (rule.contains("allow_item_tags")) {
          location["allow_item_tags"] = rule.at("allow_item_tags");
        }
        if (rule.contains("forbid_item_tags")) {
          location["forbid_item_tags"] = rule.at("forbid_item_tags");
        }
      }
    }
  }
  return location;
}

nlohmann::json native_dungeon_key_policy_execution_report(const nlohmann::json& catalog,
                                                          const nlohmann::json& logic_rules,
                                                          const nlohmann::json& placement_rules,
                                                          const nlohmann::json& policy,
                                                          const nlohmann::json& binding) {
  if (!native_proof_required(policy) && !native_proof_required(binding)) {
    return {{"status", "not_required"}, {"required_count", 0}, {"passed_count", 0}, {"failed_count", 0},
            {"small_key_placement_failed_count", 0}, {"self_lock_failed_count", 0}};
  }
  std::size_t total = 0;
  std::size_t failed = 0;
  std::size_t small_key_failed = 0;
  std::size_t self_lock_failed = 0;
  const auto proved_items =
      proof_fact_set(native_proof_object(policy).value("small_key_proof", nlohmann::json::object()),
                     "proved_item_ids");
  const auto pool = policy.value("separate_key_pool", nlohmann::json::array());
  const auto pool_items = pool.is_object() ? pool.value("items", nlohmann::json::array()) : pool;
  for (const auto& item : pool_items) {
    const auto role = item.value("role", std::string{});
    const auto type = item.value("type", std::string{});
    if (role.find("small_key") == std::string::npos && type.find("small_key") == std::string::npos) {
      continue;
    }
    ++total;
    const auto item_id = first_non_empty({json_field_as_string(item, "id"), json_field_as_string(item, "item_id")});
    bool has_candidate = false;
    for (const auto& location : filter_fillable_locations(catalog.value("locations", nlohmann::json::array()),
                                                          placement_rules)) {
      if (placement_allows_item(location_with_placement_rule(location, placement_rules), item)) {
        has_candidate = true;
        break;
      }
    }
    if (item_id.empty() || proved_items.count(item_id) == 0 || !has_candidate) {
      ++failed;
      ++small_key_failed;
    }
  }
  const auto proved_locations =
      proof_fact_set(native_proof_object(binding).value("self_lock_proof", nlohmann::json::object()),
                     "proved_location_ids");
  const auto location_ids = ids_from_json_array(catalog.value("locations", nlohmann::json::array()));
  const auto rules = binding.value("blocked_big_chest_rules", binding.value("requirements", nlohmann::json::array()));
  if (rules.is_array()) {
    for (const auto& rule : rules) {
      ++total;
      const auto location_id =
          first_non_empty({json_field_as_string(rule, "location_id"), json_field_as_string(rule, "location"),
                           json_field_as_string(rule, "id")});
      if (location_id.empty() || proved_locations.count(location_id) == 0 ||
          (!location_ids.empty() && location_ids.count(location_id) == 0)) {
        ++failed;
        ++self_lock_failed;
      }
    }
  }
  return {{"status", failed == 0 ? "passed" : "failed"},
          {"required_count", total},
          {"passed_count", total >= failed ? total - failed : 0},
          {"failed_count", failed},
          {"small_key_placement_failed_count", small_key_failed},
          {"self_lock_failed_count", self_lock_failed}};
}

std::set<std::string> native_probe_contract_facts(const nlohmann::json& logic_rules,
                                                  const nlohmann::json& contract) {
  std::set<std::string> facts;
  for (const auto& fact : starting_facts_from_logic_rules(logic_rules)) {
    facts.insert(fact);
  }
  for (const auto& key : {"facts", "produced_facts", "consumed_facts"}) {
    for (const auto& fact : json_string_array(contract.value(key, nlohmann::json::array()))) {
      facts.insert(fact);
    }
  }
  const auto proof = native_proof_object(contract);
  for (const auto& fact : proof_fact_set(proof, "produced_facts")) {
    facts.insert(fact);
  }
  for (const auto& fact : proof_fact_set(proof, "consumed_facts")) {
    facts.insert(fact);
  }
  return facts;
}

nlohmann::json combine_requires_all(const nlohmann::json& left, const nlohmann::json& right) {
  if (left.is_null()) {
    return right;
  }
  if (right.is_null()) {
    return left;
  }
  if (left.is_object() && left.value("op", left.value("type", std::string{})) == "true") {
    return right;
  }
  if (right.is_object() && right.value("op", right.value("type", std::string{})) == "true") {
    return left;
  }
  return {{"op", "all"}, {"args", nlohmann::json::array({left, right})}};
}

void apply_probe_location_rule(nlohmann::json& check, const nlohmann::json& rule) {
  if (!rule.is_object() || !rule.contains("requires")) {
    return;
  }
  check["requires"] = combine_requires_all(check.value("requires", nlohmann::json{{"op", "true"}}),
                                           rule.at("requires"));
}

void apply_probe_location_rules(nlohmann::json& check, const nlohmann::json& rules) {
  const auto location_id = first_non_empty({json_field_as_string(check, "id"),
                                            json_field_as_string(check, "location_id")});
  if (!rules.is_array()) {
    return;
  }
  for (const auto& rule : rules) {
    const auto rule_id = first_non_empty({json_field_as_string(rule, "id"),
                                          json_field_as_string(rule, "location_id"),
                                          json_field_as_string(rule, "target_id")});
    if (rule_id == location_id) {
      apply_probe_location_rule(check, rule);
    }
  }
}

nlohmann::json generated_probe_checks(const nlohmann::json& catalog,
                                      const nlohmann::json& logic_rules,
                                      const nlohmann::json& placement_rules) {
  nlohmann::json checks = nlohmann::json::array();
  for (const auto& location : filter_fillable_locations(catalog.value("locations", nlohmann::json::array()),
                                                        placement_rules)) {
    auto check = location_with_placement_rule(location, placement_rules);
    apply_probe_location_rules(check, object_child_or(logic_rules, "locations", nlohmann::json::array()));
    apply_probe_location_rules(check, rules_from_location_rule_collection(object_child_or(logic_rules,
                                                                                         "location_refinements",
                                                                                         nlohmann::json::object())));
    const auto segmentation = object_child_or(logic_rules, "location_rule_segmentation", nlohmann::json::object());
    apply_probe_location_rules(check, rules_from_location_rule_collection(segmentation));
    const auto segments = object_child_or(segmentation, "segments", nlohmann::json::array());
    if (segments.is_array()) {
      const auto location_id = first_non_empty({json_field_as_string(check, "id"),
                                                json_field_as_string(check, "location_id")});
      for (const auto& segment : segments) {
        const auto locations = segment.contains("location_ids")
            ? segment.at("location_ids")
            : segment.value("locations", nlohmann::json::array());
        const auto ids = ids_from_json_array(locations);
        if (ids.count(location_id) > 0) {
          apply_probe_location_rule(check, segment);
        }
      }
    }
    if (!check.contains("requires")) {
      check["requires"] = {{"op", "true"}};
    }
    checks.push_back(std::move(check));
  }
  return checks;
}

nlohmann::json find_probe_check(const nlohmann::json& checks, const std::string& location_id) {
  for (const auto& check : checks) {
    if (json_field_as_string(check, "id") == location_id ||
        json_field_as_string(check, "location_id") == location_id) {
      return check;
    }
  }
  return nlohmann::json::object();
}

nlohmann::json find_probe_item(const nlohmann::json& catalog,
                               const nlohmann::json& policy,
                               const std::string& item_id) {
  nlohmann::json items = catalog.value("item_pool", nlohmann::json::array());
  const auto pool = policy.value("separate_key_pool", nlohmann::json::array());
  const auto pool_items = pool.is_object() ? pool.value("items", nlohmann::json::array()) : pool;
  for (const auto& item : pool_items) {
    items.push_back(item);
  }
  for (const auto& item : items) {
    const auto id = first_non_empty({json_field_as_string(item, "id"),
                                     json_field_as_string(item, "item_id"),
                                     json_field_as_string(item, "semantic_id")});
    if (id == item_id) {
      return item;
    }
  }
  return nlohmann::json::object();
}

bool native_probe_item_placeable(const nlohmann::json& checks, const nlohmann::json& item) {
  for (const auto& check : checks) {
    if (placement_allows_item(check, item)) {
      return true;
    }
  }
  return false;
}

bool native_probe_contract_like(const nlohmann::json& value) {
  if (!value.is_object()) {
    return false;
  }
  for (const auto& key : {"requires", "facts", "produced_facts", "consumed_facts",
                          "expected_facts", "expected_reachable_regions",
                          "expected_unreachable_regions", "expected_reachable_locations",
                          "expected_placeable_items", "family", "surface",
                          "surface_id", "surface_ref", "type"}) {
    if (value.contains(key)) {
      return true;
    }
  }
  return false;
}

std::string canonical_probe_surface(std::string value) {
  if (value.find("region_graph") != std::string::npos || value.find("region-graph") != std::string::npos) {
    return "region_graph";
  }
  if (value.find("location_rule_segmentation") != std::string::npos ||
      value.find("segmentation") != std::string::npos) {
    return "location_rule_segmentation";
  }
  if (value.find("per_location_refinements") != std::string::npos) {
    return "per_location_refinements";
  }
  if (value.find("location_rule_refinements") != std::string::npos) {
    return "location_rule_refinements";
  }
  if (value.find("location_refinements") != std::string::npos ||
      value.find("refinement") != std::string::npos) {
    return "location_refinements";
  }
  if (value.find("dungeon_key_policy") != std::string::npos ||
      value.find("key_policy") != std::string::npos ||
      value.find("dungeon_key") != std::string::npos) {
    return "dungeon_key_policy";
  }
  if (value.find("placement") != std::string::npos || value.find("fillable") != std::string::npos) {
    return "placement";
  }
  return value;
}

std::string native_probe_contract_surface(const nlohmann::json& contract) {
  return canonical_probe_surface(first_non_empty({
      json_field_as_string(contract, "surface_id"),
      json_field_as_string(contract, "surface_ref"),
      json_field_as_string(contract, "surface"),
      json_field_as_string(contract, "family"),
      json_field_as_string(contract, "type"),
  }));
}

bool execute_native_probe_contract(const nlohmann::json& catalog,
                                   const nlohmann::json& logic_rules,
                                   const nlohmann::json& placement_rules,
                                   const nlohmann::json& policy,
                                   const nlohmann::json& binding,
                                   const nlohmann::json& contract) {
  auto facts = native_probe_contract_facts(logic_rules, contract);
  const auto region_graph = effective_region_graph_object(object_child_or(logic_rules, "region_graph",
                                                                          nlohmann::json::object()));
  derive_region_reachability_facts(region_graph, facts);
  if (contract.contains("requires") && !evaluate_logic_expr(contract.at("requires"), facts)) {
    return false;
  }
  for (const auto& fact : json_string_array(contract.value("expected_facts", nlohmann::json::array()))) {
    if (facts.count(fact) == 0) {
      return false;
    }
  }
  for (const auto& region : json_string_array(contract.value("expected_reachable_regions", nlohmann::json::array()))) {
    if (facts.count(region_reachability_fact(region)) == 0) {
      return false;
    }
  }
  const auto checks = generated_probe_checks(catalog, logic_rules, placement_rules);
  for (const auto& location_id : json_string_array(contract.value("expected_reachable_locations",
                                                                  nlohmann::json::array()))) {
    const auto check = find_probe_check(checks, location_id);
    if (check.empty() ||
        !evaluate_logic_expr(check.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
      return false;
    }
  }
  for (const auto& item_id : json_string_array(contract.value("expected_placeable_items",
                                                              nlohmann::json::array()))) {
    if (!native_probe_item_placeable(checks, find_probe_item(catalog, policy, item_id))) {
      return false;
    }
  }
  const auto family = first_non_empty({json_field_as_string(contract, "family"),
                                       json_field_as_string(contract, "surface"),
                                       json_field_as_string(contract, "type")});
  if (family == "dungeon_key_policy" || family == "dungeon_key" || family == "key_policy") {
    return native_dungeon_key_policy_execution_report(catalog,
                                                      logic_rules,
                                                      placement_rules,
                                                      policy,
                                                      binding).value("failed_count", 0) == 0;
  }
  return true;
}

void add_native_probe_contract_from_index(const nlohmann::json& contract,
                                          const std::string& fallback_surface,
                                          const std::string& fallback_id,
                                          nlohmann::json& contracts) {
  if (!contract.is_object() || !native_probe_contract_like(contract)) {
    return;
  }
  auto normalized = contract;
  if (!fallback_id.empty() && !normalized.contains("id")) {
    normalized["id"] = fallback_id;
  }
  if (!fallback_surface.empty() &&
      !normalized.contains("surface_id") &&
      !normalized.contains("surface_ref") &&
      !normalized.contains("surface") &&
      !normalized.contains("family")) {
    normalized["surface_id"] = fallback_surface;
  }
  contracts.push_back(std::move(normalized));
}

void collect_native_probe_contract_index(const nlohmann::json& index,
                                         const std::string& fallback_surface,
                                         nlohmann::json& contracts) {
  if (index.is_array()) {
    for (const auto& contract : index) {
      add_native_probe_contract_from_index(contract, fallback_surface, {}, contracts);
    }
    return;
  }
  if (!index.is_object()) {
    return;
  }
  const auto contracts_child = index.contains("contracts") ? index.at("contracts")
      : (index.contains("probes") ? index.at("probes") : nlohmann::json{});
  if (contracts_child.is_array()) {
    for (const auto& contract : contracts_child) {
      add_native_probe_contract_from_index(contract, fallback_surface, {}, contracts);
    }
  }
  for (const auto& [key, child] : index.items()) {
    if (key == "contracts" || key == "probes" || key == "schema_version" || key == "status") {
      continue;
    }
    if (child.is_array()) {
      collect_native_probe_contract_index(child, key, contracts);
    } else if (child.is_object() && (child.contains("contracts") || child.contains("probes"))) {
      collect_native_probe_contract_index(child, key, contracts);
    } else {
      add_native_probe_contract_from_index(child, fallback_surface, key, contracts);
    }
  }
  add_native_probe_contract_from_index(index, fallback_surface, {}, contracts);
}

void collect_native_probe_contracts(const nlohmann::json& value, nlohmann::json& contracts) {
  if (value.is_array()) {
    for (const auto& child : value) {
      collect_native_probe_contracts(child, contracts);
    }
    return;
  }
  if (!value.is_object()) {
    return;
  }
  for (const auto& key : {"native_probe_contract_index", "native_probe_contracts", "native_probe_contract"}) {
    if (value.contains(key)) {
      collect_native_probe_contract_index(value.at(key), {}, contracts);
    }
  }
  for (auto it = value.begin(); it != value.end(); ++it) {
    const auto key = it.key();
    if (key == "authorizes" || key == "requires" || key == "proof" || key == "native_proof" ||
        key == "authorization" || key == "native_probe_contract_index" ||
        key == "native_probe_contracts" || key == "native_probe_contract") {
      continue;
    }
    collect_native_probe_contracts(*it, contracts);
  }
}

// Legacy declaration body is intentionally replaced by the surface-aware collector above.

nlohmann::json native_probe_contract_execution_report(const nlohmann::json& catalog,
                                                      const nlohmann::json& logic_rules,
                                                      const nlohmann::json& placement_rules,
                                                      const nlohmann::json& surface_logic,
                                                      const nlohmann::json& policy,
                                                      const nlohmann::json& binding) {
  nlohmann::json contracts = nlohmann::json::array();
  collect_native_probe_contracts(surface_logic, contracts);
  collect_native_probe_contracts(logic_rules, contracts);
  collect_native_probe_contracts(placement_rules, contracts);
  collect_native_probe_contracts(catalog, contracts);
  std::size_t passed = 0;
  nlohmann::json failures = nlohmann::json::array();
  std::map<std::string, nlohmann::json> by_surface_map;
  for (const auto& contract : contracts) {
    const auto surface = native_probe_contract_surface(contract).empty()
        ? std::string{"unknown"}
        : native_probe_contract_surface(contract);
    auto& surface_report = by_surface_map[surface];
    if (surface_report.is_null()) {
      surface_report = {{"contract_count", 0}, {"pass_count", 0}, {"fail_count", 0}};
    }
    surface_report["contract_count"] = surface_report.value("contract_count", 0) + 1;
    if (execute_native_probe_contract(catalog, logic_rules, placement_rules, policy, binding, contract)) {
      ++passed;
      surface_report["pass_count"] = surface_report.value("pass_count", 0) + 1;
    } else {
      surface_report["fail_count"] = surface_report.value("fail_count", 0) + 1;
      if (failures.size() < 12) {
        failures.push_back({{"id", first_non_empty({json_field_as_string(contract, "id"),
                                                     json_field_as_string(contract, "contract_id")})},
                            {"family", first_non_empty({json_field_as_string(contract, "family"),
                                                         json_field_as_string(contract, "surface"),
                                                         json_field_as_string(contract, "type")})}});
      }
    }
  }
  const auto failed = contracts.size() - passed;
  nlohmann::json by_surface = nlohmann::json::object();
  for (auto& [surface, surface_report] : by_surface_map) {
    surface_report["status"] = surface_report.value("fail_count", 0) == 0 ? "passed" : "failed";
    by_surface[surface] = surface_report;
  }
  return {{"status", contracts.empty() ? "not_declared" : (failed == 0 ? "passed" : "failed")},
          {"contract_count", contracts.size()},
          {"pass_count", passed},
          {"fail_count", failed},
          {"by_surface", by_surface},
          {"failures", failures}};
}

nlohmann::json native_probe_contract_surface_report(const nlohmann::json& report,
                                                    const std::string& surface,
                                                    const nlohmann::json& fallback) {
  const auto by_surface = object_child_or(report, "by_surface", nlohmann::json::object());
  if (!by_surface.is_object() || !by_surface.contains(surface)) {
    return fallback;
  }
  const auto& surface_report = by_surface.at(surface);
  return {
      {"status", surface_report.value("fail_count", 0) == 0 ? "passed" : "failed"},
      {"required_count", surface_report.value("contract_count", 0)},
      {"passed_count", surface_report.value("pass_count", 0)},
      {"failed_count", surface_report.value("fail_count", 0)},
  };
}

nlohmann::json status_counts_for_rules(const nlohmann::json& rules) {
  nlohmann::json counts = {
      {"total", 0},
      {"consumable", 0},
      {"candidate_or_not_consumed", 0},
      {"not_authorizing", 0},
      {"unstated", 0},
  };
  if (!rules.is_array()) {
    return counts;
  }
  counts["total"] = rules.size();
  for (const auto& rule : rules) {
    const auto status = object_status(rule);
    if (status.empty()) {
      counts["unstated"] = counts.value("unstated", 0) + 1;
    }
    if (status.find("consumable") != std::string::npos) {
      counts["consumable"] = counts.value("consumable", 0) + 1;
    }
    if (has_not_consumed_candidate_surface(rule)) {
      counts["candidate_or_not_consumed"] = counts.value("candidate_or_not_consumed", 0) + 1;
    }
    if (json_object_status_contains(rule, "not-authorizing")) {
      counts["not_authorizing"] = counts.value("not_authorizing", 0) + 1;
    }
  }
  return counts;
}

bool contains_string(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool is_supported_patch_mode(const std::string& mode) {
  return mode == "artifact" || mode == "contract_only" || mode == "server_dispatch" ||
         mode == "none" || mode == "external_import";
}

std::string patch_mode_from_patch_surface(const nlohmann::json& patch) {
  if (!patch.is_object()) {
    return "unspecified";
  }
  if (patch.contains("mode") && patch.at("mode").is_string()) {
    return patch.at("mode").get<std::string>();
  }
  const auto manifest = patch.value("manifest", nlohmann::json::object());
  const auto manifest_patch = manifest.value("patch", nlohmann::json::object());
  return manifest_patch.value("mode", std::string{"unspecified"});
}

std::string blocker_phase(const std::string& reason) {
  if (reason == "can_validate_options" || reason == "can_build_item_pool" ||
      reason == "can_solve_logic" || reason == "can_place_items" ||
      reason == "can_emit_patch" || reason == "can_emit_room_contract" ||
      reason == "external_tools_required" || reason == "unsupported_options") {
    return "capability";
  }
  if (reason.find("item_pool") != std::string::npos ||
      reason == "generation_item_location_count_mismatch") {
    return "item_pool";
  }
  if (reason.find("logic") != std::string::npos ||
      reason.find("region_graph") != std::string::npos ||
      reason.find("dungeon_key_policy") != std::string::npos ||
      reason.find("reward_contract") != std::string::npos ||
      reason.find("medallion_contract") != std::string::npos ||
      reason.find("completion_contract") != std::string::npos ||
      reason.find("location_rule") != std::string::npos ||
      reason.find("location_refinements") != std::string::npos ||
      reason.find("progressive_effects") != std::string::npos) {
    return "solver";
  }
  if (reason.find("placement") != std::string::npos ||
      reason.find("fillable") != std::string::npos ||
      reason.find("risk_audit") != std::string::npos) {
    return "placer";
  }
  if (reason.find("patch") != std::string::npos) {
    return "patch";
  }
  return "surface";
}

nlohmann::json blockers_by_phase(const std::vector<std::string>& blockers) {
  nlohmann::json grouped = nlohmann::json::object();
  for (const auto& blocker : blockers) {
    const auto phase = blocker_phase(blocker);
    if (!grouped.contains(phase)) {
      grouped[phase] = nlohmann::json::array();
    }
    grouped[phase].push_back(blocker);
  }
  return grouped;
}

std::vector<std::string> surface_blockers_for(const std::vector<std::string>& blockers,
                                              const std::vector<std::string>& capability_blockers) {
  std::vector<std::string> surface_blockers;
  for (const auto& blocker : blockers) {
    if (!contains_string(capability_blockers, blocker)) {
      surface_blockers.push_back(blocker);
    }
  }
  return surface_blockers;
}

bool has_phase_blocker(const std::vector<std::string>& blockers, const std::string& phase) {
  return std::any_of(blockers.begin(), blockers.end(), [&](const auto& blocker) {
    return blocker_phase(blocker) == phase;
  });
}

std::size_t effective_fillable_location_count(const nlohmann::json& catalog,
                                              const nlohmann::json& placement_rules) {
  const auto locations = object_child_or(catalog, "locations", nlohmann::json::array());
  if (!locations.is_array()) {
    return 0;
  }
  if (!placement_rules.is_object()) {
    return locations.size();
  }

  auto fillable_locations = placement_rules.contains("fillable_location_ids")
      ? placement_rules.at("fillable_location_ids")
      : placement_rules.value("fillable_locations", nlohmann::json::array());
  if ((!fillable_locations.is_array() || fillable_locations.empty()) &&
      placement_rules.contains("fillable_locations_source")) {
    const auto& source = placement_rules.at("fillable_locations_source");
    const auto set_name = placement_rules.value("fillable_location_set", std::string{"main_pool_fillable"});
    if (source.is_object() && source.contains("sets") && source.at("sets").contains(set_name) &&
        source.at("sets").at(set_name).is_array()) {
      fillable_locations = source.at("sets").at(set_name);
    }
  }
  const auto reserved_locations = placement_rules.contains("reserved_location_ids")
      ? placement_rules.at("reserved_location_ids")
      : placement_rules.value("reserved_locations", nlohmann::json::array());

  const auto fillable_ids = ids_from_json_array(fillable_locations);
  const auto reserved_ids = ids_from_json_array(reserved_locations);
  std::size_t count = 0;
  for (const auto& location : locations) {
    const auto location_id = first_non_empty({
        json_field_as_string(location, "id"),
        json_field_as_string(location, "location_id"),
    });
    if (reserved_ids.count(location_id) > 0) {
      continue;
    }
    if (!fillable_ids.empty() && fillable_ids.count(location_id) == 0) {
      continue;
    }
    ++count;
  }
  return count;
}

nlohmann::json first_object_contract_surface(const std::vector<nlohmann::json>& parents,
                                             std::initializer_list<const char*> keys) {
  for (const auto& parent : parents) {
    if (!parent.is_object()) {
      continue;
    }
    for (const auto* key : keys) {
      if (parent.contains(key) && parent.at(key).is_object()) {
        return parent.at(key);
      }
    }
  }
  return nlohmann::json::object();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: sekailink_generic_generation_probe <linkedworld_root>\n";
    return 2;
  }

  std::string error;
  const auto surface = sekailink_server::load_linkedworld_generation_surface(std::filesystem::path(argv[1]), &error);
  if (!surface) {
    std::cerr << "load_failed: " << error << "\n";
    return 1;
  }

  const auto missing_capabilities =
      sekailink_server::missing_required_generation_capabilities(surface->capabilities);
  const auto missing_requirements =
      sekailink_server::missing_required_generation_surface_requirements(*surface);
  const auto surface_blockers = surface_blockers_for(missing_requirements, missing_capabilities);
  const auto logic_rules = object_child_or(surface->generation_rules, "logic_rules", nlohmann::json::object());
  const auto placement_rules = object_child_or(surface->generation_rules, "placement_rules", nlohmann::json::object());
  const auto fillable_set_name = placement_rules.is_object()
      ? placement_rules.value("fillable_location_set", std::string{"main_pool_fillable"})
      : std::string{"main_pool_fillable"};
  std::size_t explicit_fillable_count = 0;
  std::size_t expected_fillable_count = placement_rules.is_object()
      ? placement_rules.value("expected_fillable_count", 0)
      : 0;
  auto fillable_locations_pending_audit = false;
  auto risk_audit_status = std::string{};
  std::size_t risk_tag_count = 0;
  nlohmann::json fillable_validation = nlohmann::json::object();
  nlohmann::json risk_audit = nlohmann::json::object();
  if (placement_rules.is_object() && placement_rules.contains("fillable_locations_source") &&
      placement_rules.at("fillable_locations_source").is_object()) {
    const auto& fillable_source = placement_rules.at("fillable_locations_source");
    expected_fillable_count = fillable_source.value("expected_fillable_count", expected_fillable_count);
    fillable_validation = fillable_source.value("validation", nlohmann::json::object());
    fillable_locations_pending_audit =
        object_flag_is_true(fillable_validation, "blocks_can_place_items_until_audited") &&
        !(declares_consumed_generation_surface(fillable_validation) &&
          explicitly_authorizes_native_placement(fillable_validation));
    risk_audit = fillable_source.value("risk_audit", nlohmann::json::object());
    risk_audit_status = object_status(risk_audit);
    risk_tag_count = json_array_size(object_child_or(risk_audit, "location_tags_pending", nlohmann::json::array()));
    if (fillable_source.contains("sets") && fillable_source.at("sets").contains(fillable_set_name) &&
        fillable_source.at("sets").at(fillable_set_name).is_array()) {
      explicit_fillable_count = fillable_source.at("sets").at(fillable_set_name).size();
    }
  }
  const auto candidate_location_rules =
      object_child_or(logic_rules, "candidate_location_rules", nlohmann::json::object());
  const auto candidate_item_effects =
      object_child_or(logic_rules, "candidate_item_effects", nlohmann::json::object());
  const auto declared_location_rules =
      nested_collection(logic_rules, "locations", "rules", nlohmann::json::array());
  const auto declared_item_effects =
      nested_collection(logic_rules, "item_effects", "effects", nlohmann::json::object());
  const auto logic_ruleset =
      object_child_or(logic_rules, "ruleset", nlohmann::json::object());
  const auto region_graph =
      object_child_or(logic_rules, "region_graph", nlohmann::json::object());
  const auto region_edge_audit =
      object_child_or(region_graph, "edge_audit", nlohmann::json::object());
  const auto edge_blocker_requirements = region_edge_audit.is_object()
      ? region_edge_audit.value("edge_blocker_requirements", nlohmann::json::array())
      : nlohmann::json::array();
  const auto missing_generation_contract_surfaces = region_edge_audit.is_object()
      ? object_child_or(region_edge_audit, "missing_generation_contract_surfaces", nlohmann::json::object())
      : nlohmann::json::object();
  const auto item_pool_source =
      object_child_or(surface->catalog, "item_pool_source", nlohmann::json::object());
  const auto raw_dungeon_key_policy = item_pool_source.is_object()
      ? object_child_or(item_pool_source, "dungeon_key_policy", nlohmann::json::object())
      : nlohmann::json::object();
  const auto dungeon_key_policy = raw_dungeon_key_policy.is_object()
      ? raw_dungeon_key_policy
      : nlohmann::json::object();
  const auto raw_separate_key_pool = dungeon_key_policy.is_object()
      ? object_child_or(dungeon_key_policy, "separate_key_pool", nlohmann::json::object())
      : nlohmann::json::object();
  const auto separate_key_pool = raw_separate_key_pool.is_object()
      ? raw_separate_key_pool
      : nlohmann::json{{"items", raw_separate_key_pool}};
  const auto raw_dungeon_key_policy_binding =
      object_child_or(logic_rules, "dungeon_key_policy_binding", nlohmann::json::object());
  const auto dungeon_key_policy_binding = raw_dungeon_key_policy_binding.is_object()
      ? raw_dungeon_key_policy_binding
      : nlohmann::json::object();
  const auto region_traversal_plan =
      object_child_or(logic_rules, "region_traversal_binding_plan", nlohmann::json::object());
  const auto region_traversal_coverage =
      object_child_or(region_traversal_plan, "coverage", nlohmann::json::object());
  const auto location_rule_segmentation =
      object_child_or(logic_rules, "location_rule_segmentation", nlohmann::json::object());
  const auto location_segmentation_coverage =
      object_child_or(location_rule_segmentation, "coverage", nlohmann::json::object());
  const auto location_refinements =
      object_child_or(logic_rules, "location_refinements", nlohmann::json::object());
  const auto per_location_refinements =
      object_child_or(logic_rules, "per_location_refinements", nlohmann::json::object());
  const auto location_rule_refinements =
      object_child_or(logic_rules, "location_rule_refinements", nlohmann::json::object());
  const auto reward_contract =
      first_object_contract_surface({logic_rules, placement_rules},
                                    {"reward_contract", "reward_policy", "reward_gate_contract"});
  const auto medallion_contract =
      first_object_contract_surface({logic_rules, placement_rules},
                                    {"medallion_contract", "medallion_policy"});
  const auto completion_contract =
      first_object_contract_surface({logic_rules, surface->logic},
                                    {"completion_contract", "completion_policy",
                                     "completion_rules_contract"});
  const auto patch_mode = patch_mode_from_patch_surface(surface->patch);
  const auto generation_item_pool_count =
      surface->catalog.value("item_pool", nlohmann::json::array()).size();
  const auto effective_fillable_count =
      effective_fillable_location_count(surface->catalog, placement_rules);
  const auto item_location_count_match = generation_item_pool_count == effective_fillable_count;
  const auto native_edge_accessibility_probe =
      native_replacement_edge_execution_report(logic_rules, region_graph);
  const auto native_location_refinement_probe =
      native_location_surface_execution_report(logic_rules,
                                               effective_region_graph_object(region_graph),
                                               location_refinements);
  const auto native_per_location_refinement_probe =
      native_location_surface_execution_report(logic_rules,
                                               effective_region_graph_object(region_graph),
                                               per_location_refinements);
  const auto native_location_rule_refinement_probe =
      native_location_surface_execution_report(logic_rules,
                                               effective_region_graph_object(region_graph),
                                               location_rule_refinements);
  const auto native_segmentation_probe =
      native_location_surface_execution_report(logic_rules,
                                               effective_region_graph_object(region_graph),
                                               location_rule_segmentation);
  const auto native_dungeon_key_policy_probe =
      native_dungeon_key_policy_execution_report(surface->catalog,
                                                 logic_rules,
                                                 placement_rules,
                                                 dungeon_key_policy,
                                                 dungeon_key_policy_binding);
  const auto native_probe_contract_report =
      native_probe_contract_execution_report(surface->catalog,
                                             logic_rules,
                                             placement_rules,
                                             surface->logic,
                                             dungeon_key_policy,
                                             dungeon_key_policy_binding);
  const auto region_graph_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report, "region_graph", native_edge_accessibility_probe);
  const auto location_refinement_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report,
                                           "location_refinements",
                                           native_location_refinement_probe);
  const auto per_location_refinement_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report,
                                           "per_location_refinements",
                                           native_per_location_refinement_probe);
  const auto location_rule_refinement_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report,
                                           "location_rule_refinements",
                                           native_location_rule_refinement_probe);
  const auto segmentation_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report,
                                           "location_rule_segmentation",
                                           native_segmentation_probe);
  const auto dungeon_key_policy_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report,
                                           "dungeon_key_policy",
                                           native_dungeon_key_policy_probe);
  const auto placement_probe_report =
      native_probe_contract_surface_report(native_probe_contract_report,
                                           "placement",
                                           nlohmann::json{{"status", "not_required"},
                                                          {"required_count", 0},
                                                          {"passed_count", 0},
                                                          {"failed_count", 0}});
  const nlohmann::json package_consistency = {
      {"generation_scope", "multiworld"},
      {"single_seed_package", true},
      {"slot_local_item_location_count_match", item_location_count_match},
      {"effective_fillable_location_count", effective_fillable_count},
      {"generation_item_pool_count", generation_item_pool_count},
      {"requires_checks_items_placements_refs", true},
      {"requires_replay_validation", true},
      {"requires_per_slot_patch_contract", true},
      {"patch_artifact_expected", patch_mode == "artifact"},
  };
  const nlohmann::json patch_mode_report = {
      {"mode", patch_mode},
      {"supported", is_supported_patch_mode(patch_mode)},
      {"recognized_modes", nlohmann::json::array({"artifact", "contract_only", "server_dispatch", "none", "external_import"})},
      {"requires_artifact", patch_mode == "artifact"},
      {"requires_server_dispatch", patch_mode == "server_dispatch"},
      {"emits_patch_contract", true},
      {"manifest_ref", surface->patch.value("manifest_ref", std::string{})},
      {"manifest_loaded", surface->patch.contains("manifest")},
      {"artifact_extension", surface->patch.value("patch_file_extension", std::string{})},
  };
  const nlohmann::json readiness = {
      {"can_native_generate", missing_requirements.empty()},
      {"blocker_count", missing_requirements.size()},
      {"blockers", missing_requirements},
      {"blockers_by_phase", blockers_by_phase(missing_requirements)},
      {"capability_blockers", missing_capabilities},
      {"surface_blocker_count", surface_blockers.size()},
      {"surface_blockers", surface_blockers},
      {"surface_blockers_by_phase", blockers_by_phase(surface_blockers)},
      {"solver_surface_ready", !has_phase_blocker(surface_blockers, "solver")},
      {"placer_surface_ready", !has_phase_blocker(surface_blockers, "placer")},
      {"solver_ready", surface->capabilities.can_solve_logic &&
           !has_phase_blocker(surface_blockers, "solver")},
      {"placer_ready", surface->capabilities.can_place_items &&
           !has_phase_blocker(surface_blockers, "placer")},
      {"patch_ready", is_supported_patch_mode(patch_mode) &&
           !contains_string(missing_requirements, "can_emit_patch") &&
           !contains_string(missing_requirements, "unsupported_patch_mode")},
      {"package_consistent", item_location_count_match},
  };
  const nlohmann::json report = {
      {"module_id", surface->module_id},
      {"game_key", surface->game_key},
      {"linkedworld_id", surface->linkedworld_id},
      {"version", surface->version},
      {"source_path", surface->source_path.string()},
      {"patch_mode", patch_mode},
      {"patch_mode_report", patch_mode_report},
      {"patch_manifest_ref", surface->patch.value("manifest_ref", std::string{})},
      {"patch_manifest_loaded", surface->patch.contains("manifest")},
      {"patch_artifact_extension", surface->patch.value("patch_file_extension", std::string{})},
      {"capabilities", sekailink_server::to_json(surface->capabilities)},
      {"missing_required_generation_capabilities", missing_capabilities},
      {"missing_required_generation_requirements", missing_requirements},
      {"missing_native_generation_reasons", missing_requirements},
      {"native_generation_surface_blockers", surface_blockers},
      {"native_probe_contract_status", native_probe_contract_report.value("status", std::string{})},
      {"native_probe_contract_count", native_probe_contract_report.value("contract_count", 0)},
      {"native_probe_pass_count", native_probe_contract_report.value("pass_count", 0)},
      {"native_probe_fail_count", native_probe_contract_report.value("fail_count", 0)},
      {"native_probe_by_surface", native_probe_contract_report.value("by_surface", nlohmann::json::object())},
      {"native_probe_failures", native_probe_contract_report.value("failures", nlohmann::json::array())},
      {"native_generation_readiness", readiness},
      {"package_consistency", package_consistency},
      {"location_count", surface->catalog.value("locations", nlohmann::json::array()).size()},
      {"tracker_item_count", surface->catalog.value("items", nlohmann::json::array()).size()},
      {"generation_item_pool_count", generation_item_pool_count},
      {"effective_fillable_location_count", effective_fillable_count},
      {"item_location_count_match", item_location_count_match},
      {"explicit_fillable_location_count", explicit_fillable_count},
      {"expected_fillable_location_count", expected_fillable_count},
      {"fillable_location_set", fillable_set_name},
      {"fillable_locations_pending_audit", fillable_locations_pending_audit},
      {"risk_audit_status", risk_audit_status},
      {"risk_tag_count", risk_tag_count},
      {"logic_ruleset_status", object_status(logic_ruleset)},
      {"logic_ruleset_has_explicit_native_logic_authorization",
       has_explicit_authorization_key(logic_ruleset, "native_logic_solve") ||
           has_explicit_authorization_key(logic_ruleset, "native_location_reachability")},
      {"logic_ruleset_authorizes_native_logic", authorizes_native_logic_solve(logic_ruleset)},
      {"logic_ruleset_not_authorizing",
       json_object_status_contains(logic_ruleset, "not-authorizing") ||
           !authorizes_native_logic_solve(logic_ruleset)},
      {"logic_ruleset_missing_authorization_keys",
       missing_authorization_keys(logic_ruleset, {"native_logic_solve"})},
      {"item_effects_status", object_status(object_child_or(logic_rules, "item_effects", nlohmann::json::object()))},
      {"location_rules_status", object_status(object_child_or(logic_rules, "locations", nlohmann::json::object()))},
      {"declared_item_effect_count", json_collection_size(declared_item_effects)},
      {"declared_location_rule_count", json_collection_size(declared_location_rules)},
      {"declared_location_rule_status_counts", status_counts_for_rules(declared_location_rules)},
      {"declared_location_rules_readiness", {
          {"rules_visible_as_refinement_surface", json_collection_size(declared_location_rules) > 0},
          {"candidate_or_not_consumed_rule_count",
           status_counts_for_rules(declared_location_rules).value("candidate_or_not_consumed", 0)},
          {"not_authorizing_rule_count",
           status_counts_for_rules(declared_location_rules).value("not_authorizing", 0)},
      }},
      {"location_refinements_status", object_status(location_refinements)},
      {"location_refinement_count", location_rule_collection_size(location_refinements)},
      {"location_refinements_consumed", declares_consumed_generation_surface(location_refinements)},
      {"location_refinements_has_explicit_native_logic_authorization",
       has_explicit_authorization_key(location_refinements, "native_logic_solve") ||
           has_explicit_authorization_key(location_refinements, "native_location_reachability")},
      {"location_refinements_authorizes_native_logic", authorizes_native_logic_solve(location_refinements)},
      {"location_refinements_missing_authorization_keys",
       missing_authorization_keys(location_refinements, {"native_logic_solve"})},
      {"location_refinements_missing_fact_proof_count",
       location_surface_missing_fact_proof_count(location_refinements)},
      {"location_refinements_native_consumer_probe_status",
       location_refinement_probe_report.value("status", std::string{})},
      {"location_refinements_native_consumer_probe_count",
       location_refinement_probe_report.value("required_count", 0)},
      {"location_refinements_native_consumer_probe_failed_count",
       location_refinement_probe_report.value("failed_count", 0)},
      {"per_location_refinements_status", object_status(per_location_refinements)},
      {"per_location_refinement_count", location_rule_collection_size(per_location_refinements)},
      {"per_location_refinements_missing_fact_proof_count",
       location_surface_missing_fact_proof_count(per_location_refinements)},
      {"per_location_refinements_native_consumer_probe_status",
       per_location_refinement_probe_report.value("status", std::string{})},
      {"per_location_refinements_native_consumer_probe_count",
       per_location_refinement_probe_report.value("required_count", 0)},
      {"per_location_refinements_native_consumer_probe_failed_count",
       per_location_refinement_probe_report.value("failed_count", 0)},
      {"location_rule_refinements_status", object_status(location_rule_refinements)},
      {"location_rule_refinement_count", location_rule_collection_size(location_rule_refinements)},
      {"location_rule_refinements_missing_fact_proof_count",
       location_surface_missing_fact_proof_count(location_rule_refinements)},
      {"location_rule_refinements_native_consumer_probe_status",
       location_rule_refinement_probe_report.value("status", std::string{})},
      {"location_rule_refinements_native_consumer_probe_count",
       location_rule_refinement_probe_report.value("required_count", 0)},
      {"location_rule_refinements_native_consumer_probe_failed_count",
       location_rule_refinement_probe_report.value("failed_count", 0)},
      {"location_rule_segmentation_status", object_status(location_rule_segmentation)},
      {"location_rule_segmentation_consumed", declares_consumed_generation_surface(location_rule_segmentation)},
      {"location_rule_segmentation_has_explicit_native_logic_authorization",
       has_explicit_authorization_key(location_rule_segmentation, "native_logic_solve") ||
           has_explicit_authorization_key(location_rule_segmentation, "native_location_reachability")},
      {"location_rule_segmentation_authorizes_native_logic", authorizes_native_logic_solve(location_rule_segmentation)},
      {"location_rule_segmentation_missing_authorization_keys",
       missing_authorization_keys(location_rule_segmentation, {"native_location_reachability"})},
      {"location_rule_segmentation_missing_fact_proof_count",
       location_surface_missing_fact_proof_count(location_rule_segmentation)},
      {"location_rule_segmentation_missing_coverage_count",
       segmentation_missing_coverage_count(location_rule_segmentation)},
      {"location_rule_segmentation_native_coverage_probe_status",
       segmentation_probe_report.value("status", std::string{})},
      {"location_rule_segmentation_native_coverage_probe_count",
       segmentation_probe_report.value("required_count", 0)},
      {"location_rule_segmentation_native_coverage_probe_failed_count",
       segmentation_probe_report.value("failed_count", 0)},
      {"location_rule_segment_count",
       json_collection_size(object_child_or(location_rule_segmentation, "segments", nlohmann::json::object()))},
      {"location_rule_segmented_count",
       object_child_or(location_segmentation_coverage, "primary_segment_count", 0)},
      {"region_traversal_plan_status", object_status(region_traversal_plan)},
      {"region_traversal_bound_location_count",
       object_child_or(region_traversal_coverage, "bound_location_count", 0)},
      {"region_traversal_expected_location_count",
       object_child_or(region_traversal_coverage, "expected_location_count", 0)},
      {"region_traversal_segment_count",
       object_child_or(region_traversal_coverage, "segment_count", 0)},
      {"region_graph_status", object_status(region_graph)},
      {"region_graph_authorizes_native_logic", authorizes_native_logic_solve(region_graph)},
      {"region_graph_starting_region_count",
       json_collection_size(object_child_or(region_graph, "starting_regions", nlohmann::json::array()))},
      {"region_graph_node_count",
       json_collection_size(object_child_or(region_graph, "nodes", nlohmann::json::array()))},
      {"region_graph_edge_count",
       json_collection_size(object_child_or(region_graph, "edges", nlohmann::json::array()))},
      {"region_graph_location_binding_count",
       json_collection_size(object_child_or(region_graph, "location_region_bindings", nlohmann::json::array()))},
      {"region_graph_edge_audit_status", object_status(region_edge_audit)},
      {"region_graph_edge_audit_consumed", declares_consumed_generation_surface(region_edge_audit)},
      {"region_graph_edge_audit_authorizes_native_logic", authorizes_native_logic_solve(region_edge_audit)},
      {"region_graph_audited_edge_count",
       object_child_or(region_edge_audit, "audited_edge_count", 0)},
      {"region_graph_blocked_edge_count",
       object_child_or(region_edge_audit, "blocked_edge_count", 0)},
      {"region_graph_edge_blocker_requirement_count",
       contract_requirement_count(edge_blocker_requirements)},
      {"region_graph_edge_blocker_requirements_status",
       object_status(edge_blocker_requirements)},
      {"region_graph_edge_blocker_requirements_consumed",
       declares_consumed_generation_surface(edge_blocker_requirements)},
      {"region_graph_edge_blocker_requirements_authorizes_native_logic",
       authorizes_native_logic_solve(edge_blocker_requirements)},
      {"region_graph_edge_blocker_contract_families",
       contract_families_for_surface(edge_blocker_requirements, "region_graph_edge_blocker_requirements")},
      {"region_graph_edge_blocker_requirements_block_native_logic",
       blocks_native_logic_contract(edge_blocker_requirements)},
      {"region_graph_missing_generation_contract_surface_count",
       json_collection_size(object_child_or(missing_generation_contract_surfaces,
                                            "surfaces",
                                            nlohmann::json::array()))},
      {"region_graph_missing_generation_contract_surfaces_status",
       object_status(missing_generation_contract_surfaces)},
      {"region_graph_missing_generation_contract_surfaces_consumed",
       declares_consumed_generation_surface(missing_generation_contract_surfaces)},
      {"region_graph_missing_generation_contract_surfaces_authorizes_native_logic",
       authorizes_native_logic_solve(missing_generation_contract_surfaces)},
      {"region_graph_missing_generation_contract_families",
       contract_families_for_surface(missing_generation_contract_surfaces, "missing_generation_contract_surfaces")},
      {"region_graph_missing_generation_contract_surfaces_block_native_logic",
       blocks_native_logic_contract(missing_generation_contract_surfaces)},
      {"region_graph_declared_placeholder_edge_count",
       count_region_graph_declared_placeholder_edges(region_graph)},
      {"region_graph_placeholder_edge_count",
       count_region_graph_placeholder_edges(region_graph)},
      {"region_graph_unresolved_placeholder_edges",
       unresolved_placeholder_edge_details(region_graph)},
      {"region_graph_authorized_consumed_edge_ids",
       authorized_consumed_region_graph_edge_ids(region_graph)},
      {"region_graph_replacement_edge_missing_produced_fact_count",
       replacement_edge_missing_produced_fact_count(region_graph)},
      {"region_graph_replacement_edge_missing_consumed_fact_count",
       replacement_edge_missing_consumed_fact_count(region_graph)},
      {"region_graph_native_edge_accessibility_probe_status",
       region_graph_probe_report.value("status", std::string{})},
      {"region_graph_native_edge_accessibility_probe_count",
       region_graph_probe_report.value("required_count", 0)},
      {"region_graph_native_edge_accessibility_probe_failed_count",
       region_graph_probe_report.value("failed_count", 0)},
      {"region_graph_consumed_edge_count",
       json_collection_size(consumed_region_graph_edges(region_graph))},
      {"region_graph_authorized_consumed_edge_count",
       count_authorized_consumed_region_graph_edges(region_graph)},
      {"dungeon_key_policy_status", object_status(dungeon_key_policy)},
      {"dungeon_key_policy_consumed", declares_consumed_generation_surface(dungeon_key_policy)},
      {"dungeon_key_policy_authorizes_native_placement", authorizes_native_placement(dungeon_key_policy)},
      {"dungeon_key_policy_authorizes_native_contract", authorizes_native_dungeon_key_policy(dungeon_key_policy)},
      {"dungeon_key_policy_missing_authorization_keys",
       missing_authorization_keys(dungeon_key_policy,
                                  {"native_placement", "native_item_pool", "native_dungeon_key_policy"})},
      {"dungeon_key_policy_missing_small_key_proof_count",
       dungeon_key_policy_missing_small_key_proof_count(dungeon_key_policy)},
      {"dungeon_key_policy_native_placement_probe_status",
       dungeon_key_policy_probe_report.value("status", std::string{})},
      {"dungeon_key_policy_native_placement_probe_count",
       dungeon_key_policy_probe_report.value("required_count", 0)},
      {"dungeon_key_policy_native_placement_probe_failed_count",
       dungeon_key_policy_probe_report.value("failed_count", 0)},
      {"dungeon_key_policy_native_small_key_placement_failed_count",
       native_dungeon_key_policy_probe.value("small_key_placement_failed_count", 0)},
      {"dungeon_key_separate_pool_count",
       json_collection_size(object_child_or(separate_key_pool, "items", nlohmann::json::array()))},
      {"dungeon_key_contributes_to_main_pool",
       object_child_or(separate_key_pool, "contributes_to_total_count", true)},
      {"dungeon_key_policy_binding_status", object_status(dungeon_key_policy_binding)},
      {"dungeon_key_policy_binding_consumed", declares_consumed_generation_surface(dungeon_key_policy_binding)},
      {"dungeon_key_policy_binding_authorizes_native_logic",
       authorizes_native_logic_solve(dungeon_key_policy_binding)},
      {"dungeon_key_policy_binding_missing_authorization_keys",
       missing_authorization_keys(dungeon_key_policy_binding, {"native_logic_solve"})},
      {"dungeon_key_policy_missing_self_lock_proof_count",
       dungeon_key_policy_missing_self_lock_proof_count(dungeon_key_policy_binding)},
      {"dungeon_key_policy_native_self_lock_probe_failed_count",
       native_dungeon_key_policy_probe.value("self_lock_failed_count", 0)},
      {"dungeon_key_blocked_big_chest_rule_count",
       object_child_or(object_child_or(dungeon_key_policy_binding, "coverage", nlohmann::json::object()),
                       "blocked_big_chest_rule_count",
                       0)},
      {"dungeon_key_blocked_big_chest_direct_rule_count",
       json_collection_size(object_child_or(dungeon_key_policy_binding,
                                            "blocked_big_chest_rules",
                                            nlohmann::json::array()))},
      {"reward_contract_status", object_status(reward_contract)},
      {"reward_contract_authorizes_native_logic", authorizes_native_logic_solve(reward_contract)},
      {"reward_contract_authorizes_native_placement", authorizes_native_placement(reward_contract)},
      {"reward_contract_requirement_count",
       contract_requirement_count(object_child_or(reward_contract, "requirements", reward_contract))},
      {"medallion_contract_status", object_status(medallion_contract)},
      {"medallion_contract_authorizes_native_logic", authorizes_native_logic_solve(medallion_contract)},
      {"medallion_contract_authorizes_native_placement", authorizes_native_placement(medallion_contract)},
      {"medallion_contract_requirement_count",
       contract_requirement_count(object_child_or(medallion_contract, "requirements", medallion_contract))},
      {"completion_contract_status", object_status(completion_contract)},
      {"completion_contract_authorizes_native_logic", authorizes_native_logic_solve(completion_contract)},
      {"completion_contract_requirement_count",
       contract_requirement_count(object_child_or(completion_contract, "requirements", completion_contract))},
      {"candidate_location_rule_count",
       json_collection_size(object_child_or(
           candidate_location_rules,
           "rules",
           object_child_or(candidate_location_rules, "locations", nlohmann::json::array())))},
      {"candidate_item_effect_count",
       json_collection_size(object_child_or(candidate_item_effects, "effects", nlohmann::json::object()))},
      {"fillable_locations_validation_status", object_status(fillable_validation)},
      {"fillable_locations_validation_blocks_until_audited",
       object_flag_is_true(fillable_validation, "blocks_can_place_items_until_audited")},
      {"fillable_locations_validation_consumed", declares_consumed_generation_surface(fillable_validation)},
      {"fillable_locations_validation_authorizes_native_placement",
       authorizes_native_placement(fillable_validation)},
      {"fillable_locations_validation_has_explicit_native_placement_authorization",
       has_explicit_authorization_key(fillable_validation, "native_placement")},
      {"fillable_locations_validation_pending_audit", fillable_locations_pending_audit},
      {"risk_audit_consumed", declares_consumed_generation_surface(risk_audit)},
      {"risk_audit_authorizes_native_placement", authorizes_native_placement(risk_audit)},
      {"risk_audit_has_explicit_native_placement_authorization",
       has_explicit_authorization_key(risk_audit, "native_placement")},
      {"risk_audit_not_consumed",
       has_not_consumed_candidate_surface(risk_audit) &&
           risk_tag_count > 0 &&
           !(declares_consumed_generation_surface(risk_audit) &&
             explicitly_authorizes_native_placement(risk_audit))},
      {"placement_authorizes_native",
       placement_rules.is_object() && placement_rules.contains("authorizes_native_placement")
           ? nlohmann::json(truthy_json_flag(placement_rules.at("authorizes_native_placement")))
           : nlohmann::json(nullptr)},
      {"placement_native_probe_status", placement_probe_report.value("status", std::string{})},
      {"placement_native_probe_count", placement_probe_report.value("required_count", 0)},
      {"placement_native_probe_failed_count", placement_probe_report.value("failed_count", 0)},
      {"has_fillable_locations_source", placement_rules.is_object() && placement_rules.contains("fillable_locations_source")},
      {"has_generation_item_pool", surface->catalog.contains("item_pool")},
      {"has_logic_rules", surface->generation_rules.contains("logic_rules")},
      {"has_placement_rules", surface->generation_rules.contains("placement_rules")},
      {"logic_rules_ref", surface->generation_rules.value("logic_rules_ref", std::string{})},
      {"placement_rules_ref", surface->generation_rules.value("placement_rules_ref", std::string{})},
      {"logic_rules_shape", surface->generation_rules.value("logic_rules_shape", std::string{})},
      {"placement_rules_shape", surface->generation_rules.value("placement_rules_shape", std::string{})},
      {"can_native_generate", missing_requirements.empty()},
  };
  std::cout << report.dump(2) << "\n";
  return missing_requirements.empty() ? 0 : 3;
}
