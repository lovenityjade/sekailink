#include "sekailink_server/room_server_tcp.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  try {
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;
    std::string payload;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--host" && i + 1 < argc) {
        host = argv[++i];
      } else if (arg == "--port" && i + 1 < argc) {
        port = static_cast<std::uint16_t>(std::stoul(argv[++i]));
      } else if (arg == "--payload" && i + 1 < argc) {
        payload = argv[++i];
      }
    }

    if (port == 0 || payload.empty()) {
      std::cerr << "usage: sekailink_room_server_tcp_cli --port <port> --payload '<json>' [--host <host>]\n";
      return EXIT_FAILURE;
    }

    const auto request = nlohmann::json::parse(payload);
    const auto response = sekailink_server::tcp_send_json_line(host, port, request);
    std::cout << response << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_tcp_cli failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
