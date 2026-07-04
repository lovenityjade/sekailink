#include "sekailink_server/generic_generation_logic_rules.hpp"

#include "sekailink_server/generic_generation_internal.hpp"
#include "sekailink_server/generic_generation_surface_contracts.hpp"

#include <algorithm>

namespace sekailink_server::generation_internal {

void push_unique(std::vector<std::string>& values, const std::string& value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

nlohmann::json merge_grants(const nlohmann::json& left, const nlohmann::json& right) {
  nlohmann::json merged = nlohmann::json::array();
  std::set<std::string> seen;
  for (const auto& grants : {left, right}) {
    if (!grants.is_array()) {
      continue;
    }
    for (const auto& grant : grants) {
      const auto text = string_from_number_or_string(grant);
      if (!text.empty() && seen.insert(text).second) {
        merged.push_back(text);
      }
    }
  }
  return merged;
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
    if (!args.is_array()) {
      return false;
    }
    return std::all_of(args.begin(), args.end(), [&](const auto& child) {
      return evaluate_logic_expr(child, facts);
    });
  }
  if (op == "any") {
    if (!args.is_array()) {
      return false;
    }
    return std::any_of(args.begin(), args.end(), [&](const auto& child) {
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

nlohmann::json find_rule_for_id(const nlohmann::json& rules, const std::string& id) {
  if (!rules.is_array() || id.empty()) {
    return nlohmann::json::object();
  }
  for (const auto& rule : rules) {
    if (json_field_as_string(rule, "id") == id ||
        json_field_as_string(rule, "location_id") == id ||
        json_field_as_string(rule, "target_id") == id) {
      return rule;
    }
  }
  return nlohmann::json::object();
}

nlohmann::json find_rules_for_id(const nlohmann::json& rules, const std::string& id) {
  nlohmann::json found = nlohmann::json::array();
  if (!rules.is_array() || id.empty()) {
    return found;
  }
  for (const auto& rule : rules) {
    if (json_field_as_string(rule, "id") == id ||
        json_field_as_string(rule, "location_id") == id ||
        json_field_as_string(rule, "target_id") == id) {
      found.push_back(rule);
    }
  }
  return found;
}

std::string location_rule_id(const nlohmann::json& rule) {
  if (!rule.is_object()) {
    return {};
  }
  return first_non_empty({
      json_field_as_string(rule, "id"),
      json_field_as_string(rule, "location_id"),
      json_field_as_string(rule, "target_id"),
  });
}

nlohmann::json logic_location_rules_array(const nlohmann::json& logic_rules) {
  const auto locations = logic_rules.value("locations", nlohmann::json::array());
  if (locations.is_array()) {
    return locations;
  }
  if (locations.is_object() && locations.contains("rules") && locations.at("rules").is_array()) {
    return locations.at("rules");
  }
  return nlohmann::json::array();
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
        location_id == "refinements" || location_id == "locations") {
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

nlohmann::json logic_location_refinement_rules_array(const nlohmann::json& logic_rules) {
  nlohmann::json rules = nlohmann::json::array();
  for (const auto& key : {"location_refinements", "per_location_refinements", "location_rule_refinements"}) {
    const auto surface = logic_rules.value(key, nlohmann::json::object());
    if (!is_explicitly_consumable_logic_surface(surface)) {
      continue;
    }
    for (const auto& rule : rules_from_location_rule_collection(surface)) {
      rules.push_back(rule);
    }
  }
  return rules;
}

namespace {

void append_segment_location_rule(nlohmann::json& rules,
                                  const nlohmann::json& location,
                                  const nlohmann::json& requirement) {
  const auto location_id = location.is_object()
      ? first_non_empty({
            json_field_as_string(location, "id"),
            json_field_as_string(location, "location_id"),
            json_field_as_string(location, "location"),
        })
      : string_from_number_or_string(location);
  if (location_id.empty()) {
    return;
  }
  rules.push_back({
      {"location_id", location_id},
      {"requires", requirement},
  });
}

}  // namespace

nlohmann::json logic_segmented_location_rules_array(const nlohmann::json& logic_rules) {
  const auto segmentation = logic_rules.value("location_rule_segmentation", nlohmann::json::object());
  if (!is_explicitly_consumable_logic_surface(segmentation)) {
    return nlohmann::json::array();
  }

  nlohmann::json rules = rules_from_location_rule_collection(segmentation);
  const auto segments = segmentation.value("segments", nlohmann::json::array());
  if (segments.is_array()) {
    for (const auto& segment : segments) {
      if (!segment.is_object()) {
        continue;
      }
      for (const auto& rule : rules_from_location_rule_collection(segment)) {
        rules.push_back(rule);
      }
      const auto requirement = segment.value("requires", nlohmann::json{{"op", "true"}});
      const auto locations = segment.contains("location_ids")
          ? segment.at("location_ids")
          : segment.value("locations", nlohmann::json::array());
      if (locations.is_array()) {
        for (const auto& location : locations) {
          append_segment_location_rule(rules, location, requirement);
        }
      }
    }
  } else if (segments.is_object()) {
    for (const auto& [unused, segment] : segments.items()) {
      if (!segment.is_object()) {
        continue;
      }
      for (const auto& rule : rules_from_location_rule_collection(segment)) {
        rules.push_back(rule);
      }
      const auto requirement = segment.value("requires", nlohmann::json{{"op", "true"}});
      const auto locations = segment.contains("location_ids")
          ? segment.at("location_ids")
          : segment.value("locations", nlohmann::json::array());
      if (locations.is_array()) {
        for (const auto& location : locations) {
          append_segment_location_rule(rules, location, requirement);
        }
      }
    }
  }
  return rules;
}

nlohmann::json logic_item_effects_object(const nlohmann::json& logic_rules) {
  const auto effects = logic_rules.value("item_effects", nlohmann::json::object());
  if (effects.is_object() && effects.contains("effects") && effects.at("effects").is_object()) {
    return effects.at("effects");
  }
  if (effects.is_object()) {
    return effects;
  }
  return nlohmann::json::object();
}

nlohmann::json logic_region_graph_object(const nlohmann::json& logic_rules) {
  if (logic_rules.contains("region_graph") && logic_rules.at("region_graph").is_object()) {
    return logic_rules.at("region_graph");
  }
  if (logic_rules.contains("starting_regions") || logic_rules.contains("edges") ||
      logic_rules.contains("location_region_bindings")) {
    return logic_rules;
  }
  return nlohmann::json::object();
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

void collect_grant_references(const nlohmann::json& effect, std::set<std::string>& facts) {
  if (!effect.is_object()) {
    return;
  }
  for (const auto& fact : json_string_array(effect.value("grants", nlohmann::json::array()))) {
    facts.insert(fact);
  }
  auto levels = effect.contains("stages") ? effect.at("stages") : effect.value("levels", nlohmann::json::array());
  if (levels.empty() && effect.contains("stage_grants") && effect.at("stage_grants").is_object()) {
    levels = effect.at("stage_grants").value("stages", nlohmann::json::array());
  }
  if (levels.is_array()) {
    for (const auto& level : levels) {
      for (const auto& fact : json_string_array(level.value("grants", nlohmann::json::array()))) {
        facts.insert(fact);
      }
    }
  }
  const auto thresholds = effect.value("count_grants", nlohmann::json::object())
                              .value("thresholds", nlohmann::json::array());
  if (thresholds.is_array()) {
    for (const auto& threshold : thresholds) {
      for (const auto& fact : json_string_array(threshold.value("grants", nlohmann::json::array()))) {
        facts.insert(fact);
      }
    }
  }
}

}  // namespace sekailink_server::generation_internal
