#pragma once

#include <nlohmann/json.hpp>

namespace sekailink_server {

nlohmann::json build_linkedworld_room_surface(const nlohmann::json& linkedworld_payload);

}  // namespace sekailink_server
