#include "sekailink_server/game_server_http.hpp"
#include "sekailink_server/game_server_tcp.hpp"

#ifndef _WIN32
#include <csignal>
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace sekailink_server;

namespace {

std::atomic<bool> g_stop_requested{false};

#ifndef _WIN32
void handle_signal(int) {
  g_stop_requested = true;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  std::uint16_t tcp_port = 19091;
  std::uint16_t http_port = 19092;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--tcp-port" && i + 1 < argc) {
      tcp_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "--http-port" && i + 1 < argc) {
      http_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
    }
  }

  GameSessionRegistry registry;
  GameServerAuthPolicy auth_policy;
  if (const auto* token = std::getenv("SEKAILINK_GAME_SERVER_ADMIN_TOKEN"); token != nullptr && *token != '\0') {
    auth_policy.admin_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_GAME_SERVER_CORE_TOKEN"); token != nullptr && *token != '\0') {
    auth_policy.core_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_GAME_SERVER_RUNTIME_TOKEN"); token != nullptr && *token != '\0') {
    auth_policy.runtime_token = token;
  }

#ifndef _WIN32
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
#endif

  GameServerTcpService tcp_service(registry, &auth_policy);
  if (!tcp_service.start_background(tcp_port)) {
    std::cerr << "game_server_tcp_start_failed\n";
    return EXIT_FAILURE;
  }

  GameServerHttpTcpServer http_server(registry, http_port, &auth_policy);
  std::cout << "game_server_service_started tcp_port=" << tcp_service.port()
            << " http_port=" << http_server.port() << "\n";
  std::cout.flush();

  while (!g_stop_requested) {
    try {
      http_server.serve_one();
    } catch (...) {
      if (g_stop_requested) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
  }

  http_server.stop();
  tcp_service.stop();
  return EXIT_SUCCESS;
}
