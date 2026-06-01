#include "sekailink_server/generic_generation.hpp"
#include "sekailink_server/generic_generation_checks.hpp"
#include "sekailink_server/generic_generation_internal.hpp"
#include "sekailink_server/generic_generation_logic_rules.hpp"
#include "sekailink_server/generic_generation_patch_contracts.hpp"
#include "sekailink_server/generic_generation_placement.hpp"
#include "sekailink_server/generic_generation_region_analysis.hpp"
#include "sekailink_server/generic_generation_region_graph.hpp"
#include "sekailink_server/generic_generation_surface_contracts.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace sekailink_server {

namespace {

std::size_t native_location_rule_execution_failure_count(const nlohmann::json& logic_rules,
                                                         const nlohmann::json& surface);
std::string canonical_probe_surface(std::string value);
bool native_probe_authorized_consumed_surface(const nlohmann::json& report,
                                              const std::string& surface_id,
                                              const nlohmann::json& surface,
                                              bool explicitly_authorizes);

using generation_internal::expand_catalog_refs;
using generation_internal::expand_patch_manifest_ref;
using generation_internal::base_instance_id;
using generation_internal::authorizes_native_logic_solve;
using generation_internal::authorizes_native_placement;
using generation_internal::authorizes_consumed_region_graph_edge;
using generation_internal::all_items_are_non_advancement;
using generation_internal::blocks_native_dungeon_key_policy_contract;
using generation_internal::blocks_native_logic_contract;
using generation_internal::blocks_native_placement_contract;
using generation_internal::clears_native_placement_audit_block;
using generation_internal::collect_fact_references;
using generation_internal::collect_grant_references;
using generation_internal::declares_consumed_generation_surface;
using generation_internal::derive_region_reachability_facts;
using generation_internal::derive_region_reachability_facts_from_locations;
using generation_internal::evaluate_logic_expr;
using generation_internal::consumed_region_graph_edges;
using generation_internal::collect_item_grants;
using generation_internal::deterministic_ordered_array;
using generation_internal::effective_region_graph_edges;
using generation_internal::effective_region_graph_object;
using generation_internal::filter_fillable_locations;
using generation_internal::enrich_item_with_generation_rules;
using generation_internal::enrich_location_with_generation_rules;
using generation_internal::find_rule_for_id;
using generation_internal::find_rules_for_id;
using generation_internal::find_region_binding_for_location;
using generation_internal::first_non_empty;
using generation_internal::first_object_truthy_flag;
using generation_internal::has_non_authorizing_status;
using generation_internal::has_authorized_consumed_region_graph_edges;
using generation_internal::has_static_placement_matching;
using generation_internal::has_not_consumed_candidate_surface;
using generation_internal::has_region_graph_placeholder_edges;
using generation_internal::has_any_tag;
using generation_internal::is_explicitly_consumable_logic_surface;
using generation_internal::json_array_contains_string;
using generation_internal::json_contains_string_fragment;
using generation_internal::json_field_as_string;
using generation_internal::json_object_status_contains;
using generation_internal::json_string_array;
using generation_internal::json_string_set;
using generation_internal::json_without_index;
using generation_internal::invalid_region_graph_reasons;
using generation_internal::explicitly_authorizes_native_dungeon_key_policy_surface;
using generation_internal::explicitly_authorizes_native_logic_solve;
using generation_internal::explicitly_authorizes_native_placement_surface;
using generation_internal::explicitly_consumed_generation_surface;
using generation_internal::facts_referenced_by_location_surface;
using generation_internal::location_rule_id;
using generation_internal::logic_item_effects_object;
using generation_internal::logic_location_refinement_rules_array;
using generation_internal::logic_location_rules_array;
using generation_internal::logic_region_graph_object;
using generation_internal::logic_segmented_location_rules_array;
using generation_internal::merge_grants;
using generation_internal::missing_fact_membership_count;
using generation_internal::native_proof_authorizes_logic;
using generation_internal::native_proof_authorizes_placement;
using generation_internal::native_proof_consumed;
using generation_internal::native_proof_object;
using generation_internal::native_proof_required;
using generation_internal::object_flag_is_true;
using generation_internal::object_truthy_flag;
using generation_internal::package_hash_material;
using generation_internal::placement_allows_item;
using generation_internal::placement_failure_diagnostics;
using generation_internal::push_unique;
using generation_internal::region_reachability_fact;
using generation_internal::region_graph_edge_from;
using generation_internal::region_graph_edge_id;
using generation_internal::region_graph_edge_to;
using generation_internal::region_graph_edges_match;
using generation_internal::should_run_exact_static_matching_guard;
using generation_internal::is_supported_patch_mode;
using generation_internal::patch_artifact_ref_for_slot;
using generation_internal::patch_contract_artifact_for_slot;
using generation_internal::patch_contract_ref_for_slot;
using generation_internal::patch_manifest_for_surface;
using generation_internal::patch_manifest_patch_for_surface;
using generation_internal::patch_mode_for_surface;
using generation_internal::proof_fact_set;
using generation_internal::read_generation_rule_refs;
using generation_internal::read_text_file;
using generation_internal::required_string;
using generation_internal::resolve_generation_ir_path;
using generation_internal::rules_from_location_rule_collection;
using generation_internal::sha256_hex;
using generation_internal::slot_entity_key;
using generation_internal::starting_facts_from_logic_rules;
using generation_internal::string_contains;
using generation_internal::string_from_number_or_string;
using generation_internal::truthy_json_flag;
using generation_internal::utc_timestamp_now;
using generation_internal::verify_generated_seed_package_contracts;
using generation_internal::write_json_file;

void require_surface_for_slot(const GenerationSlotRequest& slot,
                              const std::vector<LinkedWorldGenerationSurface>& linkedworlds) {
  const auto found = std::find_if(linkedworlds.begin(), linkedworlds.end(), [&](const auto& surface) {
    return surface.linkedworld_id == slot.linkedworld_id;
  });
  if (found == linkedworlds.end()) {
    throw std::runtime_error("missing_linkedworld_generation_surface:" + slot.linkedworld_id);
  }
  if (!slot.game_key.empty() && found->game_key != slot.game_key) {
    throw std::runtime_error("linkedworld_game_key_mismatch:" + slot.linkedworld_id);
  }
}

const LinkedWorldGenerationSurface& surface_for_slot(const GenerationSlotRequest& slot,
                                                     const std::vector<LinkedWorldGenerationSurface>& linkedworlds) {
  const auto found = std::find_if(linkedworlds.begin(), linkedworlds.end(), [&](const auto& surface) {
    return surface.linkedworld_id == slot.linkedworld_id;
  });
  if (found == linkedworlds.end()) {
    throw std::runtime_error("missing_linkedworld_generation_surface:" + slot.linkedworld_id);
  }
  if (!slot.game_key.empty() && found->game_key != slot.game_key) {
    throw std::runtime_error("linkedworld_game_key_mismatch:" + slot.linkedworld_id);
  }
  return *found;
}

bool is_server_first_package_surface(const LinkedWorldGenerationSurface& surface) {
  const auto mode = patch_mode_for_surface(surface);
  return mode == "server_dispatch" &&
         surface.capabilities.can_validate_options &&
         surface.capabilities.can_emit_patch &&
         surface.capabilities.can_emit_room_contract &&
         surface.capabilities.external_tools_required.empty() &&
         surface.capabilities.unsupported_options.empty();
}

std::vector<std::string> missing_seed_package_capabilities(const LinkedWorldGenerationSurface& surface) {
  if (is_server_first_package_surface(surface)) {
    return {};
  }
  return missing_required_generation_capabilities(surface.capabilities);
}

std::vector<std::string> missing_seed_package_surface_requirements(const LinkedWorldGenerationSurface& surface) {
  if (!is_server_first_package_surface(surface)) {
    return missing_required_generation_surface_requirements(surface);
  }

  std::vector<std::string> missing;
  const auto mode = patch_mode_for_surface(surface);
  if (!is_supported_patch_mode(mode)) {
    missing.push_back("unsupported_patch_mode");
  }
  if (mode == "server_dispatch") {
    const auto dispatch = patch_manifest_patch_for_surface(surface).value("server_dispatch",
                                                                          nlohmann::json::object());
    if (!dispatch.is_object() || !truthy_json_flag(dispatch.value("enabled", false))) {
      missing.push_back("server_dispatch_contract_incomplete");
    }
    for (const auto* key : {"target", "transport", "payload_ref"}) {
      if (!dispatch.contains(key) || !dispatch.at(key).is_string() || dispatch.at(key).get<std::string>().empty()) {
        missing.push_back(std::string{"server_dispatch_missing_"} + key);
      }
    }
  }
  return missing;
}

bool is_supported_progressive_effect(const nlohmann::json& effect) {
  auto levels = effect.contains("stages") ? effect.at("stages") : effect.value("levels", nlohmann::json::array());
  if (levels.empty() && effect.contains("stage_grants") && effect.at("stage_grants").is_object()) {
    levels = effect.at("stage_grants").value("stages", nlohmann::json::array());
  }
  if (!levels.is_array() || levels.empty()) {
    return false;
  }
  return std::all_of(levels.begin(), levels.end(), [](const auto& level) {
    return level.is_object() && (!level.contains("grants") || level.at("grants").is_array());
  });
}

bool is_supported_count_effect(const nlohmann::json& effect) {
  if (!effect.contains("count_grants") || !effect.at("count_grants").is_object()) {
    return false;
  }
  const auto thresholds = effect.at("count_grants").value("thresholds", nlohmann::json::array());
  if (!thresholds.is_array() || thresholds.empty()) {
    return false;
  }
  return std::all_of(thresholds.begin(), thresholds.end(), [](const auto& threshold) {
    return threshold.is_object() && threshold.contains("count") && threshold.at("count").is_number_integer() &&
           threshold.at("count").template get<int>() > 0 &&
           (!threshold.contains("grants") || threshold.at("grants").is_array());
  });
}

bool has_unsupported_progressive_effects(const nlohmann::json& logic_rules) {
  if (!logic_rules.is_object() || !logic_rules.contains("item_effects")) {
    return false;
  }
  auto effects = logic_rules.at("item_effects");
  if (effects.is_object() && effects.contains("effects") && effects.at("effects").is_object()) {
    effects = effects.at("effects");
  }
  if (!effects.is_object()) {
    return false;
  }
  for (const auto& [unused, effect] : effects.items()) {
    if (!effect.is_object()) {
      continue;
    }
    const auto type = effect.value("type", std::string{});
    if (type == "progressive" || type == "progressive_counter" || type == "levelled" ||
        effect.contains("stage_grants")) {
      if (!is_supported_progressive_effect(effect)) {
        return true;
      }
      continue;
    }
    if (effect.contains("count_grants")) {
      if (!is_supported_count_effect(effect)) {
        return true;
      }
      continue;
    }
    if (effect.contains("grant_by_count") || effect.contains("grant_by_level") ||
        effect.contains("progressive_grants")) {
      return true;
    }
    if ((effect.contains("stages") || effect.contains("levels")) && !is_supported_progressive_effect(effect)) {
      return true;
    }
  }
  return false;
}

bool allows_catalog_requires_merge(const nlohmann::json& rule, const nlohmann::json& locations_surface) {
  return object_flag_is_true(rule, "combines_with_catalog_requires") ||
         object_flag_is_true(rule, "allow_catalog_requires_merge") ||
         object_flag_is_true(locations_surface, "combines_with_catalog_requires") ||
         object_flag_is_true(locations_surface, "allow_catalog_requires_merge") ||
         rule.value("merge_policy", std::string{}) == "additive" ||
         locations_surface.value("merge_policy", std::string{}) == "additive";
}

void validate_location_rule_guardrails(const LinkedWorldGenerationSurface& surface,
                                       const nlohmann::json& logic_rules,
                                       std::vector<std::string>& missing) {
  const auto location_rules = logic_location_rules_array(logic_rules);
  std::map<std::string, nlohmann::json> first_rule_by_location;
  std::set<std::string> duplicate_locations;
  std::set<std::string> conflicting_locations;
  for (const auto& rule : location_rules) {
    const auto location_id = location_rule_id(rule);
    if (location_id.empty()) {
      continue;
    }
    const auto found = first_rule_by_location.find(location_id);
    if (found == first_rule_by_location.end()) {
      first_rule_by_location[location_id] = rule;
      continue;
    }
    duplicate_locations.insert(location_id);
    if (found->second.dump() != rule.dump()) {
      conflicting_locations.insert(location_id);
    }
  }
  if (!duplicate_locations.empty()) {
    push_unique(missing, "duplicate_location_rules");
  }
  if (!conflicting_locations.empty()) {
    push_unique(missing, "conflicting_location_rules");
  }

  const auto raw_locations_surface = logic_rules.value("locations", nlohmann::json::object());
  const auto locations_surface = raw_locations_surface.is_object() ? raw_locations_surface : nlohmann::json::object();
  for (const auto& location : surface.catalog.value("locations", nlohmann::json::array())) {
    const auto location_id = first_non_empty({
        json_field_as_string(location, "id"),
        json_field_as_string(location, "location_id"),
    });
    if (location_id.empty() || !location.contains("requires")) {
      continue;
    }
    for (const auto& rule : find_rules_for_id(location_rules, location_id)) {
      if (rule.contains("requires") && rule.at("requires") != location.at("requires") &&
          !allows_catalog_requires_merge(rule, locations_surface)) {
        push_unique(missing, "catalog_location_rule_requires_conflict");
        return;
      }
    }
  }
}

void validate_optional_location_rule_fact_names(const nlohmann::json& logic_rules,
                                                std::vector<std::string>& missing) {
  if (!logic_rules.contains("fact_names") || !logic_rules.at("fact_names").is_array()) {
    return;
  }
  const auto declared_facts = json_string_set(logic_rules.at("fact_names"));
  std::set<std::string> referenced_facts;
  for (const auto& rule : logic_location_refinement_rules_array(logic_rules)) {
    collect_fact_references(rule.value("requires", nlohmann::json{{"op", "true"}}), referenced_facts);
  }
  for (const auto& rule : logic_segmented_location_rules_array(logic_rules)) {
    collect_fact_references(rule.value("requires", nlohmann::json{{"op", "true"}}), referenced_facts);
  }
  for (const auto& fact : referenced_facts) {
    if (declared_facts.count(fact) == 0) {
      push_unique(missing, "location_rule_unknown_fact_name");
      return;
    }
  }
}

std::string native_fact_graph_id(const nlohmann::json& value) {
  if (!value.is_object()) {
    return {};
  }
  const auto proof = native_proof_object(value);
  return first_non_empty({
      json_field_as_string(proof, "fact_graph_id"),
      json_field_as_string(value, "fact_graph_id"),
  });
}

std::string logic_fact_graph_id(const nlohmann::json& logic_rules) {
  if (!logic_rules.is_object()) {
    return {};
  }
  const auto graph = logic_rules.value("fact_graph", nlohmann::json::object());
  const auto region_graph = logic_region_graph_object(logic_rules);
  return first_non_empty({
      json_field_as_string(graph, "id"),
      json_field_as_string(graph, "fact_graph_id"),
      native_fact_graph_id(region_graph),
      json_field_as_string(logic_rules, "fact_graph_id"),
  });
}

std::size_t location_surface_missing_fact_proof_count(const nlohmann::json& surface) {
  if (!native_proof_required(surface)) {
    return 0;
  }
  const auto proof = native_proof_object(surface);
  const auto referenced_facts = facts_referenced_by_location_surface(surface);
  const auto consumed_facts = proof_fact_set(proof, "consumed_facts");
  const auto produced_facts = proof_fact_set(proof, "produced_facts");
  return missing_fact_membership_count(referenced_facts, consumed_facts) +
         missing_fact_membership_count(referenced_facts, produced_facts);
}

bool location_surface_native_proof_complete(const nlohmann::json& logic_rules,
                                            const nlohmann::json& surface) {
  if (!native_proof_required(surface)) {
    return true;
  }
  const auto proof = native_proof_object(surface);
  const auto logic_graph_id = logic_fact_graph_id(logic_rules);
  const auto proof_graph_id = native_fact_graph_id(surface);
  const auto same_graph = logic_graph_id.empty() || proof_graph_id.empty() || logic_graph_id == proof_graph_id;
  return same_graph && native_proof_consumed(proof) &&
         location_surface_missing_fact_proof_count(surface) == 0;
}

std::size_t segmentation_missing_coverage_count(const nlohmann::json& segmentation) {
  if (!segmentation.is_object()) {
    return 0;
  }
  const auto proof = native_proof_object(segmentation);
  const auto coverage = !proof.empty() ? proof.value("coverage", segmentation.value("coverage", nlohmann::json::object()))
                                       : segmentation.value("coverage", nlohmann::json::object());
  if (!coverage.is_object()) {
    return 0;
  }
  const auto uncovered = coverage.value("uncovered_location_ids", nlohmann::json::array());
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

void validate_optional_logic_surface_authorization(const nlohmann::json& logic_rules,
                                                   const std::string& key,
                                                   const std::string& blocker_prefix,
                                                   const nlohmann::json& native_probe_report,
                                                   std::vector<std::string>& missing) {
  if (!logic_rules.is_object() || !logic_rules.contains(key)) {
    return;
  }
  if (!logic_rules.at(key).is_object()) {
    push_unique(missing, blocker_prefix + "_invalid");
    return;
  }
  const auto& surface = logic_rules.at(key);
  const auto proof_complete = location_surface_native_proof_complete(logic_rules, surface);
  const auto proof_authorizes =
      native_proof_required(surface) && proof_complete && native_proof_authorizes_logic(native_proof_object(surface));
  const auto surface_authorizes = explicitly_authorizes_native_logic_solve(surface);
  const auto probe_authorizes =
      native_probe_authorized_consumed_surface(native_probe_report, canonical_probe_surface(key), surface,
                                               surface_authorizes);
  if (has_not_consumed_candidate_surface(surface)) {
    push_unique(missing, blocker_prefix + "_not_consumed");
  }
  if (!probe_authorizes && !surface_authorizes && !proof_authorizes) {
    push_unique(missing, blocker_prefix + "_not_authorizing");
  }
  if (!probe_authorizes && native_proof_required(surface) && !proof_complete) {
    push_unique(missing, blocker_prefix + "_not_authorizing");
  }
  if (!probe_authorizes && native_location_rule_execution_failure_count(logic_rules, surface) > 0) {
    push_unique(missing, blocker_prefix + "_not_authorizing");
  }
  if (!probe_authorizes && key == "location_rule_segmentation" && segmentation_missing_coverage_count(surface) > 0) {
    push_unique(missing, blocker_prefix + "_not_authorizing");
  }
}

void validate_declared_logic_contract_surface(const nlohmann::json& parent,
                                              const std::string& key,
                                              const std::string& invalid_reason,
                                              const std::string& blocker_reason,
                                              std::vector<std::string>& missing) {
  if (!parent.is_object() || !parent.contains(key)) {
    return;
  }
  if (!parent.at(key).is_object()) {
    push_unique(missing, invalid_reason);
    return;
  }
  if (blocks_native_logic_contract(parent.at(key))) {
    push_unique(missing, blocker_reason);
  }
}

void validate_declared_placement_contract_surface(const nlohmann::json& parent,
                                                  const std::string& key,
                                                  const std::string& invalid_reason,
                                                  const std::string& blocker_reason,
                                                  std::vector<std::string>& missing) {
  if (!parent.is_object() || !parent.contains(key)) {
    return;
  }
  if (!parent.at(key).is_object()) {
    push_unique(missing, invalid_reason);
    return;
  }
  const auto blocks_contract = key == "dungeon_key_policy"
      ? blocks_native_dungeon_key_policy_contract(parent.at(key))
      : blocks_native_placement_contract(parent.at(key));
  if (blocks_contract) {
    push_unique(missing, blocker_reason);
  }
}

nlohmann::json dungeon_key_rules(const nlohmann::json& binding) {
  if (!binding.is_object()) {
    return nlohmann::json::array();
  }
  if (binding.contains("blocked_big_chest_rules") && binding.at("blocked_big_chest_rules").is_array()) {
    return binding.at("blocked_big_chest_rules");
  }
  if (binding.contains("requirements") && binding.at("requirements").is_array()) {
    return binding.at("requirements");
  }
  return nlohmann::json::array();
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
  std::set<std::string> required_locations;
  for (const auto& rule : dungeon_key_rules(binding)) {
    const auto location_id = first_non_empty({
        json_field_as_string(rule, "location_id"),
        json_field_as_string(rule, "location"),
        json_field_as_string(rule, "id"),
    });
    if (!location_id.empty()) {
      required_locations.insert(location_id);
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
      if (string_contains(role, "small_key") || string_contains(type, "small_key")) {
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

bool dungeon_key_policy_native_proof_complete(const nlohmann::json& policy,
                                              const nlohmann::json& binding) {
  const auto proof_required = native_proof_required(policy) || native_proof_required(binding);
  if (!proof_required) {
    return true;
  }
  const auto policy_proof = native_proof_object(policy);
  const auto binding_proof = native_proof_object(binding);
  return native_proof_consumed(policy_proof) &&
         native_proof_consumed(binding_proof) &&
         dungeon_key_policy_missing_small_key_proof_count(policy) == 0 &&
         dungeon_key_policy_missing_self_lock_proof_count(binding) == 0;
}

nlohmann::json proof_json_array(const nlohmann::json& proof, const std::string& key) {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& fact : proof_fact_set(proof, key)) {
    out.push_back(fact);
  }
  return out;
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

nlohmann::json native_replacement_edge_execution_report(const nlohmann::json& logic_rules) {
  const auto region_graph = logic_region_graph_object(logic_rules);
  std::size_t total = 0;
  std::size_t passed = 0;
  nlohmann::json failures = nlohmann::json::array();
  for (const auto& edge : consumed_region_graph_edges(region_graph)) {
    if (!edge.is_object() || !native_proof_required(edge)) {
      continue;
    }
    ++total;
    auto executable_edge = edge;
    const auto declared_edges = region_graph.value("edges", nlohmann::json::array());
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
    const auto effective_graph = effective_region_graph_object(region_graph);
    derive_region_reachability_facts(effective_graph, facts);
    const auto from_region = region_graph_edge_from(executable_edge);
    const auto to_region = region_graph_edge_to(executable_edge);
    const auto from_reachable = !from_region.empty() && facts.count(region_reachability_fact(from_region)) > 0;
    const auto requirement_met =
        evaluate_logic_expr(executable_edge.value("requires", nlohmann::json{{"op", "true"}}), facts);
    const auto to_reachable = !to_region.empty() && facts.count(region_reachability_fact(to_region)) > 0;
    const auto edge_authorizes = authorizes_consumed_region_graph_edge(executable_edge);
    if (edge_authorizes && from_reachable && requirement_met && to_reachable) {
      ++passed;
    } else if (failures.size() < 12) {
      failures.push_back({
          {"edge_id", region_graph_edge_id(edge)},
          {"from_region", from_region},
          {"to_region", to_region},
          {"authorizes", edge_authorizes},
          {"from_reachable", from_reachable},
          {"requirement_met", requirement_met},
          {"to_reachable", to_reachable},
      });
    }
  }
  const auto failed = total - passed;
  return {
      {"status", total == 0 ? "not_required" : (failed == 0 ? "passed" : "failed")},
      {"required_count", total},
      {"passed_count", passed},
      {"failed_count", failed},
      {"failures", failures},
  };
}

std::size_t native_location_rule_execution_failure_count(const nlohmann::json& logic_rules,
                                                         const nlohmann::json& surface) {
  if (!native_proof_required(surface)) {
    return 0;
  }
  const auto proof = native_proof_object(surface);
  auto facts = native_proof_execution_facts(logic_rules, proof);
  derive_region_reachability_facts(logic_region_graph_object(logic_rules), facts);
  std::size_t failed = 0;
  for (const auto& rule : rules_from_location_rule_collection(surface)) {
    if (!evaluate_logic_expr(rule.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
      ++failed;
    }
  }
  const auto segments = surface.is_object()
      ? surface.value("segments", nlohmann::json::array())
      : nlohmann::json::array();
  if (segments.is_array()) {
    for (const auto& segment : segments) {
      if (!segment.is_object()) {
        continue;
      }
      if (!evaluate_logic_expr(segment.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
        const auto locations = segment.contains("location_ids")
            ? segment.at("location_ids")
            : segment.value("locations", nlohmann::json::array());
        failed += locations.is_array() && !locations.empty() ? locations.size() : 1;
      }
      for (const auto& rule : rules_from_location_rule_collection(segment)) {
        if (!evaluate_logic_expr(rule.value("requires", nlohmann::json{{"op", "true"}}), facts)) {
          ++failed;
        }
      }
    }
  }
  return failed;
}

nlohmann::json native_location_surface_execution_report(const nlohmann::json& logic_rules,
                                                        const nlohmann::json& surface) {
  if (!native_proof_required(surface)) {
    return {
        {"status", "not_required"},
        {"required_count", 0},
        {"passed_count", 0},
        {"failed_count", 0},
      };
  }
  std::size_t total = rules_from_location_rule_collection(surface).size();
  const auto segments = surface.is_object()
      ? surface.value("segments", nlohmann::json::array())
      : nlohmann::json::array();
  if (segments.is_array()) {
    for (const auto& segment : segments) {
      if (!segment.is_object()) {
        continue;
      }
      const auto locations = segment.contains("location_ids")
          ? segment.at("location_ids")
          : segment.value("locations", nlohmann::json::array());
      total += locations.is_array() && !locations.empty() ? locations.size() : 1;
      total += rules_from_location_rule_collection(segment).size();
    }
  }
  const auto failed = native_location_rule_execution_failure_count(logic_rules, surface);
  return {
      {"status", failed == 0 ? "passed" : "failed"},
      {"required_count", total},
      {"passed_count", total >= failed ? total - failed : 0},
      {"failed_count", failed},
  };
}

std::set<std::string> catalog_location_ids(const nlohmann::json& catalog) {
  std::set<std::string> ids;
  const auto locations = catalog.value("locations", nlohmann::json::array());
  if (!locations.is_array()) {
    return ids;
  }
  for (const auto& location : locations) {
    const auto id = first_non_empty({
        json_field_as_string(location, "id"),
        json_field_as_string(location, "location_id"),
    });
    if (!id.empty()) {
      ids.insert(id);
    }
  }
  return ids;
}

nlohmann::json key_pool_items_array(const nlohmann::json& policy) {
  const auto pool = policy.value("separate_key_pool", nlohmann::json::array());
  if (pool.is_array()) {
    return pool;
  }
  if (pool.is_object() && pool.contains("items") && pool.at("items").is_array()) {
    return pool.at("items");
  }
  return nlohmann::json::array();
}

nlohmann::json item_with_role_tags(nlohmann::json item) {
  std::set<std::string> tags = json_string_set(item.value("tags", nlohmann::json::array()));
  for (const auto& key : {"role", "type", "classification"}) {
    const auto value = item.value(key, std::string{});
    if (!value.empty()) {
      tags.insert(value);
    }
  }
  nlohmann::json tag_array = nlohmann::json::array();
  for (const auto& tag : tags) {
    tag_array.push_back(tag);
  }
  item["tags"] = std::move(tag_array);
  if (!item.contains("classification")) {
    item["classification"] = item.value("role", std::string{"unknown"});
  }
  return item;
}

bool has_native_placement_candidate_for_item(const nlohmann::json& catalog,
                                             const nlohmann::json& logic_rules,
                                             const nlohmann::json& placement_rules,
                                             const nlohmann::json& item) {
  for (const auto& location : filter_fillable_locations(catalog.value("locations", nlohmann::json::array()),
                                                        placement_rules)) {
    const auto location_id = json_field_as_string(location, "id");
    auto enriched = enrich_location_with_generation_rules(location, logic_rules, placement_rules);
    if (location_id.empty() || !placement_allows_item(enriched, item_with_role_tags(item))) {
      continue;
    }
    return true;
  }
  return false;
}

nlohmann::json native_dungeon_key_policy_execution_report(const nlohmann::json& catalog,
                                                          const nlohmann::json& logic_rules,
                                                          const nlohmann::json& placement_rules) {
  const auto item_pool_source = catalog.value("item_pool_source", nlohmann::json::object());
  const auto policy = item_pool_source.is_object()
      ? item_pool_source.value("dungeon_key_policy", nlohmann::json::object())
      : nlohmann::json::object();
  const auto binding = logic_rules.is_object()
      ? logic_rules.value("dungeon_key_policy_binding", nlohmann::json::object())
      : nlohmann::json::object();
  if (!native_proof_required(policy) && !native_proof_required(binding)) {
    return {
        {"status", "not_required"},
        {"required_count", 0},
        {"passed_count", 0},
        {"failed_count", 0},
        {"small_key_placement_failed_count", 0},
        {"self_lock_failed_count", 0},
    };
  }
  std::size_t required = 0;
  std::size_t failed = 0;
  std::size_t small_key_failed = 0;
  std::size_t self_lock_failed = 0;
  const auto policy_proof = native_proof_object(policy);
  const auto binding_proof = native_proof_object(binding);
  const auto proved_items = proof_fact_set(policy_proof.value("small_key_proof", nlohmann::json::object()),
                                           "proved_item_ids");
  for (const auto& item : key_pool_items_array(policy)) {
    const auto role = item.value("role", std::string{});
    const auto type = item.value("type", std::string{});
    if (!string_contains(role, "small_key") && !string_contains(type, "small_key")) {
      continue;
    }
    ++required;
    const auto item_id = first_non_empty({
        json_field_as_string(item, "id"),
        json_field_as_string(item, "item_id"),
    });
    const auto ok = !item_id.empty() && proved_items.count(item_id) > 0 &&
                    has_native_placement_candidate_for_item(catalog, logic_rules, placement_rules, item);
    if (!ok) {
      ++failed;
      ++small_key_failed;
    }
  }
  const auto proved_locations = proof_fact_set(binding_proof.value("self_lock_proof", nlohmann::json::object()),
                                               "proved_location_ids");
  const auto known_locations = catalog_location_ids(catalog);
  for (const auto& rule : dungeon_key_rules(binding)) {
    ++required;
    const auto location_id = first_non_empty({
        json_field_as_string(rule, "location_id"),
        json_field_as_string(rule, "location"),
        json_field_as_string(rule, "id"),
    });
    const auto ok = !location_id.empty() && proved_locations.count(location_id) > 0 &&
                    (known_locations.empty() || known_locations.count(location_id) > 0);
    if (!ok) {
      ++failed;
      ++self_lock_failed;
    }
  }
  return {
      {"status", failed == 0 ? "passed" : "failed"},
      {"required_count", required},
      {"passed_count", required >= failed ? required - failed : 0},
      {"failed_count", failed},
      {"small_key_placement_failed_count", small_key_failed},
      {"self_lock_failed_count", self_lock_failed},
  };
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

nlohmann::json native_probe_generated_checks(const nlohmann::json& catalog,
                                             const nlohmann::json& logic_rules,
                                             const nlohmann::json& placement_rules) {
  nlohmann::json checks = nlohmann::json::array();
  for (const auto& location : filter_fillable_locations(catalog.value("locations", nlohmann::json::array()),
                                                        placement_rules)) {
    checks.push_back(enrich_location_with_generation_rules(location, logic_rules, placement_rules));
  }
  return checks;
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

nlohmann::json find_probe_check(const nlohmann::json& checks, const std::string& location_id) {
  if (!checks.is_array()) {
    return nlohmann::json::object();
  }
  for (const auto& check : checks) {
    if (json_field_as_string(check, "id") == location_id ||
        json_field_as_string(check, "location_id") == location_id) {
      return check;
    }
  }
  return nlohmann::json::object();
}

nlohmann::json find_probe_item(const nlohmann::json& catalog,
                               const nlohmann::json& logic_rules,
                               const std::string& item_id) {
  nlohmann::json items = catalog.value("item_pool", nlohmann::json::array());
  const auto item_pool_source = catalog.value("item_pool_source", nlohmann::json::object());
  const auto policy = item_pool_source.is_object()
      ? item_pool_source.value("dungeon_key_policy", nlohmann::json::object())
      : nlohmann::json::object();
  for (const auto& item : key_pool_items_array(policy)) {
    items.push_back(item);
  }
  for (const auto& item : items) {
    const auto id = first_non_empty({
        json_field_as_string(item, "id"),
        json_field_as_string(item, "item_id"),
        json_field_as_string(item, "semantic_id"),
    });
    if (id == item_id || base_instance_id(id) == item_id) {
      return enrich_item_with_generation_rules(item, logic_rules);
    }
  }
  return nlohmann::json::object();
}

bool native_probe_item_placeable(const nlohmann::json& checks, const nlohmann::json& item) {
  if (!checks.is_array() || item.empty()) {
    return false;
  }
  for (const auto& check : checks) {
    if (placement_allows_item(check, item_with_role_tags(item))) {
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
                                   const nlohmann::json& contract) {
  auto facts = native_probe_contract_facts(logic_rules, contract);
  const auto region_graph = effective_region_graph_object(logic_region_graph_object(logic_rules));
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
  for (const auto& region : json_string_array(contract.value("expected_unreachable_regions", nlohmann::json::array()))) {
    if (facts.count(region_reachability_fact(region)) > 0) {
      return false;
    }
  }
  const auto checks = native_probe_generated_checks(catalog, logic_rules, placement_rules);
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
    if (!native_probe_item_placeable(checks, find_probe_item(catalog, logic_rules, item_id))) {
      return false;
    }
  }
  const auto family = first_non_empty({
      json_field_as_string(contract, "family"),
      json_field_as_string(contract, "surface"),
      json_field_as_string(contract, "type"),
  });
  if (family == "dungeon_key_policy" || family == "dungeon_key" || family == "key_policy") {
    return native_dungeon_key_policy_execution_report(catalog,
                                                      logic_rules,
                                                      placement_rules).value("failed_count", 0) == 0;
  }
  return true;
}

nlohmann::json native_probe_contract_execution_report(const nlohmann::json& catalog,
                                                      const nlohmann::json& logic_rules,
                                                      const nlohmann::json& placement_rules,
                                                      const nlohmann::json& surface_logic) {
  nlohmann::json contracts = nlohmann::json::array();
  collect_native_probe_contracts(surface_logic, contracts);
  collect_native_probe_contracts(logic_rules, contracts);
  collect_native_probe_contracts(placement_rules, contracts);
  collect_native_probe_contracts(catalog, contracts);
  std::size_t passed = 0;
  nlohmann::json failures = nlohmann::json::array();
  std::map<std::string, nlohmann::json> by_surface_map;
  for (const auto& contract : contracts) {
    const auto surface_id = native_probe_contract_surface(contract).empty()
        ? std::string{"unknown"}
        : native_probe_contract_surface(contract);
    auto& surface_report = by_surface_map[surface_id];
    if (surface_report.is_null()) {
      surface_report = {{"contract_count", 0}, {"pass_count", 0}, {"fail_count", 0}};
    }
    surface_report["contract_count"] = surface_report.value("contract_count", 0) + 1;
    if (execute_native_probe_contract(catalog, logic_rules, placement_rules, contract)) {
      ++passed;
      surface_report["pass_count"] = surface_report.value("pass_count", 0) + 1;
    } else {
      surface_report["fail_count"] = surface_report.value("fail_count", 0) + 1;
      if (failures.size() < 12) {
        failures.push_back({
            {"id", first_non_empty({json_field_as_string(contract, "id"),
                                    json_field_as_string(contract, "contract_id")})},
            {"family", first_non_empty({json_field_as_string(contract, "family"),
                                        json_field_as_string(contract, "surface"),
                                        json_field_as_string(contract, "type")})},
        });
      }
    }
  }
  const auto failed = contracts.size() - passed;
  nlohmann::json by_surface = nlohmann::json::object();
  for (auto& [surface_id, surface_report] : by_surface_map) {
    surface_report["status"] = surface_report.value("fail_count", 0) == 0 ? "passed" : "failed";
    by_surface[surface_id] = surface_report;
  }
  return {
      {"status", contracts.empty() ? "not_declared" : (failed == 0 ? "passed" : "failed")},
      {"contract_count", contracts.size()},
      {"pass_count", passed},
      {"fail_count", failed},
      {"by_surface", by_surface},
      {"failures", failures},
  };
}

bool native_probe_surface_passed(const nlohmann::json& report, const std::string& surface_id) {
  const auto by_surface = report.value("by_surface", nlohmann::json::object());
  if (!by_surface.is_object() || !by_surface.contains(surface_id) || !by_surface.at(surface_id).is_object()) {
    return false;
  }
  const auto& surface_report = by_surface.at(surface_id);
  return surface_report.value("contract_count", 0) > 0 &&
         surface_report.value("fail_count", 0) == 0;
}

bool native_probe_authorized_consumed_surface(const nlohmann::json& report,
                                              const std::string& surface_id,
                                              const nlohmann::json& surface,
                                              bool explicitly_authorizes) {
  return native_probe_surface_passed(report, surface_id) &&
         explicitly_consumed_generation_surface(surface) &&
         explicitly_authorizes;
}

bool native_probe_surface_can_materialize_generation_facts(const std::string& surface_id) {
  static const std::set<std::string> kMaterializingSurfaces = {
      "region_graph",
      "location_refinements",
      "per_location_refinements",
      "location_rule_refinements",
      "location_rule_segmentation",
      "dungeon_key_policy",
      "placement",
  };
  return kMaterializingSurfaces.count(surface_id) > 0;
}

void insert_proof_generation_facts(const nlohmann::json& proof, std::set<std::string>& facts) {
  for (const auto& fact : proof_fact_set(proof, "produced_facts")) {
    facts.insert(fact);
  }
  for (const auto& fact : proof_fact_set(proof, "consumed_facts")) {
    facts.insert(fact);
  }
}

void insert_authorized_surface_generation_facts(const nlohmann::json& native_probe_report,
                                                const std::string& surface_id,
                                                const nlohmann::json& surface,
                                                std::set<std::string>& facts) {
  if (!native_probe_authorized_consumed_surface(native_probe_report,
                                                surface_id,
                                                surface,
                                                explicitly_authorizes_native_logic_solve(surface))) {
    return;
  }
  insert_proof_generation_facts(native_proof_object(surface), facts);
}

std::vector<std::string> authorized_native_generation_facts(const nlohmann::json& catalog,
                                                            const nlohmann::json& logic_rules,
                                                            const nlohmann::json& placement_rules,
                                                            const nlohmann::json& surface_logic) {
  std::set<std::string> facts;
  const auto native_probe_report =
      native_probe_contract_execution_report(catalog, logic_rules, placement_rules, surface_logic);
  if (native_probe_report.value("fail_count", 0) > 0) {
    return {};
  }

  nlohmann::json contracts = nlohmann::json::array();
  collect_native_probe_contracts(surface_logic, contracts);
  collect_native_probe_contracts(logic_rules, contracts);
  collect_native_probe_contracts(placement_rules, contracts);
  collect_native_probe_contracts(catalog, contracts);
  for (const auto& contract : contracts) {
    const auto surface_id = native_probe_contract_surface(contract);
    if (!native_probe_surface_can_materialize_generation_facts(surface_id) ||
        !native_probe_surface_passed(native_probe_report, surface_id) ||
        !execute_native_probe_contract(catalog, logic_rules, placement_rules, contract)) {
      continue;
    }
    for (const auto& fact : native_probe_contract_facts(logic_rules, contract)) {
      facts.insert(fact);
    }
  }

  const auto region_graph = logic_region_graph_object(logic_rules);
  if (native_probe_surface_passed(native_probe_report, "region_graph")) {
    for (const auto& edge : consumed_region_graph_edges(region_graph)) {
      if (authorizes_consumed_region_graph_edge(edge)) {
        insert_proof_generation_facts(native_proof_object(edge), facts);
      }
    }
  }

  if (logic_rules.is_object()) {
    insert_authorized_surface_generation_facts(native_probe_report,
                                               "location_refinements",
                                               logic_rules.value("location_refinements", nlohmann::json::object()),
                                               facts);
    insert_authorized_surface_generation_facts(native_probe_report,
                                               "per_location_refinements",
                                               logic_rules.value("per_location_refinements", nlohmann::json::object()),
                                               facts);
    insert_authorized_surface_generation_facts(native_probe_report,
                                               "location_rule_refinements",
                                               logic_rules.value("location_rule_refinements",
                                                                 nlohmann::json::object()),
                                               facts);
    insert_authorized_surface_generation_facts(native_probe_report,
                                               "location_rule_segmentation",
                                               logic_rules.value("location_rule_segmentation",
                                                                 nlohmann::json::object()),
                                               facts);
  }

  return {facts.begin(), facts.end()};
}

void validate_declared_generation_contract_surfaces(const LinkedWorldGenerationSurface& surface,
                                                    const nlohmann::json& logic_rules,
                                                    const nlohmann::json& placement_rules,
                                                    const nlohmann::json& native_probe_report,
                                                    std::vector<std::string>& missing) {
  const auto item_pool_source = surface.catalog.value("item_pool_source", nlohmann::json::object());
  const auto policy = item_pool_source.is_object()
      ? item_pool_source.value("dungeon_key_policy", nlohmann::json::object())
      : nlohmann::json::object();
  const auto binding = logic_rules.is_object()
      ? logic_rules.value("dungeon_key_policy_binding", nlohmann::json::object())
      : nlohmann::json::object();
  const auto dungeon_policy_probe_authorizes =
      native_probe_surface_passed(native_probe_report, "dungeon_key_policy") &&
      explicitly_consumed_generation_surface(policy) &&
      explicitly_consumed_generation_surface(binding) &&
      explicitly_authorizes_native_dungeon_key_policy_surface(policy) &&
      explicitly_authorizes_native_logic_solve(binding);
  if (item_pool_source.is_object() && item_pool_source.contains("dungeon_key_policy")) {
    if (!policy.is_object()) {
      push_unique(missing, "dungeon_key_policy_invalid");
    } else if (!dungeon_policy_probe_authorizes &&
               blocks_native_dungeon_key_policy_contract(policy)) {
      push_unique(missing, "dungeon_key_policy_not_consumed");
    }
  }
  if (logic_rules.is_object() && logic_rules.contains("dungeon_key_policy_binding")) {
    if (!binding.is_object()) {
      push_unique(missing, "dungeon_key_policy_binding_invalid");
    } else if (!dungeon_policy_probe_authorizes && blocks_native_logic_contract(binding)) {
      push_unique(missing, "dungeon_key_policy_not_consumed");
    }
  }
  if (!dungeon_policy_probe_authorizes) {
    if ((native_proof_required(policy) || native_proof_required(binding)) &&
        !dungeon_key_policy_native_proof_complete(policy, binding)) {
      push_unique(missing, "dungeon_key_policy_not_consumed");
    }
    if (native_dungeon_key_policy_execution_report(surface.catalog,
                                                  logic_rules,
                                                  placement_rules).value("failed_count", 0) > 0) {
      push_unique(missing, "dungeon_key_policy_not_consumed");
    }
  }
  if (native_probe_report.value("fail_count", 0) > 0) {
    push_unique(missing, "native_probe_contract_failed");
  }

  for (const auto& key : {"reward_contract", "reward_policy", "reward_gate_contract"}) {
    validate_declared_logic_contract_surface(logic_rules,
                                             key,
                                             "reward_contract_invalid",
                                             "reward_contract_not_consumed",
                                             missing);
    validate_declared_placement_contract_surface(placement_rules,
                                                 key,
                                                 "reward_contract_invalid",
                                                 "reward_contract_not_consumed",
                                                 missing);
  }
  for (const auto& key : {"medallion_contract", "medallion_policy"}) {
    validate_declared_logic_contract_surface(logic_rules,
                                             key,
                                             "medallion_contract_invalid",
                                             "medallion_contract_not_consumed",
                                             missing);
    validate_declared_placement_contract_surface(placement_rules,
                                                 key,
                                                 "medallion_contract_invalid",
                                                 "medallion_contract_not_consumed",
                                                 missing);
  }
  for (const auto& key : {"completion_contract", "completion_policy", "completion_rules_contract"}) {
    validate_declared_logic_contract_surface(logic_rules,
                                             key,
                                             "completion_contract_invalid",
                                             "completion_contract_not_consumed",
                                             missing);
    validate_declared_logic_contract_surface(surface.logic,
                                             key,
                                             "completion_contract_invalid",
                                             "completion_contract_not_consumed",
                                             missing);
  }
}

nlohmann::json build_deterministic_multiworld_placements(const GenerationPackageRequest& request,
                                                         const nlohmann::json& checks,
                                                         const nlohmann::json& items) {
  if (checks.empty()) {
    throw std::runtime_error("missing_generation_locations");
  }
  if (items.empty()) {
    throw std::runtime_error("missing_generation_items");
  }
  if (checks.size() != items.size()) {
    throw std::runtime_error("generation_catalog_count_mismatch:locations=" + std::to_string(checks.size()) +
                             ":items=" + std::to_string(items.size()));
  }

  auto ordered_checks =
      deterministic_ordered_array(checks, request.rng_seed, request.seed_id, "location");
  auto ordered_items = deterministic_ordered_array(items, request.rng_seed, request.seed_id, "item");

  if (request.slots.size() > 1 && ordered_items.size() > 1) {
    const auto first = ordered_items.front();
    ordered_items.erase(ordered_items.begin());
    ordered_items.push_back(first);
  }

  std::stable_sort(ordered_items.begin(), ordered_items.end(), [](const auto& left, const auto& right) {
    return left.value("advancement", false) > right.value("advancement", false);
  });

  std::map<int, std::set<std::string>> facts_by_slot;
  for (const auto& slot : request.slots) {
    facts_by_slot[slot.slot_id].insert("slot:" + std::to_string(slot.slot_id));
  }
  for (const auto& location : ordered_checks) {
    const auto slot_id = location.value("slot_id", 0);
    for (const auto& fact : json_string_array(location.value("logic_starting_facts", nlohmann::json::array()))) {
      facts_by_slot[slot_id].insert(fact);
    }
  }
  derive_region_reachability_facts_from_locations(ordered_checks, facts_by_slot);
  std::map<std::string, std::size_t> progressive_levels_by_slot;

  nlohmann::json placements = nlohmann::json::array();
  for (std::size_t check_index = 0; check_index < ordered_checks.size();) {
    const auto location = ordered_checks.at(check_index);
    if (!location.contains("preplacement")) {
      ++check_index;
      continue;
    }
    const auto preplacement = location.at("preplacement");
    const auto wanted_item = first_non_empty({
        json_field_as_string(preplacement, "item_id"),
        json_field_as_string(preplacement, "item"),
        json_field_as_string(preplacement, "item_name"),
        json_field_as_string(preplacement, "semantic_id"),
    });
    auto item_index = ordered_items.size();
    for (std::size_t candidate_index = 0; candidate_index < ordered_items.size(); ++candidate_index) {
      const auto& candidate = ordered_items.at(candidate_index);
      if (wanted_item == json_field_as_string(candidate, "id") ||
          wanted_item == base_instance_id(json_field_as_string(candidate, "id")) ||
          wanted_item == json_field_as_string(candidate, "item_id") ||
          wanted_item == json_field_as_string(candidate, "semantic_id") ||
          wanted_item == json_field_as_string(candidate, "name")) {
        item_index = candidate_index;
        break;
      }
    }
    if (item_index == ordered_items.size()) {
      throw std::runtime_error("missing_preplacement_item:" + wanted_item);
    }
    const auto item = ordered_items.at(item_index);
    if (!placement_allows_item(location, item)) {
      throw std::runtime_error("preplacement_violates_constraints:" + json_field_as_string(location, "id"));
    }
    const auto receiving_slot = item.value("slot_id", 0);
    const auto grants = collect_item_grants(item, receiving_slot, progressive_levels_by_slot);
    for (const auto& fact : json_string_array(grants)) {
      facts_by_slot[receiving_slot].insert(fact);
    }
    placements.push_back({
        {"placement_index", placements.size()},
        {"location_id", json_field_as_string(location, "id")},
        {"location_name", location.value("name", std::string{})},
        {"owning_slot", location.value("slot_id", 0)},
        {"receiving_slot", receiving_slot},
        {"item_id", json_field_as_string(item, "id")},
        {"item_name", item.value("name", std::string{})},
        {"classification", item.value("classification", std::string{"unknown"})},
        {"advancement", item.value("advancement", false)},
        {"progression_sphere", item.value("advancement", false) ? nlohmann::json(0) : nlohmann::json(nullptr)},
        {"preplaced", true},
        {"requires", location.value("requires", nlohmann::json{{"op", "true"}})},
        {"grants", grants},
        {"logic_starting_facts", location.value("logic_starting_facts", nlohmann::json::array())},
        {"source_linkedworld_ref", location.value("linkedworld_id", std::string{})},
        {"item_source_linkedworld_ref", item.value("linkedworld_id", std::string{})},
    });
    ordered_checks.erase(ordered_checks.begin() + static_cast<std::ptrdiff_t>(check_index));
    ordered_items.erase(ordered_items.begin() + static_cast<std::ptrdiff_t>(item_index));
  }

  std::size_t sphere = 0;
  while (!ordered_items.empty()) {
    derive_region_reachability_facts_from_locations(ordered_checks, facts_by_slot);
    bool placed_any = false;
    for (std::size_t item_index = 0; item_index < ordered_items.size(); ++item_index) {
      const auto item = ordered_items.at(item_index);
      auto location_index = ordered_checks.size();
      for (std::size_t candidate_index = 0; candidate_index < ordered_checks.size(); ++candidate_index) {
        const auto& candidate = ordered_checks.at(candidate_index);
        const auto owning_slot = candidate.value("slot_id", 0);
        if (!evaluate_logic_expr(candidate.value("requires", nlohmann::json{{"op", "true"}}),
                                 facts_by_slot[owning_slot])) {
          continue;
        }
        if (!placement_allows_item(candidate, item)) {
          continue;
        }
        const auto remaining_checks = json_without_index(ordered_checks, candidate_index);
        const auto remaining_items = json_without_index(ordered_items, item_index);
        if (should_run_exact_static_matching_guard(remaining_checks, remaining_items) &&
            !has_static_placement_matching(remaining_checks, remaining_items)) {
          continue;
        }
        location_index = candidate_index;
        break;
      }
      if (location_index == ordered_checks.size()) {
        continue;
      }

      const auto location = ordered_checks.at(location_index);
      const auto receiving_slot = item.value("slot_id", 0);
      const auto grants = collect_item_grants(item, receiving_slot, progressive_levels_by_slot);
      for (const auto& fact : json_string_array(grants)) {
        facts_by_slot[receiving_slot].insert(fact);
      }
      const auto placement_index = placements.size();
      placements.push_back({
          {"placement_index", placement_index},
          {"location_id", json_field_as_string(location, "id")},
          {"location_name", location.value("name", std::string{})},
          {"owning_slot", location.value("slot_id", 0)},
          {"receiving_slot", receiving_slot},
          {"item_id", json_field_as_string(item, "id")},
          {"item_name", item.value("name", std::string{})},
          {"classification", item.value("classification", std::string{"unknown"})},
          {"advancement", item.value("advancement", false)},
          {"progression_sphere", item.value("advancement", false) ? nlohmann::json(sphere) : nlohmann::json(nullptr)},
          {"preplaced", false},
          {"requires", location.value("requires", nlohmann::json{{"op", "true"}})},
          {"grants", grants},
          {"logic_starting_facts", location.value("logic_starting_facts", nlohmann::json::array())},
          {"source_linkedworld_ref", location.value("linkedworld_id", std::string{})},
          {"item_source_linkedworld_ref", item.value("linkedworld_id", std::string{})},
      });
      ordered_checks.erase(ordered_checks.begin() + static_cast<std::ptrdiff_t>(location_index));
      ordered_items.erase(ordered_items.begin() + static_cast<std::ptrdiff_t>(item_index));
      placed_any = true;
      break;
    }
    if (!placed_any) {
      // After all progression has been placed, remaining non-advancement items
      // may be filled by placement constraints only. Replay marks these as
      // post-logic fills so it does not treat inaccessible filler-only checks as
      // progression-sphere violations.
      if (all_items_are_non_advancement(ordered_items)) {
        while (!ordered_items.empty()) {
          const auto item = ordered_items.front();
          auto location_index = ordered_checks.size();
          for (std::size_t candidate_index = 0; candidate_index < ordered_checks.size(); ++candidate_index) {
            if (!placement_allows_item(ordered_checks.at(candidate_index), item)) {
              continue;
            }
            location_index = candidate_index;
            break;
          }
          if (location_index == ordered_checks.size()) {
            break;
          }
          const auto location = ordered_checks.at(location_index);
          const auto receiving_slot = item.value("slot_id", 0);
          const auto grants = collect_item_grants(item, receiving_slot, progressive_levels_by_slot);
          for (const auto& fact : json_string_array(grants)) {
            facts_by_slot[receiving_slot].insert(fact);
          }
          placements.push_back({
              {"placement_index", placements.size()},
              {"location_id", json_field_as_string(location, "id")},
              {"location_name", location.value("name", std::string{})},
              {"owning_slot", location.value("slot_id", 0)},
              {"receiving_slot", receiving_slot},
              {"item_id", json_field_as_string(item, "id")},
              {"item_name", item.value("name", std::string{})},
              {"classification", item.value("classification", std::string{"unknown"})},
              {"advancement", false},
              {"progression_sphere", nullptr},
              {"preplaced", false},
              {"post_logic_fill", true},
              {"requires", location.value("requires", nlohmann::json{{"op", "true"}})},
              {"grants", grants},
              {"logic_starting_facts", location.value("logic_starting_facts", nlohmann::json::array())},
              {"source_linkedworld_ref", location.value("linkedworld_id", std::string{})},
              {"item_source_linkedworld_ref", item.value("linkedworld_id", std::string{})},
          });
          ordered_checks.erase(ordered_checks.begin() + static_cast<std::ptrdiff_t>(location_index));
          ordered_items.erase(ordered_items.begin());
        }
        if (ordered_items.empty()) {
          break;
        }
      }
      throw std::runtime_error(
          "unsolved_logic_or_placement_constraints:" +
          placement_failure_diagnostics(ordered_checks, ordered_items, facts_by_slot).dump());
    }
    ++sphere;
  }
  return placements;
}

void validate_placements_replay(const GenerationPackageRequest& request,
                                const nlohmann::json& checks,
                                const nlohmann::json& items,
                                const nlohmann::json& placements) {
  if (placements.size() != checks.size() || placements.size() != items.size()) {
    throw std::runtime_error("replay_count_mismatch");
  }

  std::map<std::string, nlohmann::json> checks_by_key;
  for (const auto& check : checks) {
    const auto key = slot_entity_key(check.value("slot_id", 0), json_field_as_string(check, "id"));
    if (key.empty() || checks_by_key.count(key) > 0) {
      throw std::runtime_error("replay_duplicate_location:" + key);
    }
    checks_by_key[key] = check;
  }

  std::map<std::string, nlohmann::json> items_by_key;
  for (const auto& item : items) {
    const auto key = slot_entity_key(item.value("slot_id", 0), json_field_as_string(item, "id"));
    if (key.empty() || items_by_key.count(key) > 0) {
      throw std::runtime_error("replay_duplicate_item:" + key);
    }
    items_by_key[key] = item;
  }

  std::set<std::string> placed_location_keys;
  std::set<std::string> placed_item_keys;
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto& placement = placements.at(index);
    if (placement.value("placement_index", std::size_t{0}) != index) {
      throw std::runtime_error("replay_placement_index_mismatch");
    }
    const auto location_key =
        slot_entity_key(placement.value("owning_slot", 0), json_field_as_string(placement, "location_id"));
    const auto item_key =
        slot_entity_key(placement.value("receiving_slot", 0), json_field_as_string(placement, "item_id"));
    if (checks_by_key.count(location_key) == 0) {
      throw std::runtime_error("replay_unknown_location:" + location_key);
    }
    if (items_by_key.count(item_key) == 0) {
      throw std::runtime_error("replay_unknown_item:" + item_key);
    }
    if (placed_location_keys.count(location_key) > 0) {
      throw std::runtime_error("replay_location_placed_twice:" + location_key);
    }
    if (placed_item_keys.count(item_key) > 0) {
      throw std::runtime_error("replay_item_placed_twice:" + item_key);
    }
    if (!placement_allows_item(checks_by_key.at(location_key), items_by_key.at(item_key))) {
      throw std::runtime_error("replay_placement_constraint_failed:" + location_key);
    }
    placed_location_keys.insert(location_key);
    placed_item_keys.insert(item_key);
  }

  std::map<int, std::set<std::string>> facts_by_slot;
  for (const auto& slot : request.slots) {
    facts_by_slot[slot.slot_id].insert("slot:" + std::to_string(slot.slot_id));
  }
  for (const auto& check : checks) {
    const auto slot_id = check.value("slot_id", 0);
    for (const auto& fact : json_string_array(check.value("logic_starting_facts", nlohmann::json::array()))) {
      facts_by_slot[slot_id].insert(fact);
    }
  }
  derive_region_reachability_facts_from_locations(checks, facts_by_slot);
  std::map<std::string, std::size_t> progressive_levels_by_slot;

  for (std::size_t index = 0; index < placements.size(); ++index) {
    derive_region_reachability_facts_from_locations(checks, facts_by_slot);
    const auto& placement = placements.at(index);
    const auto owning_slot = placement.value("owning_slot", 0);
    const auto location_key =
        slot_entity_key(owning_slot, json_field_as_string(placement, "location_id"));
    const auto item_key =
        slot_entity_key(placement.value("receiving_slot", 0), json_field_as_string(placement, "item_id"));
    const auto& check = checks_by_key.at(location_key);
    const auto& item = items_by_key.at(item_key);
    if (!placement.value("post_logic_fill", false) &&
        !evaluate_logic_expr(check.value("requires", nlohmann::json{{"op", "true"}}), facts_by_slot[owning_slot])) {
      throw std::runtime_error("replay_validation_failed");
    }
    const auto receiving_slot = item.value("slot_id", 0);
    const auto grants = collect_item_grants(item, receiving_slot, progressive_levels_by_slot);
    if (placement.value("grants", nlohmann::json::array()) != grants) {
      throw std::runtime_error("replay_grants_mismatch:" + item_key);
    }
    for (const auto& fact : json_string_array(grants)) {
      facts_by_slot[receiving_slot].insert(fact);
    }
  }
}

}  // namespace

nlohmann::json to_json(const GenerationSlotRequest& slot) {
  nlohmann::json value = {
      {"slot_id", slot.slot_id},
      {"user_id", slot.user_id},
      {"display_name", slot.display_name},
      {"game_key", slot.game_key},
      {"linkedworld_id", slot.linkedworld_id},
      {"config_version_id", slot.config_version_id},
  };
  if (!slot.config_snapshot.empty()) {
    value["config_snapshot"] = slot.config_snapshot;
  }
  return value;
}

nlohmann::json to_json(const LinkedWorldGenerationCapability& capabilities) {
  return {
      {"can_validate_options", capabilities.can_validate_options},
      {"can_build_item_pool", capabilities.can_build_item_pool},
      {"can_solve_logic", capabilities.can_solve_logic},
      {"can_place_items", capabilities.can_place_items},
      {"can_emit_patch", capabilities.can_emit_patch},
      {"can_emit_room_contract", capabilities.can_emit_room_contract},
      {"external_tools_required", capabilities.external_tools_required},
      {"unsupported_options", capabilities.unsupported_options},
  };
}

nlohmann::json to_json(const LinkedWorldGenerationSurface& surface) {
  return {
      {"module_id", surface.module_id},
      {"game_key", surface.game_key},
      {"linkedworld_id", surface.linkedworld_id},
      {"version", surface.version},
      {"source_path", surface.source_path.string()},
      {"capabilities", to_json(surface.capabilities)},
      {"logic", surface.logic},
      {"catalog", surface.catalog},
      {"generation_rules", surface.generation_rules},
      {"patch", surface.patch},
      {"runtime", surface.runtime},
  };
}

std::optional<LinkedWorldGenerationSurface> load_linkedworld_generation_surface(
    const std::filesystem::path& linkedworld_root,
    std::string* error) {
  try {
    const auto manifest_path = linkedworld_root / "manifest" / "manifest.json";
    const auto manifest = nlohmann::json::parse(read_text_file(manifest_path));
    const auto ir_path = resolve_generation_ir_path(linkedworld_root, manifest);
    if (!std::filesystem::exists(ir_path)) {
      if (error) *error = "missing_generation_ir:" + ir_path.string();
      return std::nullopt;
    }
    const auto ir = nlohmann::json::parse(read_text_file(ir_path));
    const auto capabilities_json = ir.value("capabilities", nlohmann::json::object());

    LinkedWorldGenerationSurface surface;
    surface.module_id = manifest.value("module_id", ir.value("module_id", ""));
    surface.game_key = manifest.value("game_id", ir.value("game_key", ""));
    surface.linkedworld_id = ir.value("linkedworld_id", surface.game_key);
    surface.version = manifest.value("version", ir.value("version", ""));
    surface.source_path = ir_path;
    surface.capabilities.can_validate_options = capabilities_json.value("can_validate_options", false);
    surface.capabilities.can_build_item_pool = capabilities_json.value("can_build_item_pool", false);
    surface.capabilities.can_solve_logic = capabilities_json.value("can_solve_logic", false);
    surface.capabilities.can_place_items = capabilities_json.value("can_place_items", false);
    surface.capabilities.can_emit_patch = capabilities_json.value("can_emit_patch", false);
    surface.capabilities.can_emit_room_contract = capabilities_json.value("can_emit_room_contract", false);
    surface.capabilities.external_tools_required =
        capabilities_json.value("external_tools_required", std::vector<std::string>{});
    surface.capabilities.unsupported_options =
        capabilities_json.value("unsupported_options", std::vector<std::string>{});
    surface.logic = ir.value("logic", nlohmann::json::object());
    surface.catalog = ir.value("catalog", nlohmann::json::object());
    expand_catalog_refs(linkedworld_root, surface.catalog);
    surface.generation_rules = read_generation_rule_refs(linkedworld_root, surface.catalog);
    surface.patch = ir.value("patch", nlohmann::json::object());
    expand_patch_manifest_ref(linkedworld_root, surface.patch);
    surface.runtime = ir.value("runtime", nlohmann::json::object());
    return surface;
  } catch (const std::exception& exception) {
    if (error) *error = exception.what();
    return std::nullopt;
  }
}

std::vector<std::string> missing_required_generation_capabilities(
    const LinkedWorldGenerationCapability& capabilities) {
  std::vector<std::string> missing;
  if (!capabilities.can_validate_options) missing.push_back("can_validate_options");
  if (!capabilities.can_build_item_pool) missing.push_back("can_build_item_pool");
  if (!capabilities.can_solve_logic) missing.push_back("can_solve_logic");
  if (!capabilities.can_place_items) missing.push_back("can_place_items");
  if (!capabilities.can_emit_patch) missing.push_back("can_emit_patch");
  if (!capabilities.can_emit_room_contract) missing.push_back("can_emit_room_contract");
  if (!capabilities.external_tools_required.empty()) missing.push_back("external_tools_required");
  if (!capabilities.unsupported_options.empty()) missing.push_back("unsupported_options");
  return missing;
}

std::vector<std::string> missing_required_generation_surface_requirements(
    const LinkedWorldGenerationSurface& surface) {
  auto missing = missing_required_generation_capabilities(surface.capabilities);
  if (surface.catalog.contains("item_pool") && surface.catalog.at("item_pool").is_array() &&
      surface.catalog.at("item_pool").empty()) {
    missing.push_back("empty_generation_item_pool");
  } else if (surface.capabilities.can_build_item_pool) {
    if (!surface.catalog.contains("item_pool") || !surface.catalog.at("item_pool").is_array()) {
      missing.push_back("missing_generation_item_pool");
    }
  }
  if (surface.capabilities.can_solve_logic && !surface.generation_rules.contains("logic_rules")) {
    missing.push_back("missing_logic_rules");
  }
  if (surface.generation_rules.contains("logic_rules")) {
    const auto& logic_rules = surface.generation_rules.at("logic_rules");
    const auto placement_rules_for_probe =
        surface.generation_rules.value("placement_rules", nlohmann::json::object());
    const auto native_probe_report = native_probe_contract_execution_report(surface.catalog,
                                                                            logic_rules,
                                                                            placement_rules_for_probe,
                                                                            surface.logic);
    if (has_not_consumed_candidate_surface(logic_rules.value("candidate_location_rules", nlohmann::json::object())) ||
        has_not_consumed_candidate_surface(logic_rules.value("candidate_item_effects", nlohmann::json::object())) ||
        json_array_contains_string(logic_rules.value("flags", nlohmann::json::array()), "candidate-not-consumed")) {
      push_unique(missing, "logic_candidate_not_consumed");
    }
    const auto ruleset = logic_rules.value("ruleset", nlohmann::json::object());
    const auto logic_rules_probe_authorizes =
        native_probe_report.value("contract_count", 0) > 0 &&
        native_probe_report.value("fail_count", 0) == 0 &&
        (explicitly_authorizes_native_logic_solve(ruleset) ||
         explicitly_authorizes_native_logic_solve(logic_rules));
    if (!logic_rules_probe_authorizes &&
        (json_object_status_contains(ruleset, "not-authorizing") ||
         !authorizes_native_logic_solve(ruleset))) {
      push_unique(missing, "logic_rules_not_authorizing");
    }
    const auto region_graph = logic_region_graph_object(logic_rules);
    const auto region_graph_probe_authorizes =
        (native_probe_authorized_consumed_surface(native_probe_report,
                                                  "region_graph",
                                                  region_graph,
                                                  explicitly_authorizes_native_logic_solve(region_graph)) ||
         (native_probe_surface_passed(native_probe_report, "region_graph") &&
          has_authorized_consumed_region_graph_edges(region_graph)));
    if (!region_graph.empty() &&
        !region_graph_probe_authorizes &&
        blocks_native_logic_contract(region_graph)) {
      push_unique(missing, "region_graph_not_authorizing");
    }
    const auto edge_audit = region_graph.value("edge_audit", nlohmann::json::object());
    const auto edge_blocker_requirements = edge_audit.is_object()
        ? edge_audit.value("edge_blocker_requirements", nlohmann::json::array())
        : nlohmann::json::array();
    const auto missing_generation_contract_surfaces = edge_audit.is_object()
        ? edge_audit.value("missing_generation_contract_surfaces", nlohmann::json::object())
        : nlohmann::json::object();
    if (!region_graph_probe_authorizes &&
        (blocks_native_logic_contract(edge_audit) ||
         blocks_native_logic_contract(edge_blocker_requirements) ||
         blocks_native_logic_contract(missing_generation_contract_surfaces))) {
      push_unique(missing, "region_graph_edge_blockers_not_consumed");
    }
    if (!region_graph_probe_authorizes && has_region_graph_placeholder_edges(region_graph)) {
      push_unique(missing, "region_graph_placeholder_edges");
    }
    if (!region_graph_probe_authorizes &&
        native_replacement_edge_execution_report(logic_rules).value("failed_count", 0) > 0) {
      push_unique(missing, "region_graph_placeholder_edges");
    }
    for (const auto& reason : invalid_region_graph_reasons(region_graph)) {
      push_unique(missing, reason);
    }
    validate_declared_generation_contract_surfaces(surface,
                                                  logic_rules,
                                                  placement_rules_for_probe,
                                                  native_probe_report,
                                                  missing);
    validate_optional_logic_surface_authorization(logic_rules,
                                                  "location_refinements",
                                                  "location_refinements",
                                                  native_probe_report,
                                                  missing);
    validate_optional_logic_surface_authorization(logic_rules,
                                                  "per_location_refinements",
                                                  "location_refinements",
                                                  native_probe_report,
                                                  missing);
    validate_optional_logic_surface_authorization(logic_rules,
                                                  "location_rule_refinements",
                                                  "location_refinements",
                                                  native_probe_report,
                                                  missing);
    validate_optional_logic_surface_authorization(logic_rules,
                                                  "location_rule_segmentation",
                                                  "location_rule_segmentation",
                                                  native_probe_report,
                                                  missing);
    validate_location_rule_guardrails(surface, logic_rules, missing);
    validate_optional_location_rule_fact_names(logic_rules, missing);
    if (has_unsupported_progressive_effects(logic_rules)) {
      push_unique(missing, "unsupported_progressive_effects");
    }
  }
  if (surface.capabilities.can_place_items && !surface.generation_rules.contains("placement_rules")) {
    missing.push_back("missing_placement_rules");
  }
  if (!surface.generation_rules.contains("logic_rules") &&
      surface.generation_rules.contains("placement_rules")) {
    const auto native_probe_report = native_probe_contract_execution_report(surface.catalog,
                                                                            nlohmann::json::object(),
                                                                            surface.generation_rules.at("placement_rules"),
                                                                            surface.logic);
    validate_declared_generation_contract_surfaces(surface,
                                                  nlohmann::json::object(),
                                                  surface.generation_rules.at("placement_rules"),
                                                  native_probe_report,
                                                  missing);
  }
  if (surface.generation_rules.contains("placement_rules")) {
    const auto& placement_rules = surface.generation_rules.at("placement_rules");
    const auto logic_rules_for_probe =
        surface.generation_rules.value("logic_rules", nlohmann::json::object());
    const auto native_probe_report = native_probe_contract_execution_report(surface.catalog,
                                                                            logic_rules_for_probe,
                                                                            placement_rules,
                                                                            surface.logic);
    const auto placement_probe_authorizes =
        native_probe_authorized_consumed_surface(native_probe_report,
                                                 "placement",
                                                 placement_rules,
                                                 explicitly_authorizes_native_placement_surface(placement_rules));
    if (has_not_consumed_candidate_surface(placement_rules)) {
      push_unique(missing, "placement_rules_not_consumed");
    }
    if (placement_rules.contains("authorizes_native_placement") &&
        !placement_probe_authorizes &&
        !truthy_json_flag(placement_rules.at("authorizes_native_placement"))) {
      push_unique(missing, "placement_rules_not_authorizing");
    }
    if (object_flag_is_true(placement_rules, "blocks_can_place_items_until_audited")) {
      push_unique(missing, "placement_blocked_until_audited");
    }
    if (placement_rules.contains("fillable_locations_source") &&
        placement_rules.at("fillable_locations_source").is_object()) {
      const auto& source = placement_rules.at("fillable_locations_source");
      const auto validation = source.value("validation", nlohmann::json::object());
      if (object_flag_is_true(validation, "blocks_can_place_items_until_audited") &&
          !clears_native_placement_audit_block(validation)) {
        push_unique(missing, "fillable_locations_pending_audit");
      }
      const auto risk_audit = source.value("risk_audit", nlohmann::json::object());
      if (has_not_consumed_candidate_surface(risk_audit) &&
          !risk_audit.value("location_tags_pending", nlohmann::json::array()).empty() &&
          !clears_native_placement_audit_block(risk_audit)) {
        push_unique(missing, "risk_audit_not_consumed");
      }
      const auto expected = placement_rules.at("fillable_locations_source").value("expected_fillable_count", 0);
      const auto set_name = placement_rules.value("fillable_location_set", std::string{"main_pool_fillable"});
      std::size_t actual = 0;
      if (source.contains("sets") && source.at("sets").contains(set_name) &&
          source.at("sets").at(set_name).is_array()) {
        actual = source.at("sets").at(set_name).size();
      }
      if (expected > 0 && actual != static_cast<std::size_t>(expected)) {
        missing.push_back("explicit_fillable_location_count_mismatch");
      }
    }
    if (surface.catalog.contains("locations") && surface.catalog.at("locations").is_array() &&
        surface.catalog.contains("item_pool") && surface.catalog.at("item_pool").is_array()) {
      const auto fillable_locations = filter_fillable_locations(surface.catalog.at("locations"), placement_rules);
      if (fillable_locations.size() != surface.catalog.at("item_pool").size()) {
        push_unique(missing, "generation_item_location_count_mismatch");
      }
    }
  }
  if (surface.capabilities.can_emit_patch && !is_supported_patch_mode(patch_mode_for_surface(surface))) {
    push_unique(missing, "unsupported_patch_mode");
  }
  return missing;
}

GenerationPackageResult generate_seed_package_from_linkedworlds(
    const GenerationPackageRequest& request,
    const std::vector<LinkedWorldGenerationSurface>& linkedworlds) {
  GenerationPackageResult result;
  try {
    if (request.room_id.empty()) throw std::runtime_error("missing_room_id");
    if (request.seed_id.empty()) throw std::runtime_error("missing_seed_id");
    if (request.slots.empty()) throw std::runtime_error("missing_slots");

    for (const auto& slot : request.slots) {
      require_surface_for_slot(slot, linkedworlds);
      const auto& surface = surface_for_slot(slot, linkedworlds);
      const auto missing = missing_seed_package_capabilities(surface);
      if (!missing.empty()) {
        result.error = "missing_generation_capability:" + slot.linkedworld_id + ":" + nlohmann::json(missing).dump();
        return result;
      }
      const auto surface_missing = missing_seed_package_surface_requirements(surface);
      if (!surface_missing.empty()) {
        if (std::find(surface_missing.begin(), surface_missing.end(), "missing_generation_item_pool") !=
            surface_missing.end()) {
          result.error = "missing_generation_item_pool:" + slot.linkedworld_id;
        } else if (std::find(surface_missing.begin(), surface_missing.end(), "empty_generation_item_pool") !=
                   surface_missing.end()) {
          result.error = "empty_generation_item_pool:" + slot.linkedworld_id;
        } else if (std::find(surface_missing.begin(), surface_missing.end(), "missing_logic_rules") !=
                   surface_missing.end()) {
          result.error = "missing_logic_rules:" + slot.linkedworld_id;
        } else if (std::find(surface_missing.begin(), surface_missing.end(), "missing_placement_rules") !=
                   surface_missing.end()) {
          result.error = "missing_placement_rules:" + slot.linkedworld_id;
        } else {
          result.error = "missing_generation_surface_requirement:" + slot.linkedworld_id + ":" +
                         nlohmann::json(surface_missing).dump();
        }
        return result;
      }
    }

    const auto package_dir = request.output_root / request.seed_id;
    std::filesystem::create_directories(package_dir / "patches");
    std::filesystem::create_directories(package_dir / "patch_contracts");

    nlohmann::json slots = nlohmann::json::array();
    nlohmann::json linkedworlds_json = nlohmann::json::array();
    nlohmann::json config_versions = nlohmann::json::array();
    nlohmann::json placements = nlohmann::json::array();
    nlohmann::json checks = nlohmann::json::array();
    nlohmann::json items = nlohmann::json::array();
    std::map<std::string, nlohmann::json> slot_files;
    std::size_t source_location_count = 0;

    for (const auto& slot : request.slots) {
      const auto& surface = surface_for_slot(slot, linkedworlds);
      slots.push_back(to_json(slot));
      nlohmann::json config_version = {
          {"slot_id", slot.slot_id},
          {"user_id", slot.user_id},
          {"game_key", slot.game_key},
          {"linkedworld_id", surface.linkedworld_id},
          {"config_version_id", slot.config_version_id},
          {"source", "nexus.config_version_id"},
      };
      if (!slot.config_snapshot.empty()) {
        config_version["config_snapshot"] = slot.config_snapshot;
      }
      config_versions.push_back(std::move(config_version));
      linkedworlds_json.push_back({
          {"slot_id", slot.slot_id},
          {"module_id", surface.module_id},
          {"game_key", surface.game_key},
          {"linkedworld_id", surface.linkedworld_id},
          {"version", surface.version},
          {"patch_mode", patch_mode_for_surface(surface)},
          {"capabilities", to_json(surface.capabilities)},
      });

      const auto logic_rules = surface.generation_rules.value("logic_rules", nlohmann::json::object());
      const auto placement_rules = surface.generation_rules.value("placement_rules", nlohmann::json::object());
      if (!is_server_first_package_surface(surface)) {
        const auto native_generation_facts =
            authorized_native_generation_facts(surface.catalog, logic_rules, placement_rules, surface.logic);
        source_location_count += surface.catalog.value("locations", nlohmann::json::array()).size();
        const auto slot_locations = filter_fillable_locations(
            surface.catalog.value("locations", nlohmann::json::array()),
            placement_rules);
        for (const auto& location : slot_locations) {
          auto check = enrich_location_with_generation_rules(location, logic_rules, placement_rules);
          for (const auto& fact : native_generation_facts) {
            check["logic_starting_facts"].push_back(fact);
          }
          check["slot_id"] = slot.slot_id;
          check["linkedworld_id"] = surface.linkedworld_id;
          check["game_key"] = surface.game_key;
          checks.push_back(std::move(check));
        }
        for (const auto& item : surface.catalog.at("item_pool")) {
          auto item_out = enrich_item_with_generation_rules(item, logic_rules);
          item_out["slot_id"] = slot.slot_id;
          item_out["linkedworld_id"] = surface.linkedworld_id;
          item_out["game_key"] = surface.game_key;
          item_out["source"] = "linkedworld.generation.item_pool";
          items.push_back(std::move(item_out));
        }
      }

      nlohmann::json slot_manifest = {
          {"slot_id", slot.slot_id},
          {"user_id", slot.user_id},
          {"display_name", slot.display_name},
          {"game_key", slot.game_key},
          {"linkedworld_id", surface.linkedworld_id},
          {"config_version_id", slot.config_version_id},
          {"patch_mode", patch_mode_for_surface(surface)},
          {"launch_artifact", nullptr},
          {"patch_artifact", patch_artifact_ref_for_slot(surface, slot.slot_id)},
          {"patch_contract_ref", patch_contract_ref_for_slot(surface, slot.slot_id)},
          {"tracker_seed_state", "tracker_seed_state.json"},
          {"sklmi_contract_ref", "sklmi_seed_contract.json"},
          {"link_room_contract_ref", "link_room_seed_contract.json"},
          {"player_options_source", "nexus.config_version_id"},
      };
      if (!slot.config_snapshot.empty()) {
        slot_manifest["config_snapshot"] = slot.config_snapshot;
      }
      slot_files["slot_manifest." + std::to_string(slot.slot_id) + ".json"] = slot_manifest;
      slot_files[patch_contract_ref_for_slot(surface, slot.slot_id)] = {
          {"schema_version", "sekailink-patch-contract-v1"},
          {"seed_id", request.seed_id},
          {"slot_id", slot.slot_id},
          {"linkedworld_id", surface.linkedworld_id},
          {"game_key", surface.game_key},
          {"patch_mode", patch_mode_for_surface(surface)},
          {"mode", patch_mode_for_surface(surface)},
          {"artifact", patch_contract_artifact_for_slot(surface, slot.slot_id)},
          {"base_asset", patch_manifest_patch_for_surface(surface).value("base_asset", nlohmann::json(nullptr))},
          {"apply_host", patch_manifest_patch_for_surface(surface).value("apply_host", nlohmann::json(nullptr))},
          {"server_dispatch", patch_manifest_patch_for_surface(surface).value("server_dispatch", nlohmann::json(nullptr))},
          {"patch_manifest_ref", surface.patch.value("manifest_ref", std::string{})},
          {"patch_manifest", patch_manifest_for_surface(surface)},
          {"patch", surface.patch},
      };
    }

    std::string replay_validation = "full-logic-replay-passed";
    if (!checks.empty() || !items.empty()) {
      placements = build_deterministic_multiworld_placements(request, checks, items);
      validate_placements_replay(request, checks, items, placements);
    } else {
      replay_validation = "not-applicable-server-first-package";
    }
    const auto preplacement_count = static_cast<std::size_t>(std::count_if(
        placements.begin(), placements.end(), [](const auto& placement) {
          return placement.value("preplaced", false);
        }));
    const auto post_logic_fill_count = static_cast<std::size_t>(std::count_if(
        placements.begin(), placements.end(), [](const auto& placement) {
          return placement.value("post_logic_fill", false);
        }));
    const nlohmann::json room_manifest = {
        {"schema_version", "sekailink-room-seed-manifest-v1"},
        {"room_id", request.room_id},
        {"seed_id", request.seed_id},
        {"generation_scope", "multiworld"},
        {"slots", slots},
        {"slot_count", slots.size()},
        {"linkedworld_count", linkedworlds_json.size()},
        {"config_versions", config_versions},
        {"location_count", checks.size()},
        {"item_count", items.size()},
        {"event_surface", "linkedworld"},
        {"send_receive_rules", "link_room_seed_contract.json"},
        {"client_log_labels", nlohmann::json::array()},
        {"completion_rules", "linkedworld.logic.completion"},
        {"goal_rules", "linkedworld.logic.goals"},
    };
    const nlohmann::json tracker_seed_state = {
        {"schema_version", "sekailink-tracker-seed-state-v1"},
        {"seed_id", request.seed_id},
        {"room_id", request.room_id},
        {"generation_scope", "multiworld"},
        {"slots", slots},
        {"linkedworlds", linkedworlds_json},
        {"config_versions", config_versions},
    };
    const nlohmann::json sklmi_seed_contract = {
        {"schema_version", "sekailink-sklmi-seed-contract-v1"},
        {"seed_id", request.seed_id},
        {"room_id", request.room_id},
        {"generation_scope", "multiworld"},
        {"slots", slots},
        {"config_versions", config_versions},
        {"runtime_contract_source", "linkedworld.runtime"},
    };
    const nlohmann::json link_room_seed_contract = {
        {"schema_version", "sekailink-link-room-seed-contract-v1"},
        {"seed_id", request.seed_id},
        {"room_id", request.room_id},
        {"generation_scope", "multiworld"},
        {"slots", slots},
        {"config_versions", config_versions},
        {"checks_ref", "checks.json"},
        {"items_ref", "items.json"},
        {"placements_ref", "placements.json"},
    };
    const nlohmann::json audit = {
        {"schema_version", "sekailink-generation-audit-v1"},
        {"job_id", request.job_id},
        {"seed_id", request.seed_id},
        {"rng_seed_commitment", sha256_hex(request.rng_seed)},
        {"config_source", "nexus.config_version_id"},
        {"config_versions", config_versions},
        {"generator_mode", "generic-linkedworld-ir"},
        {"solver_mode", "generic-fact-sphere-v1"},
        {"placement_algorithm", "deterministic-constraint-sphere-v1"},
        {"source_location_count", source_location_count},
        {"filtered_location_count", checks.size()},
        {"item_count", items.size()},
        {"placement_count", placements.size()},
        {"preplacement_count", preplacement_count},
        {"post_logic_fill_count", post_logic_fill_count},
        {"replay_validation", replay_validation},
    };

    std::map<std::string, nlohmann::json> files;
    for (const auto& [name, value] : slot_files) {
      files[name] = value;
    }
    files["room_manifest.json"] = room_manifest;
    files["placements.json"] = placements;
    files["checks.json"] = checks;
    files["items.json"] = items;
    files["tracker_seed_state.json"] = tracker_seed_state;
    files["sklmi_seed_contract.json"] = sklmi_seed_contract;
    files["link_room_seed_contract.json"] = link_room_seed_contract;
    files["audit.json"] = audit;

    nlohmann::json artifact_hashes = nlohmann::json::object();
    for (const auto& [name, value] : files) {
      artifact_hashes[name] = sha256_hex(value.dump());
    }

    const nlohmann::json seed_manifest = {
        {"schema_version", "sekailink-seed-package-v1"},
        {"seed_id", request.seed_id},
        {"job_id", request.job_id},
        {"room_id", request.room_id},
        {"created_at", utc_timestamp_now()},
        {"generator_version", "sekailink-worlds-generic-v1"},
        {"generation_algorithm", "linkedworld-ir-capability-gated"},
        {"generation_scope", "multiworld"},
        {"rng_seed_commitment", sha256_hex(request.rng_seed)},
        {"players", slots},
        {"slot_count", slots.size()},
        {"location_count", checks.size()},
        {"item_count", items.size()},
        {"placement_count", placements.size()},
        {"post_logic_fill_count", post_logic_fill_count},
        {"linkedworlds", linkedworlds_json},
        {"linkedworld_count", linkedworlds_json.size()},
        {"config_versions", config_versions},
        {"artifact_hashes", artifact_hashes},
        {"nexus_projection_keys", {{"seed_id", request.seed_id}, {"room_id", request.room_id}}},
    };
    verify_generated_seed_package_contracts(request, files, seed_manifest);
    files["seed_manifest.json"] = seed_manifest;

    for (const auto& [name, value] : files) {
      write_json_file(package_dir / name, value);
    }

    result.ok = true;
    result.package_dir = package_dir;
    result.manifest = seed_manifest;
    result.package_hash = sha256_hex(package_hash_material(files));
    return result;
  } catch (const std::exception& exception) {
    result.ok = false;
    result.error = exception.what();
    return result;
  }
}

}  // namespace sekailink_server
