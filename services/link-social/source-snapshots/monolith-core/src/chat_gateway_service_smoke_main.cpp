#include "sekailink_server/chat_gateway_service.hpp"

#include <cstdlib>
#include <iostream>

using namespace sekailink_server;

int main() {
  ChatGatewayServiceConfig config;
  config.auth_token = "test-token";
  ChatGatewayService service(config);

  const auto health = service.handle("GET", "/health", std::nullopt, std::nullopt);
  if (!health.value("ok", false)) {
    std::cerr << "health failed\n";
    return EXIT_FAILURE;
  }

  const auto denied = service.handle("GET", "/channels", std::nullopt, std::nullopt);
  if (denied.value("error", "") != "unauthorized") {
    std::cerr << "auth guard failed\n";
    return EXIT_FAILURE;
  }

  const auto channels = service.handle("GET", "/channels", std::string("test-token"), std::nullopt);
  if (!channels.value("ok", false) || !channels.contains("channels")) {
    std::cerr << "channels failed\n";
    return EXIT_FAILURE;
  }

  const auto invalid = service.handle(
      "POST",
      "/channels/../../messages",
      std::string("test-token"),
      nlohmann::json{{"author", "Tester"}, {"content", "hello"}});
  if (invalid.value("error", "") != "not_found") {
    std::cerr << "invalid path failed\n";
    return EXIT_FAILURE;
  }

  const auto empty_messages = service.handle("GET", "/channels/global%3Afr/messages", std::string("test-token"), std::nullopt);
  if (!empty_messages.value("ok", false) || !empty_messages.contains("messages")) {
    std::cerr << "message list failed\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
