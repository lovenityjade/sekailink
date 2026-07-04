#pragma once

#include "sekailink_server/room_audit_store.hpp"
#include "sekailink_server/room_registry.hpp"

#include <string>
#include <vector>

namespace sekailink_server {

std::vector<std::string> purge_expired_rooms(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    const std::string& now_utc);

}  // namespace sekailink_server
