#include "sekailink_server/game_session.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

GameWorldPackage make_package() {
  return GameWorldPackage{
      .world_id = "alttp-world-1",
      .world_version = "1.0",
      .seed_id = "seed-123",
      .seed_hash = "hash-abc",
      .linkedworld_id = "alttp",
      .server_rules = nlohmann::json::object({{"hint_cost", 20}}),
      .slots = {
          {1, GameWorldSlot{.slot_id = 1, .name = "Jade", .game = "A Link to the Past"}},
          {2, GameWorldSlot{.slot_id = 2, .name = "Cloud", .game = "A Link to the Past"}},
      },
      .locations = {
          {1001,
           GameWorldLocation{
               .location_id = 1001,
               .owner_slot = 1,
               .receiver_slot = 2,
               .item_id = 2001,
               .location_name = "Link's House",
               .item_name = "Bow",
               .flags = 0,
           }},
      },
  };
}

}  // namespace

int main() {
  try {
    GameSession session(make_package());

    std::string error;
    const auto runtime_ticket = session.issue_session_ticket(
        1,
        GameClientKind::Runtime,
        "driver-1",
        "alttp",
        "snes_v1",
        &error);
    require(runtime_ticket.has_value(), "runtime_ticket");

    const auto receiver_ticket = session.issue_session_ticket(2, GameClientKind::Core, std::nullopt, std::nullopt, std::nullopt, &error);
    require(receiver_ticket.has_value(), "receiver_ticket");

    const auto accepted = session.apply_runtime_event(RuntimeEvent{
        .session_token = runtime_ticket->session_token,
        .slot_id = 1,
        .driver_instance_id = "driver-1",
        .linkedworld_id = "alttp",
        .core_profile = "snes_v1",
        .event_type = "location_checked",
        .canonical_id = 1001,
    });
    require(accepted.accepted, "runtime_event_accepted");
    require(accepted.created_delivery_ids.size() == 1, "delivery_created");

    const auto duplicate = session.apply_runtime_event(RuntimeEvent{
        .session_token = runtime_ticket->session_token,
        .slot_id = 1,
        .driver_instance_id = "driver-1",
        .linkedworld_id = "alttp",
        .core_profile = "snes_v1",
        .event_type = "location_checked",
        .canonical_id = 1001,
    });
    require(!duplicate.accepted && duplicate.duplicate, "duplicate_check_rejected");

    const auto bad_token = session.apply_runtime_event(RuntimeEvent{
        .session_token = "bad-token",
        .slot_id = 1,
        .driver_instance_id = "driver-1",
        .linkedworld_id = "alttp",
        .core_profile = "snes_v1",
        .event_type = "location_checked",
        .canonical_id = 1001,
    });
    require(!bad_token.accepted && bad_token.reason == "invalid_session_token", "bad_token");

    const auto pending = session.pending_items_for_slot(2, receiver_ticket->session_token);
    require(pending.size() == 1, "pending_items");
    require(pending.front().item_name == "Bow", "pending_item_name");
    require(session.acknowledge_delivery(2, pending.front().delivery_id, receiver_ticket->session_token), "ack_delivery");
    require(session.pending_items_for_slot(2, receiver_ticket->session_token).empty(), "pending_after_ack");

    const auto save = session.export_save_state();
    require(save.delivered_items.size() == 1, "save_delivery_count");
    const auto restored = GameSession::from_save(make_package(), save, &error);
    require(restored.has_value(), "restore_session");
    require(restored->pending_items_for_slot(2, receiver_ticket->session_token).empty(), "restored_ack_state_without_ticket_is_safe");

    std::cout << "sekailink_game_session_smoke_ok\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_game_session_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
