#include "sekailink_server/room_retention.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_retention_smoke";
    std::filesystem::remove_all(root);

    sekailink_server::RoomRegistry registry;
    sekailink_server::RoomAuditStore audit_store(root);

    require(registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "expired-room",
        .room_type = sekailink_server::RoomType::Async,
        .game = "alttp",
        .slot_id = 1,
        .slot_name = "Expired",
        .slot_alias = "Expired",
        .expires_at = std::string("2026-03-20T00:00:00Z"),
    }), "expired_create_failed");
    require(registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "live-room",
        .room_type = sekailink_server::RoomType::Async,
        .game = "alttp",
        .slot_id = 2,
        .slot_name = "Live",
        .slot_alias = "Live",
        .expires_at = std::string("2026-03-30T00:00:00Z"),
    }), "live_create_failed");

    const auto purged = sekailink_server::purge_expired_rooms(registry, &audit_store, "2026-03-26T17:30:00Z");
    require(purged.size() == 1, "purged_size");
    require(purged.front() == "expired-room", "purged_room");
    require(registry.find_room("expired-room") == nullptr, "expired_room_removed");
    require(registry.find_room("live-room") != nullptr, "live_room_kept");

    const auto events = audit_store.read_events("expired-room");
    require(!events.empty(), "expired_events_empty");
    require(events.back().at("event_type").get<std::string>() == "room_expired", "expired_event_type");

    std::cout << "purged_rooms=" << purged.size() << "\n";
    std::cout << "expired_event=" << events.back().at("event_type").get<std::string>() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_retention_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
