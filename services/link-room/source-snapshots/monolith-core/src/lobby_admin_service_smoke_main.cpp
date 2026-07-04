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
  if (sock < 0) throw std::runtime_error("lobby_smoke_socket_failed");
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("lobby_smoke_connect_failed");
  }
  std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  if (!bearer_token.empty()) request += "Authorization: Bearer " + bearer_token + "\r\n";
  request += "User-Agent: SekaiLinkAdminCLI/1.0\r\n";
  request += "X-SekaiLink-Client: admin-cli\r\n";
  request += "X-SekaiLink-Client-Version: 1.0.0\r\n";
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
  if (pos == std::string::npos) throw std::runtime_error("lobby_smoke_body_missing");
  return response.substr(pos + 4);
}
#endif

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "lobby_admin_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_lobby_admin_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path config_path = root / "lobby_admin.json";
    const fs::path sqlite_path = root / "lobbies.sqlite3";
    const fs::path state_path = root / "lobby_state.json";
    const fs::path runtime_config_path = root / "lobby_runtime.json";
    const fs::path runtime_sqlite_path = root / "lobby_runtime.sqlite3";
    const fs::path runtime_state_path = root / "lobby_runtime_state.json";
    {
      std::ofstream runtime_config_stream(runtime_config_path);
      runtime_config_stream << "{\n"
                            << "  \"http_port\": 19097,\n"
                            << "  \"listen_host\": \"127.0.0.1\",\n"
                            << "  \"sqlite_path\": \"" << runtime_sqlite_path.string() << "\",\n"
                            << "  \"auth_token\": \"runtime-bridge-secret\",\n"
                            << "  \"state_path\": \"" << runtime_state_path.string() << "\"\n"
                            << "}\n";
    }
    {
      std::ofstream config_stream(config_path);
      config_stream << "{\n"
                    << "  \"http_port\": 19096,\n"
                    << "  \"listen_host\": \"127.0.0.1\",\n"
                    << "  \"sqlite_path\": \"" << sqlite_path.string() << "\",\n"
                    << "  \"admin_token\": \"lobby-admin-secret\",\n"
                    << "  \"state_path\": \"" << state_path.string() << "\",\n"
                    << "  \"runtime_bridge\": {\n"
                    << "    \"enabled\": true,\n"
                    << "    \"host\": \"127.0.0.1\",\n"
                    << "    \"port\": 19097,\n"
                    << "    \"admin_token\": \"runtime-bridge-secret\"\n"
                    << "  }\n"
                    << "}\n";
    }

    const std::string binary = "/home/nobara-user/sekailink/build-server-core/sekailink_lobby_admin_service";
    const std::string runtime_binary = "/home/nobara-user/sekailink/build-server-core/sekailink_lobby_runtime_service";
    const pid_t runtime_pid = ::fork();
    if (runtime_pid < 0) throw std::runtime_error("lobby_runtime_smoke_fork_failed");
    if (runtime_pid == 0) {
      ::execl(runtime_binary.c_str(), runtime_binary.c_str(), "--config", runtime_config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    bool runtime_ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto health = http_request(19097, "GET", "/health", "", "");
        if (health.find("200 OK") != std::string::npos) {
          runtime_ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    require(runtime_ready, "lobby_runtime_smoke_not_ready");

    const pid_t pid = ::fork();
    if (pid < 0) throw std::runtime_error("lobby_smoke_fork_failed");
    if (pid == 0) {
      ::execl(binary.c_str(), binary.c_str(), "--config", config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    bool ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto health = http_request(19096, "GET", "/health", "", "");
        if (health.find("200 OK") != std::string::npos) {
          ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    require(ready, "lobby_smoke_not_ready");

    const auto add = http_request(
        19096,
        "POST",
        "/admin/lobbies",
        "lobby-admin-secret",
        R"({"lobby_id":"lobby-alpha","name":"Alpha Lobby","visibility":"public","owner_username":"jade","description":"Smoke lobby","rules":{"mode":"casual"},"metadata":{"game":"alttp"}})");
    require(add.find("200 OK") != std::string::npos, "lobby_add_status");
    const auto add_json = nlohmann::json::parse(extract_body(add));
    require(add_json.at("lobby").at("name").get<std::string>() == "Alpha Lobby", "lobby_add_name");

    const auto list = http_request(19096, "GET", "/admin/lobbies", "lobby-admin-secret", "");
    require(list.find("200 OK") != std::string::npos, "lobby_list_status");
    const auto list_json = nlohmann::json::parse(extract_body(list));
    require(!list_json.at("lobbies").empty(), "lobby_list_nonempty");
    const auto list_filtered = http_request(19096, "GET", "/admin/lobbies?limit=1&query=alpha", "lobby-admin-secret", "");
    require(list_filtered.find("200 OK") != std::string::npos, "lobby_list_filtered_status");
    const auto list_filtered_json = nlohmann::json::parse(extract_body(list_filtered));
    require(list_filtered_json.at("lobbies").size() == 1, "lobby_list_filtered_size");
    require(list_filtered_json.at("lobbies").at(0).at("lobby_id").get<std::string>() == "lobby-alpha", "lobby_list_filtered_id");

    const auto info = http_request(19096, "GET", "/admin/lobbies/lobby-alpha", "lobby-admin-secret", "");
    require(info.find("200 OK") != std::string::npos, "lobby_info_status");
    const auto info_json = nlohmann::json::parse(extract_body(info));
    require(info_json.contains("runtime"), "lobby_info_runtime_present");
    require(info_json.at("runtime").at("lobby").at("lobby_id").get<std::string>() == "lobby-alpha", "lobby_info_runtime_id");
    require(info_json.contains("audit_events"), "lobby_info_audit_present");
    require(!info_json.at("audit_events").empty(), "lobby_info_audit_nonempty");
    require(
        info_json.at("audit_events").back().at("request_context").at("client_name").get<std::string>() == "admin-cli",
        "lobby_info_audit_client_name");

    const auto edit = http_request(
        19096,
        "PATCH",
        "/admin/lobbies/lobby-alpha",
        "lobby-admin-secret",
        R"({"name":"Alpha Lobby Prime","visibility":"private","metadata":{"game":"alttp","tier":"beta"}})");
    require(edit.find("200 OK") != std::string::npos, "lobby_edit_status");
    const auto edit_json = nlohmann::json::parse(extract_body(edit));
    require(edit_json.at("lobby").at("visibility").get<std::string>() == "private", "lobby_edit_visibility");
    const auto runtime_info = http_request(19097, "GET", "/runtime/lobbies/lobby-alpha", "runtime-bridge-secret", "");
    require(runtime_info.find("200 OK") != std::string::npos, "lobby_runtime_info_status");
    const auto runtime_info_json = nlohmann::json::parse(extract_body(runtime_info));
    require(runtime_info_json.at("lobby").at("visibility").get<std::string>() == "private", "lobby_runtime_edit_visibility");
    require(runtime_info_json.at("lobby").at("name").get<std::string>() == "Alpha Lobby Prime", "lobby_runtime_edit_name");

    const auto close = http_request(19096, "POST", "/admin/lobbies/lobby-alpha/close", "lobby-admin-secret", "");
    require(close.find("200 OK") != std::string::npos, "lobby_close_status");
    const auto close_json = nlohmann::json::parse(extract_body(close));
    require(close_json.at("lobby").at("status").get<std::string>() == "closed", "lobby_close_state");

    const auto info_after_close = http_request(19096, "GET", "/admin/lobbies/lobby-alpha", "lobby-admin-secret", "");
    require(info_after_close.find("200 OK") != std::string::npos, "lobby_info_after_close_status");
    const auto info_after_close_json = nlohmann::json::parse(extract_body(info_after_close));
    require(info_after_close_json.at("audit_events").size() >= 4, "lobby_audit_event_count");

    const auto runtime_closed = http_request(19097, "GET", "/runtime/lobbies/lobby-alpha", "runtime-bridge-secret", "");
    const auto runtime_closed_json = nlohmann::json::parse(extract_body(runtime_closed));
    require(runtime_closed_json.at("lobby").at("status").get<std::string>() == "closed", "lobby_runtime_close_state");

    require(fs::exists(state_path), "lobby_state_file_missing");

    ::kill(pid, SIGTERM);
    ::kill(runtime_pid, SIGTERM);
    int status = 0;
    int runtime_status = 0;
    ::waitpid(pid, &status, 0);
    ::waitpid(runtime_pid, &runtime_status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "lobby_service_exit");
    require(WIFEXITED(runtime_status) && WEXITSTATUS(runtime_status) == 0, "lobby_runtime_service_exit");

    std::cout << "lobby_admin_service_ok=1\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "lobby_admin_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
