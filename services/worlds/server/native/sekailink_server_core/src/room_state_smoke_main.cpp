#include "sekailink_server/room_state.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    RoomStateSnapshot snapshot;
    snapshot.room_id = "room-001";
    snapshot.room_type = RoomType::Async;
    snapshot.connection_state = ConnectionState::Online;
    snapshot.game = "A Link to the Past";
    snapshot.team_id = 0;
    snapshot.slot_id = 1;
    snapshot.slot_name = "Jade";
    snapshot.slot_alias = "Sekai Jade";
    snapshot.players.push_back(PlayerRoomView{0, 1, "Jade", "Sekai Jade", "A Link to the Past", true});
    snapshot.players.push_back(PlayerRoomView{0, 2, "Cloud", "Cloud", "A Link to the Past", false});
    snapshot.checked_locations = {1001};
    snapshot.missing_locations = {1002, 1003};
    snapshot.received_items.push_back(ReceivedItemView{0, 2001, "Bow", 1001, 2, "Cloud", 0});
    snapshot.stored_data["lttp_big_key_eastern"] = true;
    snapshot.milestones = {"dark_world"};
    snapshot.notifications = {"go_mode_ready"};
    snapshot.patch_url = "https://cdn.sekailink.com/patch.apz";
    snapshot.tracker_pack = "alttp-pack";
    snapshot.tracker_variant = "Map Tracker - AP";
    snapshot.seed_id = "seed-123";
    snapshot.seed_hash = "hash-abc";
    snapshot.slot_data = {{"mode", "open"}, {"shuffle", "none"}};
    snapshot.location_to_item[1002] = 2;
    snapshot.location_to_item_id[1002] = 2002;
    snapshot.location_names[1001] = "Link's House";
    snapshot.player_aliases[1] = "Sekai Jade";
    snapshot.emu_connected = true;
    snapshot.tracker_connected = true;
    snapshot.sni_required = true;
    snapshot.memory_backend_state = "required";
    snapshot.capabilities.supports_async = true;
    snapshot.capabilities.supports_tracker_state = true;
    snapshot.capabilities.supports_mobile_summary = true;
    snapshot.capabilities.supports_sni_local = true;
    snapshot.async_state = AsyncState{
        .expires_at = "2026-04-01T00:00:00Z",
        .last_player_activity = "2026-03-26T12:00:00Z",
        .allowed_players = {1, 2},
        .daily_summary_state = "pending",
        .async_notification_state = "queued",
        .suspend_state = std::nullopt,
    };

    const auto payload = to_json(snapshot);
    require(payload["room_type"] == "async", "room_type");
    require(payload["connection_state"] == "online", "connection_state");
    require(payload["slot_alias"] == "Sekai Jade", "slot_alias");
    require(payload["received_items"][0]["item_name"] == "Bow", "received_item");
    require(payload["capabilities"]["supports_mobile_summary"] == true, "capability");
    require(payload["async_state"]["allowed_players"].size() == 2, "async_state");
    require(payload["memory_backend_state"] == "required", "memory_backend_state");

    std::cout << payload.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_server_room_state_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
