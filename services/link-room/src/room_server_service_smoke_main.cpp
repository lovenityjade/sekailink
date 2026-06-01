#include "sekailink_server/room_server_tcp.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "nlohmann/json.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::string http_get(std::uint16_t port, const std::string& path, const std::string& bearer_token) {
#ifdef _WIN32
  throw std::runtime_error("room_server_service_smoke_not_supported_on_windows_yet");
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
  std::cerr << "sekailink_room_server_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_room_server_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path config_path = root / "room_server_config.json";
    const fs::path state_path = root / "room_server_state.json";
    const fs::path audit_root = root / "audit";
    const fs::path projection_root = root / "projection";

    {
      std::ofstream config_stream(config_path);
      config_stream << R"({
  "tcp_port": 0,
  "http_port": 0,
  "audit_root": ")" << audit_root.string() << R"(",
  "projection_root": ")" << projection_root.string() << R"(",
  "projection_backend": "jsonl",
  "auth_policy": {
    "admin_token": "admin-smoke-token",
    "runtime_token": "runtime-smoke-token",
    "client_report_token": "client-smoke-token"
  }
}
)";
    }

    const std::string binary = "/home/nobara-user/sekailink/build-server-core/sekailink_room_server_service";
    const pid_t pid = ::fork();
    if (pid < 0) {
      throw std::runtime_error("fork_failed");
    }
    if (pid == 0) {
      ::execl(
          binary.c_str(),
          binary.c_str(),
          "--config",
          config_path.c_str(),
          "--state-file",
          state_path.c_str(),
          static_cast<char*>(nullptr));
      _exit(127);
    }

    bool state_ready = false;
    for (int i = 0; i < 50; ++i) {
      if (fs::exists(state_path) && fs::file_size(state_path) > 0) {
        state_ready = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!state_ready) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("state_file_not_ready");
    }

    nlohmann::json state;
    {
      std::ifstream state_stream(state_path);
      state_stream >> state;
    }

    if (state.at("service").get<std::string>() != "sekailink_room_server") {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("unexpected_service_name");
    }
    if (state.at("status").get<std::string>() != "running") {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("unexpected_service_status");
    }
    if (!state.contains("started_at") || state.at("started_at").is_null()) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("missing_started_at");
    }
    if (state.value("boot_room_count", 999U) != 0U) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("unexpected_boot_room_count");
    }
    if (state.value("effective_tcp_host", std::string()) != "127.0.0.1") {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("unexpected_effective_tcp_host");
    }
    if (state.value("effective_http_host", std::string()) != "127.0.0.1") {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("unexpected_effective_http_host");
    }
    if (!state.value("loopback_only", false)) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("expected_loopback_only");
    }
    if (!state.value("admin_auth_enabled", false) || !state.value("runtime_auth_enabled", false) ||
        !state.value("client_report_auth_enabled", false)) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("missing_auth_enabled_flags");
    }

    const auto tcp_port = state.at("effective_tcp_port").get<std::uint16_t>();
    const auto http_port = state.at("effective_http_port").get<std::uint16_t>();

    const auto create_response = sekailink_server::tcp_send_json_line(
        "127.0.0.1",
        tcp_port,
        nlohmann::json{
            {"channel", "admin"},
            {"auth_token", "admin-smoke-token"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "service-room"},
                 {"room_type", "live"},
                 {"game", "alttp"},
                 {"slot_id", 1},
                 {"slot_name", "Jade"},
             }},
        });
    if (create_response.find("\"ok\":true") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("tcp_create_room_failed: " + create_response);
    }

    const auto http_response = http_get(http_port, "/rooms/service-room/snapshot", "admin-smoke-token");
    if (http_response.find("200 OK") == std::string::npos || http_response.find("service-room") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("http_snapshot_failed");
    }

    ::kill(pid, SIGTERM);
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      throw std::runtime_error("service_exit_failed");
    }

    std::cout << "room_server_service_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
