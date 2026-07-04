#pragma once

#include "sekailink_server/room_audit_store.hpp"
#include "sekailink_server/room_registry.hpp"

#include <optional>
#include <string>

namespace sekailink_server {

RoomEvent room_event_from_json(const nlohmann::json& value);
ClientReport client_report_from_json(const nlohmann::json& value);
RoomStateSnapshot room_state_snapshot_from_json(const nlohmann::json& value);
std::optional<RoomStateSnapshot> load_latest_room_snapshot(const RoomAuditStore& audit_store, const std::string& room_id);
std::optional<RoomSession> load_room_session_from_audit(const RoomAuditStore& audit_store, const std::string& room_id);
bool restore_room_from_audit(RoomRegistry& registry, const RoomAuditStore& audit_store, const std::string& room_id);
std::vector<std::string> restore_all_rooms_from_audit(RoomRegistry& registry, const RoomAuditStore& audit_store);

}  // namespace sekailink_server
