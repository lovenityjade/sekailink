#include "sekailink_server/evolution_room_query_service.hpp"
#include "sekailink_server/room_projection.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

std::string http_get(std::uint16_t port, const std::string& path, const std::string& bearer_token) {
#ifdef _WIN32
  throw std::runtime_error("evolution_room_query_smoke_not_supported_on_windows_yet");
#else
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("http_socket_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("http_connect_failed");
  }

  const std::string request =
      "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer " + bearer_token + "\r\nConnection: close\r\n\r\n";
  if (::send(sock, request.data(), request.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("http_send_failed");
  }

  std::string response;
  char buffer[4096];
  while (true) {
    const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(sock);
  return response;
#endif
}

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_evolution_room_query_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path sqlite_path = fs::temp_directory_path() / "sekailink_evolution_room_query_smoke.sqlite3";
    fs::remove(sqlite_path);

    sekailink_server::RoomProjectionSqliteStore sqlite_store(sqlite_path);
    sekailink_server::RoomStateSnapshot snapshot;
    snapshot.room_id = "evolution-room";
    snapshot.room_type = sekailink_server::RoomType::Live;
    snapshot.connection_state = sekailink_server::ConnectionState::Online;
    snapshot.game = "alttp";
    snapshot.team_id = 0;
    snapshot.slot_id = 1;
    snapshot.slot_name = "Jade";
    snapshot.slot_alias = "Jade";
    snapshot.generated_at = "2026-03-26T19:00:00Z";
    std::vector<sekailink_server::RoomEvent> events{
        {.event_type = "location_check", .timestamp = "2026-03-26T19:01:00Z", .payload = {{"location_id", 1}}},
        {.event_type = "item_received", .timestamp = "2026-03-26T19:02:00Z", .payload = {{"item_id", 2}}},
        {.event_type = "client_report_ingested",
         .timestamp = "2026-03-26T19:03:30Z",
         .payload = {{"source", "sekaiemu"}, {"severity", "error"}}},
    };
    std::vector<sekailink_server::ClientReport> reports{
        {.report_type = "runtime_error",
         .source = "sekaiemu",
         .severity = "error",
         .message = "first report",
         .timestamp = "2026-03-26T19:03:00Z"},
        {.report_type = "runtime_warning",
         .source = "sekaiemu",
         .severity = "warning",
         .message = "second report",
         .timestamp = "2026-03-26T19:04:00Z"},
    };
    sqlite_store.append_batch(sekailink_server::build_projection_batch(snapshot, events, reports));

    auto config = sekailink_server::EvolutionRoomQueryConfig{
        .http_port = 0,
        .auth_token = std::string("query-secret"),
        .projection_backend = sekailink_server::ProjectionBackend::Sqlite,
        .projection_target = sqlite_path,
    };
    sekailink_server::EvolutionRoomQueryHttpServer server(std::move(config));
    if (!server.start()) {
      throw std::runtime_error("evolution_query_server_start_failed");
    }

    std::atomic<bool> stop_requested{false};
    std::thread server_thread([&]() {
      while (!stop_requested) {
        try {
          server.serve_one();
        } catch (...) {
          if (stop_requested) {
            break;
          }
        }
      }
    });

    const auto health = http_get(server.port(), "/health", "wrong");
    if (health.find("200 OK") == std::string::npos) {
      throw std::runtime_error("evolution_health_failed");
    }

    const auto rooms = http_get(server.port(), "/rooms", "query-secret");
    if (rooms.find("200 OK") == std::string::npos || rooms.find("evolution-room") == std::string::npos) {
      throw std::runtime_error("evolution_rooms_failed");
    }

    const auto snapshot_response = http_get(server.port(), "/rooms/evolution-room", "query-secret");
    if (snapshot_response.find("200 OK") == std::string::npos || snapshot_response.find("\"slot_name\":\"Jade\"") == std::string::npos) {
      throw std::runtime_error("evolution_snapshot_failed");
    }

    const auto events_response = http_get(server.port(), "/rooms/evolution-room/events?limit=1&offset=1", "query-secret");
    if (events_response.find("200 OK") == std::string::npos || events_response.find("\"offset\":1") == std::string::npos ||
        events_response.find("item_received") == std::string::npos) {
      throw std::runtime_error("evolution_events_paging_failed");
    }

    const auto source_filtered_events =
        http_get(server.port(), "/rooms/evolution-room/events?event_type=client_report_ingested&severity=error&source=sekaiemu", "query-secret");
    if (source_filtered_events.find("200 OK") == std::string::npos || source_filtered_events.find("\"source\":\"sekaiemu\"") == std::string::npos) {
      throw std::runtime_error("evolution_events_source_filter_failed");
    }

    const auto reports_response = http_get(server.port(), "/rooms/evolution-room/client-reports?limit=1&offset=0", "query-secret");
    if (reports_response.find("200 OK") == std::string::npos || reports_response.find("\"limit\":1") == std::string::npos ||
        reports_response.find("first report") == std::string::npos) {
      throw std::runtime_error("evolution_reports_paging_failed");
    }

    const auto unauthorized = http_get(server.port(), "/rooms", "wrong");
    if (unauthorized.find("401 Unauthorized") == std::string::npos) {
      throw std::runtime_error("evolution_unauthorized_failed");
    }

    stop_requested = true;
    server.stop();
    if (server_thread.joinable()) {
      server_thread.join();
    }

    std::cout << "evolution_room_query_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_evolution_room_query_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
