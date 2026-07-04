#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "nlohmann/json.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

#ifndef _WIN32
std::string http_request(
    std::uint16_t port,
    const std::string& method,
    const std::string& path,
    const std::string& bearer_token,
    const std::string& body) {
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) throw std::runtime_error("lobby_runtime_smoke_socket_failed");
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("lobby_runtime_smoke_connect_failed");
  }
  std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  if (!bearer_token.empty()) request += "Authorization: Bearer " + bearer_token + "\r\n";
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
    if (received <= 0) break;
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(sock);
  return response;
}

std::string extract_body(const std::string& response) {
  const auto pos = response.find("\r\n\r\n");
  if (pos == std::string::npos) throw std::runtime_error("lobby_runtime_smoke_body_missing");
  return response.substr(pos + 4);
}
#endif

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "lobby_runtime_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_lobby_runtime_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path config_path = root / "lobby_runtime.json";
    const fs::path sqlite_path = root / "lobby_runtime.sqlite3";
    const fs::path state_path = root / "lobby_runtime_state.json";
    {
      std::ofstream config_stream(config_path);
      config_stream << "{\n"
                    << "  \"http_port\": 19097,\n"
                    << "  \"listen_host\": \"127.0.0.1\",\n"
                    << "  \"sqlite_path\": \"" << sqlite_path.string() << "\",\n"
                    << "  \"auth_token\": \"lobby-runtime-secret\",\n"
                    << "  \"state_path\": \"" << state_path.string() << "\"\n"
                    << "}\n";
    }

    const std::string binary = "/home/nobara-user/sekailink/build-server-core/sekailink_lobby_runtime_service";
    const pid_t pid = ::fork();
    if (pid < 0) throw std::runtime_error("lobby_runtime_smoke_fork_failed");
    if (pid == 0) {
      ::execl(binary.c_str(), binary.c_str(), "--config", config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    bool ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto health = http_request(19097, "GET", "/health", "", "");
        if (health.find("200 OK") != std::string::npos) {
          ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    require(ready, "lobby_runtime_smoke_not_ready");

    const auto open = http_request(
        19097,
        "POST",
        "/admin/runtime/lobbies/open",
        "lobby-runtime-secret",
        R"({"lobby_id":"runtime-alpha","name":"Runtime Alpha","visibility":"public","owner_username":"jade","metadata":{"game":"alttp"}})");
    require(open.find("200 OK") != std::string::npos, "lobby_runtime_open_status");

    const auto join = http_request(
        19097,
        "POST",
        "/runtime/lobbies/runtime-alpha/presence/join",
        "lobby-runtime-secret",
        R"({"username":"jade"})");
    require(join.find("200 OK") != std::string::npos, "lobby_runtime_join_status");
    const auto join_json = nlohmann::json::parse(extract_body(join));
    require(join_json.at("lobby").at("presence_count").get<int>() == 1, "lobby_runtime_join_count");

    const auto list = http_request(19097, "GET", "/runtime/lobbies/open", "lobby-runtime-secret", "");
    require(list.find("200 OK") != std::string::npos, "lobby_runtime_list_status");
    const auto list_json = nlohmann::json::parse(extract_body(list));
    require(list_json.at("lobbies").size() == 1, "lobby_runtime_list_count");
    const auto list_filtered = http_request(19097, "GET", "/runtime/lobbies/open?limit=1&query=alpha", "lobby-runtime-secret", "");
    require(list_filtered.find("200 OK") != std::string::npos, "lobby_runtime_list_filtered_status");
    const auto list_filtered_json = nlohmann::json::parse(extract_body(list_filtered));
    require(list_filtered_json.at("lobbies").size() == 1, "lobby_runtime_list_filtered_count");
    require(list_filtered_json.at("lobbies").at(0).at("lobby_id").get<std::string>() == "runtime-alpha", "lobby_runtime_list_filtered_id");

    const auto leave = http_request(
        19097,
        "POST",
        "/runtime/lobbies/runtime-alpha/presence/leave",
        "lobby-runtime-secret",
        R"({"username":"jade"})");
    require(leave.find("200 OK") != std::string::npos, "lobby_runtime_leave_status");

    const auto close = http_request(
        19097,
        "POST",
        "/runtime/lobbies/runtime-alpha/close",
        "lobby-runtime-secret",
        "");
    require(close.find("200 OK") != std::string::npos, "lobby_runtime_close_status");
    const auto close_json = nlohmann::json::parse(extract_body(close));
    require(close_json.at("lobby").at("status").get<std::string>() == "closed", "lobby_runtime_close_state");

    require(fs::exists(state_path), "lobby_runtime_state_file_missing");

    ::kill(pid, SIGTERM);
    int status = 0;
    ::waitpid(pid, &status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "lobby_runtime_service_exit");

    std::cout << "lobby_runtime_service_ok=1\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "lobby_runtime_service_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
#endif
}
