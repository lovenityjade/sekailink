#include "sekailink_server/room_retention.hpp"

namespace sekailink_server {

std::vector<std::string> purge_expired_rooms(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    const std::string& now_utc) {
  const auto expired_room_ids = registry.expired_room_ids(now_utc);
  for (const auto& room_id : expired_room_ids) {
    if (audit_store != nullptr) {
      if (const auto* room = registry.find_room(room_id); room != nullptr) {
        audit_store->append_event(room_id, RoomEvent{
            .event_type = "room_expired",
            .timestamp = now_utc,
            .payload = {
                {"reason", "expired"},
                {"expired_at_check", now_utc},
            },
        });
      }
    }
    registry.remove_room(room_id);
  }
  return expired_room_ids;
}

}  // namespace sekailink_server
