#include "sekailink_server/generic_generation_placement.hpp"

#include "sekailink_server/generic_generation_internal.hpp"
#include "sekailink_server/generic_generation_logic_rules.hpp"
#include "sekailink_server/generic_generation_region_analysis.hpp"
#include "sekailink_server/generic_generation_region_graph.hpp"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace sekailink_server::generation_internal {

bool placement_allows_item(const nlohmann::json& location, const nlohmann::json& item) {
  const auto item_tags = json_string_set(item.value("tags", nlohmann::json::array()));
  const auto location_tags = json_string_set(location.value("tags", nlohmann::json::array()));
  const auto classification = item.value("classification", std::string{});
  std::set<std::string> available_tags = item_tags;
  if (!classification.empty()) {
    available_tags.insert(classification);
  }
  if (item.value("advancement", false)) {
    available_tags.insert("advancement");
    available_tags.insert("progression");
  }

  const auto allow_tags = json_string_array(location.value("allow_item_tags", nlohmann::json::array()));
  if (!allow_tags.empty() && !has_any_tag(available_tags, allow_tags)) {
    return false;
  }
  const auto forbid_tags = json_string_array(location.value("forbid_item_tags", nlohmann::json::array()));
  if (!forbid_tags.empty() && has_any_tag(available_tags, forbid_tags)) {
    return false;
  }
  for (const auto& constraint : location.value("placement_item_constraints", nlohmann::json::array())) {
    const auto constrained_item_tags = json_string_array(constraint.value("item_tags", nlohmann::json::array()));
    if (!constrained_item_tags.empty() && !has_any_tag(available_tags, constrained_item_tags)) {
      continue;
    }
    const auto forbid_location_tags =
        json_string_array(constraint.value("forbid_location_tags", nlohmann::json::array()));
    if (!forbid_location_tags.empty() && has_any_tag(location_tags, forbid_location_tags)) {
      return false;
    }
    const auto allow_location_tags =
        json_string_array(constraint.value("allow_location_tags", nlohmann::json::array()));
    if (!allow_location_tags.empty() && !has_any_tag(location_tags, allow_location_tags)) {
      return false;
    }
  }
  return true;
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

nlohmann::json find_preplacement_for_location(const nlohmann::json& preplacements,
                                              const std::string& location_id) {
  if (!preplacements.is_array() || location_id.empty()) {
    return nlohmann::json::object();
  }
  for (const auto& preplacement : preplacements) {
    const auto preplaced_location = first_non_empty({
        json_field_as_string(preplacement, "location_id"),
        json_field_as_string(preplacement, "location"),
        json_field_as_string(preplacement, "target_id"),
    });
    if (preplaced_location == location_id) {
      return preplacement;
    }
  }
  return nlohmann::json::object();
}

void apply_location_logic_rule(nlohmann::json& location, const nlohmann::json& rule) {
  if (rule.contains("requires")) {
    location["requires"] = combine_requires_all(
        location.value("requires", nlohmann::json{{"op", "true"}}),
        rule.at("requires"));
  }
  if (rule.contains("region") && !location.contains("region")) {
    location["region"] = rule.at("region");
  }
  if (rule.contains("tags") && !location.contains("tags")) {
    location["tags"] = rule.at("tags");
  }
}

nlohmann::json enrich_location_with_generation_rules(const nlohmann::json& location,
                                                     const nlohmann::json& logic_rules,
                                                     const nlohmann::json& placement_rules) {
  auto enriched = location;
  const auto location_id = json_field_as_string(location, "id");
  const auto logic_rule = find_rule_for_id(logic_location_rules_array(logic_rules), location_id);
  const auto legacy_location_rule =
      find_rule_for_id(logic_rules.value("location_rules", nlohmann::json::array()), location_id);
  const auto region_graph = effective_region_graph_object(logic_region_graph_object(logic_rules));
  for (const auto& rule : {logic_rule, legacy_location_rule}) {
    apply_location_logic_rule(enriched, rule);
  }
  for (const auto& rule : find_rules_for_id(logic_location_refinement_rules_array(logic_rules), location_id)) {
    apply_location_logic_rule(enriched, rule);
  }
  for (const auto& rule : find_rules_for_id(logic_segmented_location_rules_array(logic_rules), location_id)) {
    apply_location_logic_rule(enriched, rule);
  }
  if (!enriched.contains("requires")) {
    enriched["requires"] = {{"op", "true"}};
  }
  const auto region_binding = find_region_binding_for_location(region_graph, location_id);
  if (!region_binding.empty()) {
    const auto region_id = first_non_empty({
        json_field_as_string(region_binding, "region_id"),
        json_field_as_string(region_binding, "region"),
    });
    if (!region_id.empty()) {
      enriched["region_id"] = region_id;
      enriched["requires"] = combine_requires_all(
          enriched.value("requires", nlohmann::json{{"op", "true"}}),
          {{"op", "fact"}, {"id", region_reachability_fact(region_id)}});
    }
  }
  enriched["logic_starting_facts"] = starting_facts_from_logic_rules(logic_rules);
  if (!region_graph.empty()) {
    enriched["region_graph"] = region_graph;
  }

  const auto placement_rule =
      find_rule_for_id(placement_rules.value("location_constraints", nlohmann::json::array()), location_id);
  if (placement_rule.contains("allow_item_tags")) {
    enriched["allow_item_tags"] = placement_rule.at("allow_item_tags");
  }
  if (placement_rule.contains("forbid_item_tags")) {
    enriched["forbid_item_tags"] = placement_rule.at("forbid_item_tags");
  }
  if (placement_rules.contains("item_constraints")) {
    enriched["placement_item_constraints"] = placement_rules.at("item_constraints");
  }
  const auto preplacement =
      find_preplacement_for_location(placement_rules.value("preplacements", nlohmann::json::array()), location_id);
  if (!preplacement.empty()) {
    enriched["preplacement"] = preplacement;
  }
  return enriched;
}

nlohmann::json enrich_item_with_generation_rules(const nlohmann::json& item,
                                                 const nlohmann::json& logic_rules) {
  auto enriched = item;
  const auto effects = logic_item_effects_object(logic_rules);
  for (const auto& key : {base_instance_id(json_field_as_string(item, "id")),
                         json_field_as_string(item, "semantic_id"),
                         json_field_as_string(item, "name")}) {
    if (!key.empty() && effects.contains(key)) {
      const auto effect = effects.at(key);
      enriched["generation_effect_key"] = key;
      enriched["generation_effect"] = effect;
      if (effect.contains("grants") && !enriched.contains("grants")) {
        enriched["grants"] = effect.at("grants");
      }
      if (effect.contains("tags") && !enriched.contains("tags")) {
        enriched["tags"] = effect.at("tags");
      }
      break;
    }
  }
  return enriched;
}

nlohmann::json collect_item_grants(const nlohmann::json& item,
                                   int receiving_slot,
                                   std::map<std::string, std::size_t>& progressive_levels_by_slot) {
  if (item.contains("generation_effect") && item.at("generation_effect").is_object()) {
    const auto& effect = item.at("generation_effect");
    const auto type = effect.value("type", std::string{});
    if (type == "progressive" || type == "progressive_counter" || type == "levelled" ||
        effect.contains("stages") || effect.contains("levels") || effect.contains("stage_grants")) {
      auto levels =
          effect.contains("stages") ? effect.at("stages") : effect.value("levels", nlohmann::json::array());
      if (levels.empty() && effect.contains("stage_grants") && effect.at("stage_grants").is_object()) {
        levels = effect.at("stage_grants").value("stages", nlohmann::json::array());
      }
      const auto effect_key = item.value("generation_effect_key", base_instance_id(json_field_as_string(item, "id")));
      const auto state_key = slot_entity_key(receiving_slot, effect_key);
      const auto level = progressive_levels_by_slot[state_key]++;
      if (levels.is_array() && level < levels.size()) {
        return levels.at(level).value("grants", nlohmann::json::array());
      }
      return nlohmann::json::array();
    }
    if (effect.contains("count_grants") && effect.at("count_grants").is_object()) {
      const auto effect_key = item.value("generation_effect_key", base_instance_id(json_field_as_string(item, "id")));
      const auto state_key = slot_entity_key(receiving_slot, effect_key);
      const auto count = ++progressive_levels_by_slot[state_key];
      const auto thresholds = effect.at("count_grants").value("thresholds", nlohmann::json::array());
      for (const auto& threshold : thresholds) {
        if (threshold.value("count", 0) == static_cast<int>(count)) {
          return merge_grants(effect.value("grants", nlohmann::json::array()),
                              threshold.value("grants", nlohmann::json::array()));
        }
      }
      return effect.value("grants", nlohmann::json::array());
    }
  }
  return item.value("grants", nlohmann::json::array());
}

nlohmann::json deterministic_ordered_array(const nlohmann::json& source,
                                           const std::string& rng_seed,
                                           const std::string& seed_id,
                                           const std::string& prefix) {
  std::vector<std::pair<std::string, nlohmann::json>> ranked;
  for (const auto& value : source) {
    const auto slot_id = json_field_as_string(value, "slot_id");
    const auto id = json_field_as_string(value, "id");
    const auto fallback_id = json_field_as_string(value, "name");
    const auto material = rng_seed + ":" + seed_id + ":" + prefix + ":" + slot_id + ":" +
                          (id.empty() ? fallback_id : id);
    ranked.push_back({sha256_hex(material), value});
  }
  std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
    if (left.first == right.first) {
      return left.second.dump() < right.second.dump();
    }
    return left.first < right.first;
  });

  nlohmann::json ordered = nlohmann::json::array();
  for (auto& [unused, value] : ranked) {
    ordered.push_back(std::move(value));
  }
  return ordered;
}

nlohmann::json json_without_index(const nlohmann::json& source, std::size_t remove_index) {
  nlohmann::json remaining = nlohmann::json::array();
  for (std::size_t index = 0; index < source.size(); ++index) {
    if (index != remove_index) {
      remaining.push_back(source.at(index));
    }
  }
  return remaining;
}

bool has_static_placement_matching(const nlohmann::json& checks, const nlohmann::json& items) {
  if (items.empty()) {
    return true;
  }
  if (!checks.is_array() || !items.is_array() || checks.size() < items.size()) {
    return false;
  }

  std::vector<int> matched_item_by_check(checks.size(), -1);
  std::function<bool(std::size_t, std::vector<bool>&)> augment = [&](std::size_t item_index,
                                                                     std::vector<bool>& seen_checks) {
    for (std::size_t check_index = 0; check_index < checks.size(); ++check_index) {
      if (seen_checks.at(check_index) || !placement_allows_item(checks.at(check_index), items.at(item_index))) {
        continue;
      }
      seen_checks.at(check_index) = true;
      if (matched_item_by_check.at(check_index) < 0 ||
          augment(static_cast<std::size_t>(matched_item_by_check.at(check_index)), seen_checks)) {
        matched_item_by_check.at(check_index) = static_cast<int>(item_index);
        return true;
      }
    }
    return false;
  };

  for (std::size_t item_index = 0; item_index < items.size(); ++item_index) {
    std::vector<bool> seen_checks(checks.size(), false);
    if (!augment(item_index, seen_checks)) {
      return false;
    }
  }
  return true;
}

bool all_items_are_non_advancement(const nlohmann::json& items) {
  if (!items.is_array()) {
    return false;
  }
  return std::all_of(items.begin(), items.end(), [](const auto& item) {
    return item.is_object() && !item.value("advancement", false);
  });
}

constexpr std::size_t kExactStaticMatchingGuardLimit = 64;

bool should_run_exact_static_matching_guard(const nlohmann::json& remaining_checks,
                                            const nlohmann::json& remaining_items) {
  if (!remaining_checks.is_array() || !remaining_items.is_array()) {
    return false;
  }
  // Exact bipartite rematching prevents consuming a scarce location too early.
  // It is intentionally bounded because large filler-heavy pools make this guard
  // expensive; later solver iterations and replay still enforce reachability and
  // per-location placement constraints for every placement.
  return remaining_checks.size() <= kExactStaticMatchingGuardLimit &&
         remaining_items.size() <= kExactStaticMatchingGuardLimit;
}

nlohmann::json placement_failure_diagnostics(
    const nlohmann::json& remaining_checks,
    const nlohmann::json& remaining_items,
    const std::map<int, std::set<std::string>>& facts_by_slot) {
  nlohmann::json facts_summary = nlohmann::json::object();
  for (const auto& [slot_id, facts] : facts_by_slot) {
    nlohmann::json sample = nlohmann::json::array();
    for (const auto& fact : facts) {
      if (sample.size() >= 24) {
        break;
      }
      sample.push_back(fact);
    }
    facts_summary[std::to_string(slot_id)] = {
        {"fact_count", facts.size()},
        {"sample", sample},
    };
  }
  nlohmann::json reachable_locations = nlohmann::json::array();
  nlohmann::json blocked_locations = nlohmann::json::array();
  for (const auto& location : remaining_checks) {
    const auto owning_slot = location.value("slot_id", 0);
    const auto facts = facts_by_slot.find(owning_slot);
    const auto reachable = facts != facts_by_slot.end() &&
                           evaluate_logic_expr(location.value("requires", nlohmann::json{{"op", "true"}}),
                                               facts->second);
    auto summary = nlohmann::json{
        {"slot_id", owning_slot},
        {"location_id", json_field_as_string(location, "id")},
    };
    if (reachable) {
      if (reachable_locations.size() < 12) {
        reachable_locations.push_back(std::move(summary));
      }
    } else if (blocked_locations.size() < 12) {
      summary["requires"] = location.value("requires", nlohmann::json{{"op", "true"}});
      blocked_locations.push_back(std::move(summary));
    }
  }

  nlohmann::json unplaceable_items = nlohmann::json::array();
  for (const auto& item : remaining_items) {
    std::size_t static_candidates = 0;
    std::size_t reachable_candidates = 0;
    for (const auto& location : remaining_checks) {
      if (!placement_allows_item(location, item)) {
        continue;
      }
      ++static_candidates;
      const auto owning_slot = location.value("slot_id", 0);
      const auto facts = facts_by_slot.find(owning_slot);
      if (facts != facts_by_slot.end() &&
          evaluate_logic_expr(location.value("requires", nlohmann::json{{"op", "true"}}), facts->second)) {
        ++reachable_candidates;
      }
    }
    if (reachable_candidates == 0 && unplaceable_items.size() < 12) {
      unplaceable_items.push_back({
          {"slot_id", item.value("slot_id", 0)},
          {"item_id", json_field_as_string(item, "id")},
          {"classification", item.value("classification", std::string{"unknown"})},
          {"static_candidate_count", static_candidates},
          {"reachable_candidate_count", reachable_candidates},
      });
    }
  }

  return {
      {"remaining_location_count", remaining_checks.size()},
      {"remaining_item_count", remaining_items.size()},
      {"static_matching_possible", has_static_placement_matching(remaining_checks, remaining_items)},
      {"facts_by_slot", facts_summary},
      {"reachable_locations", reachable_locations},
      {"blocked_locations", blocked_locations},
      {"unplaceable_items", unplaceable_items},
  };
}

}  // namespace sekailink_server::generation_internal
