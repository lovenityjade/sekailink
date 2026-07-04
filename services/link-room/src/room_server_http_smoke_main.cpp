#include "sekailink_server/room_server_http.hpp"

#include "nlohmann/json.hpp"

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
    sekailink_server::RoomRegistry registry;
    const auto created = registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "http-room-1",
        .room_type = sekailink_server::RoomType::Async,
        .game = "alttp",
        .team_id = 0,
        .slot_id = 1,
        .slot_name = "Jade",
        .slot_alias = "Jade",
        .seed_name = "seed-http-name-1",
        .seed_id = "seed-http-1",
        .seed_hash = "hash-http-1",
    });
    require(created, "room_create_failed");
    auto* room = registry.find_room("http-room-1");
    require(room != nullptr, "room_missing");
    room->heartbeat_runtime({
        .runtime_kind = "sklmi",
        .runtime_session_name = "http-live-1",
        .driver_instance_id = "driver-http-1",
        .linkedworld_id = "alttp",
        .core_profile = "snes_v1",
        .connected = true,
    });
    room->record_check(1234);
    sekailink_server::ClientReport report;
    report.report_type = "runtime_error";
    report.source = "client";
    report.severity = "error";
    report.message = "http smoke";
    report.timestamp = sekailink_server::utc_timestamp_now();
    report.room_id = std::string("http-room-1");
    report.details = nlohmann::json{{"code", "E_HTTP"}};
    room->ingest_client_report(std::move(report));

    const sekailink_server::RoomServerHttpService service(registry);

    const auto health = service.handle_request("GET", "/health");
    const auto rooms = service.handle_request("GET", "/rooms");
    const auto snapshot = service.handle_request("GET", "/rooms/http-room-1/snapshot");
    const auto summary = service.handle_request("GET", "/rooms/http-room-1/summary");
    const auto sync = service.handle_request("GET", "/rooms/http-room-1/sync");
    const auto events = service.handle_request("GET", "/rooms/http-room-1/events");
    const auto reports = service.handle_request("GET", "/rooms/http-room-1/client-reports");
    const auto missing = service.handle_request("GET", "/rooms/missing-room/snapshot");

    require(health.status_code == 200, "health_status");
    const auto health_json = nlohmann::json::parse(health.body);
    require(health_json.at("loopback_only").get<bool>() == true, "health_loopback_only");
    require(health_json.at("room_count").get<std::size_t>() == 1U, "health_room_count");
    require(health_json.at("admin_auth_enabled").get<bool>() == false, "health_auth_disabled");
    require(rooms.body.find("http-room-1") != std::string::npos, "rooms_body");
    require(snapshot.body.find("\"room_id\":\"http-room-1\"") != std::string::npos, "snapshot_body");
    require(summary.body.find("\"seed_name\":\"seed-http-name-1\"") != std::string::npos, "summary_seed_name");
    require(summary.body.find("\"runtime_connected\":true") != std::string::npos, "summary_runtime_connected");
    require(sync.body.find("\"driver_instance_id\":\"driver-http-1\"") != std::string::npos, "sync_driver_instance_id");
    require(sync.body.find("\"checked_count\":1") != std::string::npos, "sync_checked_count");
    require(events.body.find("location_checked") != std::string::npos, "events_body");
    require(reports.body.find("runtime_error") != std::string::npos, "reports_body");
    require(missing.status_code == 404, "missing_status");

    std::cout << "health=" << health.status_code << "\n";
    std::cout << "rooms=" << rooms.status_code << "\n";
    std::cout << "snapshot=" << snapshot.status_code << "\n";
    std::cout << "summary=" << summary.status_code << "\n";
    std::cout << "sync=" << sync.status_code << "\n";
    std::cout << "events=" << events.status_code << "\n";
    std::cout << "reports=" << reports.status_code << "\n";
    std::cout << "missing=" << missing.status_code << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_http_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
