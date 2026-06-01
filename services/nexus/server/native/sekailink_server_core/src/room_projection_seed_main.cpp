#include "sekailink_server/room_projection_factory.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

using namespace sekailink_server;

int main(int argc, char** argv) {
  try {
    ProjectionBackend backend = ProjectionBackend::Jsonl;
    std::string target;
    std::string room_id;
    std::string game = "alttp";
    int slot_id = 1;
    std::string slot_name = "Jade";
    std::string slot_alias = "Jade";
    std::optional<std::int64_t> location_id;
    std::optional<std::string> report_message;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--backend" && i + 1 < argc) {
        backend = parse_projection_backend(argv[++i]);
      } else if (arg == "--target" && i + 1 < argc) {
        target = argv[++i];
      } else if (arg == "--room-id" && i + 1 < argc) {
        room_id = argv[++i];
      } else if (arg == "--game" && i + 1 < argc) {
        game = argv[++i];
      } else if (arg == "--slot-id" && i + 1 < argc) {
        slot_id = std::stoi(argv[++i]);
      } else if (arg == "--slot-name" && i + 1 < argc) {
        slot_name = argv[++i];
      } else if (arg == "--slot-alias" && i + 1 < argc) {
        slot_alias = argv[++i];
      } else if (arg == "--location-id" && i + 1 < argc) {
        location_id = std::stoll(argv[++i]);
      } else if (arg == "--report-message" && i + 1 < argc) {
        report_message = argv[++i];
      }
    }

    if (target.empty() || room_id.empty()) {
      std::cerr << "usage: sekailink_room_projection_seed --backend <jsonl|sqlite|mysql> --target <target> --room-id <id> [options]\n";
      return EXIT_FAILURE;
    }

    auto store = make_projection_store(backend, target);

    RoomStateSnapshot snapshot;
    snapshot.room_id = room_id;
    snapshot.room_type = RoomType::Live;
    snapshot.connection_state = ConnectionState::Online;
    snapshot.game = game;
    snapshot.team_id = 0;
    snapshot.slot_id = slot_id;
    snapshot.slot_name = slot_name;
    snapshot.slot_alias = slot_alias;
    snapshot.generated_at = utc_timestamp_now();

    std::vector<RoomEvent> events;
    if (location_id.has_value()) {
      snapshot.checked_locations.push_back(*location_id);
      events.push_back(RoomEvent{
          .event_type = "location_checked",
          .timestamp = utc_timestamp_now(),
          .payload = {{"location_id", *location_id}},
      });
    }

    std::vector<ClientReport> reports;
    if (report_message.has_value()) {
      reports.push_back(ClientReport{
          .report_type = "runtime_error",
          .source = "sekaiemu",
          .severity = "error",
          .message = *report_message,
          .timestamp = utc_timestamp_now(),
          .room_id = room_id,
          .game = game,
      });
    }

    store->append_batch(build_projection_batch(snapshot, events, reports));
    std::cout << "seeded_room_id=" << room_id << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_projection_seed failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
