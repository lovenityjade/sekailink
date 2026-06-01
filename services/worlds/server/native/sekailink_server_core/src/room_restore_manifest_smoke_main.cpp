#include "sekailink_server/room_restore.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void create_and_persist_room(
    sekailink_server::RoomRegistry& registry,
    sekailink_server::RoomAuditStore& audit_store,
    const std::string& room_id,
    int slot_id) {
  require(registry.create_room(sekailink_server::RoomSessionConfig{
      .room_id = room_id,
      .room_type = sekailink_server::RoomType::Async,
      .game = "alttp",
      .team_id = 0,
      .slot_id = slot_id,
      .slot_name = room_id,
      .slot_alias = room_id,
  }), "create_room_failed");

  auto* room = registry.find_room(room_id);
  require(room != nullptr, "room_missing");
  room->record_check(1000 + slot_id);
  audit_store.append_snapshot(room->snapshot());
}

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_restore_manifest_smoke";
    std::filesystem::remove_all(root);

    sekailink_server::RoomAuditStore audit_store(root);
    sekailink_server::RoomRegistry registry;
    create_and_persist_room(registry, audit_store, "manifest-room-1", 1);
    create_and_persist_room(registry, audit_store, "manifest-room-2", 2);

    const auto room_ids = audit_store.read_room_ids();
    require(room_ids.size() == 2, "room_index_size");

    sekailink_server::RoomRegistry restored_registry;
    const auto restored = sekailink_server::restore_all_rooms_from_audit(restored_registry, audit_store);
    require(restored.size() == 2, "restored_size");
    require(restored_registry.find_room("manifest-room-1") != nullptr, "restored_room_1");
    require(restored_registry.find_room("manifest-room-2") != nullptr, "restored_room_2");

    std::cout << "indexed_rooms=" << room_ids.size() << "\n";
    std::cout << "restored_rooms=" << restored.size() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_restore_manifest_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
