#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "sekailink_server/seed_config_api.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

#ifndef _WIN32
std::string http_request(
    std::uint16_t port,
    const std::string& method,
    const std::string& path,
    const std::string& bearer_token,
    const std::string& body) {
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("seed_config_http_smoke_socket_failed");
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("seed_config_http_smoke_connect_failed");
  }
  std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  if (!bearer_token.empty()) {
    request += "Authorization: Bearer " + bearer_token + "\r\n";
  }
  if (!body.empty()) {
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  } else {
    request += "Content-Length: 0\r\n";
  }
  request += "\r\n";
  request += body;
  ::send(sock, request.data(), request.size(), 0);
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
}

std::string extract_body(const std::string& response) {
  const auto pos = response.find("\r\n\r\n");
  if (pos == std::string::npos) {
    throw std::runtime_error("seed_config_http_smoke_body_missing");
  }
  return response.substr(pos + 4);
}
#endif

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "seed_config_api_http_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    sekailink_server::SeedConfigHttpServer server({
        .http_port = 0,
        .listen_host = "127.0.0.1",
        .admin_token = "admin-secret",
        .user_token = "user-secret",
    });
    require(server.start(), "server_start_failed");
    std::atomic<bool> running{true};
    std::thread loop([&]() {
      while (running.load()) {
        try {
          server.serve_one();
        } catch (...) {
        }
      }
    });

    const auto health = http_request(server.port(), "GET", "/seed-configs/health", "", "");
    require(health.find("200 OK") != std::string::npos, "health_status");
    const auto health_json = nlohmann::json::parse(extract_body(health));
    require(health_json.at("service").get<std::string>() == "sekailink_seed_config_api", "health_service");

    const std::string import_body = R"({
      "game_key":"alttp",
      "display_name":"A Link to the Past",
      "system_key":"snes",
      "linkedworld_id":"linkedworld.alttp.v1",
      "schema_version":"alttp-options-v1",
      "source_hash":"linkedworld-hash",
      "options":[
        {"option_key":"goal","yaml_key":"goal","label":"Goal","type":"enum","default_value":"ganon","choices":[
          {"choice_key":"ganon","yaml_value":"ganon","label":"Defeat Ganon"},
          {"choice_key":"triforce_hunt","yaml_value":"triforce_hunt","label":"Triforce Hunt"}
        ]},
        {"option_key":"triforce_pieces_required","yaml_key":"triforce_pieces_required","label":"Triforce pieces required","type":"integer","default_value":20}
      ]
    })";
    const auto import = http_request(server.port(), "POST", "/admin/seed-configs/games", "admin-secret", import_body);
    require(import.find("200 OK") != std::string::npos, "import_status");

    const auto save = http_request(
        server.port(),
        "POST",
        "/users/7/seed-configs",
        "user-secret",
        R"({"game_key":"alttp","name":"First Run","values":{"goal":"triforce_hunt"}})");
    require(save.find("200 OK") != std::string::npos, "save_status");
    const auto save_json = nlohmann::json::parse(extract_body(save));
    require(save_json.at("config").at("values").at("triforce_pieces_required").get<int>() == 20, "save_default");

    const auto manifest = http_request(server.port(), "GET", "/users/7/sklconf/manifest", "user-secret", "");
    require(manifest.find("200 OK") != std::string::npos, "manifest_status");
    const auto manifest_json = nlohmann::json::parse(extract_body(manifest));
    require(manifest_json.at("manifest").at("entries").size() == 2, "manifest_entries");

    running = false;
    server.stop();
    loop.join();
    std::cout << "seed_config_api_http_smoke_ok\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "seed_config_api_http_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
