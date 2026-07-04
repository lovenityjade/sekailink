#include "sekailink_server/room_projection_restore.hpp"

#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_session.hpp"

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
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_projection_restore_smoke";
    std::filesystem::remove_all(root);

    sekailink_server::RoomSession session_a(sekailink_server::RoomSessionConfig{
        .room_id = "projection-room-1",
        .room_type = sekailink_server::RoomType::Async,
        .game = "alttp",
        .slot_id = 1,
        .slot_name = "Projection1",
        .slot_alias = "Projection1",
    });
    session_a.record_check(101);

    sekailink_server::RoomSession session_b(sekailink_server::RoomSessionConfig{
        .room_id = "projection-room-2",
        .room_type = sekailink_server::RoomType::Live,
        .game = "alttp",
        .slot_id = 2,
        .slot_name = "Projection2",
        .slot_alias = "Projection2",
    });
    session_b.record_check(202);

    sekailink_server::RoomProjectionSqliteStore store(root / "projection.sqlite3");
    store.append_batch(sekailink_server::build_projection_batch(session_a.snapshot(), session_a.events(), session_a.client_reports()));
    store.append_batch(sekailink_server::build_projection_batch(session_b.snapshot(), session_b.events(), session_b.client_reports()));

    sekailink_server::RoomRegistry registry;
    const auto restored = sekailink_server::restore_rooms_from_projection_store(registry, store);
    require(restored.size() == 2, "restored_size");
    require(registry.find_room("projection-room-1") != nullptr, "projection_room_1");
    require(registry.find_room("projection-room-2") != nullptr, "projection_room_2");

    std::cout << "projection_restored_rooms=" << restored.size() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_projection_restore_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
