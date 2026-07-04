#include "sekailink_server/game_server_protocol.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace sekailink_server;

int main() {
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

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      const auto envelope = nlohmann::json::parse(line);
      const auto response = handle_game_protocol_json(registry, &auth_policy, envelope);
      std::cout << response.dump() << std::endl;
    } catch (const std::exception& exception) {
      std::cout << nlohmann::json{{"ok", false}, {"error", exception.what()}}.dump() << std::endl;
    }
  }

  return EXIT_SUCCESS;
}
