#include "sekailink_server/room_seed_package_dispatch.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void print_usage(const char* argv0) {
  std::cerr
      << "usage: " << argv0
      << " --package-dir <dir> --room-id <id> [--auth-token <token>] [--host <host> --port <port>] [--print]\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::filesystem::path package_dir;
    std::string room_id;
    std::optional<std::string> auth_token;
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;
    bool print_only = false;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if ((arg == "--package-dir" || arg == "--package") && i + 1 < argc) {
        package_dir = argv[++i];
      } else if (arg == "--room-id" && i + 1 < argc) {
        room_id = argv[++i];
      } else if (arg == "--auth-token" && i + 1 < argc) {
        auth_token = argv[++i];
      } else if (arg == "--host" && i + 1 < argc) {
        host = argv[++i];
      } else if (arg == "--port" && i + 1 < argc) {
        port = static_cast<std::uint16_t>(std::stoul(argv[++i]));
      } else if (arg == "--print") {
        print_only = true;
      } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
      }
    }

    if (package_dir.empty() || room_id.empty()) {
      print_usage(argv[0]);
      return EXIT_FAILURE;
    }

    const sekailink_server::RoomSeedPackageDispatchRequest request{
        .package_dir = package_dir,
        .room_id = room_id,
        .auth_token = auth_token,
    };

    if (print_only || port == 0) {
      const auto envelope = sekailink_server::build_seed_package_dispatch_envelope(request);
      std::cout << envelope.dump(2) << "\n";
      return EXIT_SUCCESS;
    }

    std::cout << sekailink_server::dispatch_seed_package_to_room_tcp(request, host, port)
              << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_seed_package_dispatch failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
