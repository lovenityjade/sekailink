#include "sekailink_server/generic_generation_checks.hpp"

#include "sekailink_server/generic_generation_internal.hpp"

#include <set>

namespace sekailink_server::generation_internal {

nlohmann::json filter_fillable_locations(const nlohmann::json& locations,
                                         const nlohmann::json& placement_rules) {
  if (!placement_rules.is_object()) {
    return locations;
  }

  const auto fillable_locations = placement_rules.contains("fillable_location_ids")
                                      ? placement_rules.at("fillable_location_ids")
                                      : placement_rules.value("fillable_locations", nlohmann::json::array());
  auto effective_fillable_locations = fillable_locations;
  if (effective_fillable_locations.empty() && placement_rules.contains("fillable_locations_source")) {
    const auto& source = placement_rules.at("fillable_locations_source");
    const auto set_name = placement_rules.value("fillable_location_set", std::string{"main_pool_fillable"});
    if (source.contains("sets") && source.at("sets").contains(set_name) && source.at("sets").at(set_name).is_array()) {
      effective_fillable_locations = source.at("sets").at(set_name);
    }
  }
  const auto reserved_locations = placement_rules.contains("reserved_location_ids")
                                      ? placement_rules.at("reserved_location_ids")
                                      : placement_rules.value("reserved_locations", nlohmann::json::array());

  std::set<std::string> reserved_ids;
  for (const auto& reserved : reserved_locations) {
    if (reserved.is_object()) {
      reserved_ids.insert(first_non_empty({
          json_field_as_string(reserved, "id"),
          json_field_as_string(reserved, "location_id"),
          json_field_as_string(reserved, "location"),
      }));
    } else {
      reserved_ids.insert(string_from_number_or_string(reserved));
    }
  }

  if (effective_fillable_locations.is_array() && !effective_fillable_locations.empty()) {
    std::set<std::string> fillable_ids;
    for (const auto& id : effective_fillable_locations) {
      if (id.is_object()) {
        fillable_ids.insert(first_non_empty({
            json_field_as_string(id, "id"),
            json_field_as_string(id, "location_id"),
            json_field_as_string(id, "location"),
        }));
      } else {
        fillable_ids.insert(string_from_number_or_string(id));
      }
    }
    nlohmann::json filtered = nlohmann::json::array();
    for (const auto& location : locations) {
      const auto location_id = first_non_empty({
          json_field_as_string(location, "id"),
          json_field_as_string(location, "location_id"),
      });
      if (reserved_ids.count(location_id) > 0) {
        continue;
      }
      if (fillable_ids.count(location_id) > 0) {
        filtered.push_back(location);
      }
    }
    return filtered;
  }

  const auto limit = placement_rules.value("fillable_location_count", 0);
  if (limit > 0 && static_cast<std::size_t>(limit) < locations.size()) {
    nlohmann::json filtered = nlohmann::json::array();
    for (const auto& location : locations) {
      const auto location_id = first_non_empty({
          json_field_as_string(location, "id"),
          json_field_as_string(location, "location_id"),
      });
      if (reserved_ids.count(location_id) > 0) {
        continue;
      }
      filtered.push_back(location);
      if (filtered.size() == static_cast<std::size_t>(limit)) {
        break;
      }
    }
    return filtered;
  }

  if (!reserved_ids.empty()) {
    nlohmann::json filtered = nlohmann::json::array();
    for (const auto& location : locations) {
      const auto location_id = first_non_empty({
          json_field_as_string(location, "id"),
          json_field_as_string(location, "location_id"),
      });
      if (reserved_ids.count(location_id) == 0) {
        filtered.push_back(location);
      }
    }
    return filtered;
  }

  return locations;
}

}  // namespace sekailink_server::generation_internal
