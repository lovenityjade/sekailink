#include "sekailink_server/game_room_projection.hpp"
#include "sekailink_server/game_server_protocol.hpp"

#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    const nlohmann::json imported_ap = {
        {"slot_info",
         {
             {"1", {{"name", "Jade"}, {"game", "EarthBound"}, {"type", "player"}}},
             {"2", {{"name", "Ness"}, {"game", "EarthBound"}, {"type", "player"}}},
         }},
        {"locations",
         {
             {"1",
              {
                  {"1001",
                   {
                       {"receiver_slot", 2},
                       {"item_id", 5001},
                       {"item_name", "Sound Stone"},
                       {"location_name", "Onett Drugstore"},
                       {"flags", 0},
                   }},
              }},
             {"2",
              {
                  {"2001",
                   {
                       {"receiver_slot", 1},
                       {"item_id", 6001},
                       {"item_name", "Mr. Baseball Cap"},
                       {"location_name", "Twoson Burglin Park"},
                       {"flags", 0},
                   }},
              }},
         }},
        {"game_options", {{"release_mode", "enabled"}}},
    };

    GameSessionRegistry game_registry;
    std::string error;
    require(game_registry.create_session_from_archipelago_import(
                "projection-earthbound",
                imported_ap,
                ArchipelagoWorldImportOptions{
                    .world_id = "world-proj-1",
                    .world_version = "1.0",
                    .seed_id = "seed-proj-1",
                    .seed_hash = "P1",
                    .linkedworld_id = "earthbound-proj",
                },
                &error),
            error);

    auto* session = game_registry.find_session("projection-earthbound");
    require(session != nullptr, "session_missing");

    auto runtime_ticket = session->issue_session_ticket(
        1,
        GameClientKind::Runtime,
        std::string("driver-proj-1"),
        std::string("earthbound-proj"),
        std::string("bsnes"),
        &error);
    require(runtime_ticket.has_value(), "runtime_ticket");

    const auto result = session->apply_runtime_event(RuntimeEvent{
        .session_token = runtime_ticket->session_token,
        .slot_id = 1,
        .driver_instance_id = "driver-proj-1",
        .linkedworld_id = "earthbound-proj",
        .core_profile = "bsnes",
        .event_type = "location_checked",
        .canonical_id = 1001,
    });
    require(result.accepted, "runtime_event");

    RoomRegistry room_registry;
    sync_all_game_session_projections(game_registry, room_registry);

    const auto room_ids = room_registry.list_room_ids();
    require(room_ids.size() == 2, "room_count");

    const auto* room1 = room_registry.find_room("game::projection-earthbound::slot::1");
    const auto* room2 = room_registry.find_room("game::projection-earthbound::slot::2");
    require(room1 != nullptr, "room1");
    require(room2 != nullptr, "room2");

    const auto snapshot1 = room1->snapshot();
    const auto snapshot2 = room2->snapshot();
    require(snapshot1.checked_locations.size() == 1, "slot1_checked");
    require(snapshot1.missing_locations.empty(), "slot1_missing");
    require(snapshot2.received_items.size() == 1, "slot2_received");
    require(snapshot2.received_items.at(0).item_name == "Sound Stone", "slot2_item_name");
    require(snapshot2.slot_data.at("session_name").get<std::string>() == "projection-earthbound", "slot_data_session_name");

    std::cout << "sekailink_game_room_projection_smoke_ok\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_game_room_projection_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
