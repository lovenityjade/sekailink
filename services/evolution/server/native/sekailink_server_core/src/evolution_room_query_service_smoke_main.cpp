#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_projection_sqlite.hpp"

namespace {

std::string http_get(std::uint16_t port, const std::string& path, const std::string& bearer_token) {
#ifdef _WIN32
  throw std::runtime_error("evolution_room_query_service_smoke_not_supported_on_windows_yet");
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
  std::cerr << "sekailink_evolution_room_query_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_evolution_room_query_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path sqlite_path = root / "room_projection.sqlite3";
    const fs::path config_path = root / "evolution_room_query.json";
    const fs::path state_path = root / "evolution_room_query_state.json";

    sekailink_server::RoomProjectionSqliteStore sqlite_store(sqlite_path);
    sekailink_server::RoomStateSnapshot snapshot;
    snapshot.room_id = "service-room";
    snapshot.room_type = sekailink_server::RoomType::Live;
    snapshot.connection_state = sekailink_server::ConnectionState::Online;
    snapshot.game = "alttp";
    snapshot.team_id = 0;
    snapshot.slot_id = 1;
    snapshot.slot_name = "Jade";
    snapshot.slot_alias = "Jade";
    snapshot.generated_at = "2026-03-26T19:30:00Z";
    std::vector<sekailink_server::RoomEvent> events{
        {.event_type = "location_check", .timestamp = "2026-03-26T19:31:00Z", .payload = {{"location_id", 42}}},
        {.event_type = "client_report_ingested",
         .timestamp = "2026-03-26T19:31:30Z",
         .payload = {{"source", "sekaiemu"}, {"severity", "error"}}},
    };
    std::vector<sekailink_server::ClientReport> reports{
        {.report_type = "runtime_error",
         .source = "sekaiemu",
         .severity = "error",
         .message = "service smoke client report",
         .timestamp = "2026-03-26T19:32:00Z",
         .game = std::make_optional<std::string>("alttp"),
         .runtime = std::make_optional<std::string>("snes")},
    };
    sqlite_store.append_batch(sekailink_server::build_projection_batch(snapshot, events, reports));

    {
      std::ofstream config_stream(config_path);
      config_stream << "{\n"
                    << "  \"http_port\": 19094,\n"
                    << "  \"auth_token\": \"evolution-service-secret\",\n"
                    << "  \"projection_backend\": \"sqlite\",\n"
                    << "  \"projection_target\": \"" << sqlite_path.string() << "\",\n"
                    << "  \"state_path\": \"" << state_path.string() << "\"\n"
                    << "}\n";
    }

    const std::string binary = "/home/nobara-user/sekailink/build-server-core/sekailink_evolution_room_query_service";
    const pid_t pid = ::fork();
    if (pid < 0) {
      throw std::runtime_error("fork_failed");
    }
    if (pid == 0) {
      ::execl(binary.c_str(), binary.c_str(), "--config", config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }

    bool ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto health = http_get(19094, "/health", "ignored");
        if (health.find("200 OK") != std::string::npos) {
          ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!ready) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_query_not_ready");
    }

    const auto rooms = http_get(19094, "/rooms?limit=1&offset=0", "evolution-service-secret");
    if (rooms.find("service-room") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_rooms_failed");
    }
    if (rooms.find("\"offset\":0") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_rooms_offset_missing");
    }
    if (!fs::exists(state_path)) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_state_missing");
    }

    const auto room_events = http_get(19094, "/rooms/service-room/events?limit=1&offset=0", "evolution-service-secret");
    if (room_events.find("location_check") == std::string::npos || room_events.find("\"limit\":1") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_events_failed");
    }

    const auto room_events_filtered = http_get(
        19094,
        "/rooms/service-room/events?event_type=client_report_ingested&severity=error&source=sekaiemu",
        "evolution-service-secret");
    if (room_events_filtered.find("\"source\":\"sekaiemu\"") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_events_source_filter_failed");
    }

    const auto room_reports = http_get(19094, "/rooms/service-room/client-reports?limit=1&offset=0", "evolution-service-secret");
    if (room_reports.find("service smoke client report") == std::string::npos || room_reports.find("\"offset\":0") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_reports_failed");
    }

    const auto diagnostics = http_get(19094, "/rooms/service-room/diagnostics", "evolution-service-secret");
    if (diagnostics.find("\"event_count\":2") == std::string::npos ||
        diagnostics.find("\"client_report_count\":1") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_diagnostics_failed");
    }

    const auto unauthorized = http_get(19094, "/rooms", "wrong-secret");
    if (unauthorized.find("401 Unauthorized") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("evolution_service_unauthorized_failed");
    }

    ::kill(pid, SIGTERM);
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      throw std::runtime_error("evolution_service_exit_failed");
    }

    std::cout << "evolution_room_query_service_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_evolution_room_query_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
