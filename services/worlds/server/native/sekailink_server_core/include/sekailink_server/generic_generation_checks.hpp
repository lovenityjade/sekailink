#pragma once

#include "nlohmann/json.hpp"

namespace sekailink_server::generation_internal {

nlohmann::json filter_fillable_locations(const nlohmann::json& locations,
                                         const nlohmann::json& placement_rules);

}  // namespace sekailink_server::generation_internal
