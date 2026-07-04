#include "sekailink_server/room_server_http.hpp"

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
    });
    require(created, "room_create_failed");
    auto* room = registry.find_room("http-room-1");
    require(room != nullptr, "room_missing");
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
    const auto events = service.handle_request("GET", "/rooms/http-room-1/events");
    const auto reports = service.handle_request("GET", "/rooms/http-room-1/client-reports");
    const auto missing = service.handle_request("GET", "/rooms/missing-room/snapshot");

    require(health.status_code == 200, "health_status");
    require(rooms.body.find("http-room-1") != std::string::npos, "rooms_body");
    require(snapshot.body.find("\"room_id\":\"http-room-1\"") != std::string::npos, "snapshot_body");
    require(events.body.find("location_checked") != std::string::npos, "events_body");
    require(reports.body.find("runtime_error") != std::string::npos, "reports_body");
    require(missing.status_code == 404, "missing_status");

    std::cout << "health=" << health.status_code << "\n";
    std::cout << "rooms=" << rooms.status_code << "\n";
    std::cout << "snapshot=" << snapshot.status_code << "\n";
    std::cout << "events=" << events.status_code << "\n";
    std::cout << "reports=" << reports.status_code << "\n";
    std::cout << "missing=" << missing.status_code << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_http_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
