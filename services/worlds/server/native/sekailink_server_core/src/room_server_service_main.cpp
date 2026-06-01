#include "sekailink_server/room_server_config.hpp"
#include "sekailink_server/room_server_node.hpp"

#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using namespace sekailink_server;

namespace {

std::atomic<bool> g_stop_requested{false};

#ifndef _WIN32
void handle_signal(int) {
  g_stop_requested = true;
}
#endif

void apply_env_auth_overrides(RoomServerNodeConfig& config) {
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_ADMIN_TOKEN"); token != nullptr && *token != '\0') {
    config.auth_policy.admin_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN"); token != nullptr && *token != '\0') {
    config.auth_policy.runtime_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN"); token != nullptr && *token != '\0') {
    config.auth_policy.client_report_token = token;
  }
}

}  // namespace

int main(int argc, char** argv) {
  RoomServerNodeConfig config;
  config.http_port = 18081;
  std::filesystem::path state_file;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc) {
      config = load_room_server_node_config(argv[++i]);
      if (config.http_port == 0) {
        config.http_port = 18081;
      }
    } else if (arg == "--tcp-port" && i + 1 < argc) {
      config.tcp_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "--http-port" && i + 1 < argc) {
      config.http_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "--audit-root" && i + 1 < argc) {
      config.audit_root = argv[++i];
    } else if (arg == "--projection-root" && i + 1 < argc) {
      config.projection_root = argv[++i];
    } else if (arg == "--projection-backend" && i + 1 < argc) {
      config.projection_backend = parse_projection_backend(argv[++i]);
    } else if (arg == "--restore-from-audit") {
      config.restore_from_audit = true;
    } else if (arg == "--restore-from-projection") {
      config.restore_from_projection = true;
    } else if (arg == "--purge-expired-periodically") {
      config.purge_expired_periodically = true;
    } else if (arg == "--purge-interval-ms" && i + 1 < argc) {
      config.purge_interval_ms = static_cast<std::uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--state-file" && i + 1 < argc) {
      state_file = argv[++i];
    }
  }

  apply_env_auth_overrides(config);

#ifndef _WIN32
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
#endif

  RoomServerNode node(config);
  if (!node.start()) {
    return EXIT_FAILURE;
  }

  if (!state_file.empty()) {
    write_room_server_state_file(state_file, config, node.tcp_port(), node.http_port());
  }

  std::cout << "room_server_service_started tcp_port=" << node.tcp_port()
            << " http_port=" << node.http_port() << "\n";
  std::cout.flush();

  while (!g_stop_requested) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  node.stop();
  return EXIT_SUCCESS;
}
