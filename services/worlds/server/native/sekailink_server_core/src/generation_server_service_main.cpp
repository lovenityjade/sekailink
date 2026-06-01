#include "sekailink_server/generation_server.hpp"

#ifndef _WIN32
#include <csignal>
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
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

}  // namespace

int main(int argc, char** argv) {
  std::string config_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    }
  }

  if (config_path.empty()) {
    std::cerr << "missing --config\n";
    return EXIT_FAILURE;
  }

  auto config = load_generation_server_config(config_path);

#ifndef _WIN32
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
#endif

  GenerationTcpServer server(std::move(config));
  if (!server.start()) {
    std::cerr << "generation_server_service_start_failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "generation_server_service_started tcp_port=" << server.port() << "\n";
  std::cout.flush();

  while (!g_stop_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  server.stop();
  return EXIT_SUCCESS;
}
