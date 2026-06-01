#include "sekailink_server/room_state.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace sekailink_server;

int main() {
  RoomStateSnapshot snapshot;
  snapshot.room_id = "perf-room";
  snapshot.room_type = RoomType::Async;
  snapshot.connection_state = ConnectionState::Online;
  snapshot.game = "A Link to the Past";
  snapshot.team_id = 0;
  snapshot.slot_id = 1;
  snapshot.slot_name = "Jade";
  snapshot.slot_alias = "Sekai Jade";
  snapshot.patch_url = "https://cdn.sekailink.com/perf.apz";
  snapshot.tracker_pack = "alttp-pack";
  snapshot.tracker_variant = "Map Tracker - AP";
  snapshot.seed_id = "seed-perf";
  snapshot.seed_hash = "hash-perf";
  snapshot.emu_connected = true;
  snapshot.tracker_connected = true;
  snapshot.sni_required = true;
  snapshot.memory_backend_state = "required";
  snapshot.capabilities.supports_async = true;
  snapshot.capabilities.supports_mobile_summary = true;
  snapshot.capabilities.supports_sni_local = true;
  snapshot.async_state = AsyncState{
      .expires_at = "2026-04-01T00:00:00Z",
      .last_player_activity = "2026-03-26T12:00:00Z",
      .allowed_players = {1, 2, 3, 4},
      .daily_summary_state = "pending",
      .async_notification_state = "queued",
      .suspend_state = std::nullopt,
  };

  for (int i = 0; i < 64; ++i) {
    snapshot.players.push_back(PlayerRoomView{0, i + 1, "Player" + std::to_string(i + 1),
                                              "Alias" + std::to_string(i + 1),
                                              "A Link to the Past", i % 2 == 0});
  }
  for (int i = 0; i < 1024; ++i) {
    snapshot.checked_locations.push_back(100000 + i);
    snapshot.missing_locations.push_back(200000 + i);
    snapshot.location_to_item[300000 + i] = (i % 64) + 1;
    snapshot.location_to_item_id[300000 + i] = 400000 + i;
    snapshot.location_names[300000 + i] = "Location " + std::to_string(i);
  }
  for (int i = 0; i < 256; ++i) {
    snapshot.received_items.push_back(ReceivedItemView{
        i, 500000 + i, "Item " + std::to_string(i), 300000 + i, (i % 64) + 1,
        "Alias" + std::to_string((i % 64) + 1), 0});
  }
  snapshot.stored_data["mode"] = "async";
  snapshot.stored_data["hints_enabled"] = true;
  snapshot.slot_data = {{"mode", "open"}, {"shuffle", "none"}, {"goal", "ganon"}};

  constexpr int kIterations = 2000;
  const auto start = std::chrono::steady_clock::now();
  std::size_t total_bytes = 0;
  for (int i = 0; i < kIterations; ++i) {
    total_bytes += to_json(snapshot).dump().size();
  }
  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "iterations=" << kIterations << "\n";
  std::cout << "elapsed_ms=" << elapsed_ms << "\n";
  std::cout << "total_bytes=" << total_bytes << "\n";

  if (elapsed_ms <= 0 || total_bytes == 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
