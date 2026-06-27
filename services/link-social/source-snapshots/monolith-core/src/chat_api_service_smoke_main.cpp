#include "sekailink_server/chat_api_service.hpp"

#include <cstdlib>
#include <iostream>

using namespace sekailink_server;

int main() {
  ChatApiServiceConfig config;
  config.chat_gateway_token = "test-token";
  ChatApiService service(config);

  const auto health = service.handle("GET", "/health", std::nullopt, std::nullopt, std::nullopt);
  if (health.status != 200) {
    std::cerr << "health failed\n";
    return EXIT_FAILURE;
  }
  const auto denied = service.handle("GET", "/channels", std::nullopt, std::nullopt, std::nullopt);
  if (denied.status != 401) {
    std::cerr << "auth guard failed\n";
    return EXIT_FAILURE;
  }
  const auto options = service.handle("OPTIONS", "/channels", std::nullopt, std::nullopt, std::nullopt);
  if (options.status != 204) {
    std::cerr << "options failed\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
