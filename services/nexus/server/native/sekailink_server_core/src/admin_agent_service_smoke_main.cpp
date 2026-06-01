#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::string http_get(std::uint16_t port, const std::string& path, const std::string& bearer_token) {
#ifdef _WIN32
  throw std::runtime_error("admin_agent_service_smoke_not_supported_on_windows_yet");
#else
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("http_socket_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("http_connect_failed");
  }

  const std::string request =
      "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer " + bearer_token + "\r\nConnection: close\r\n\r\n";
  if (::send(sock, request.data(), request.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("http_send_failed");
  }

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
#endif
}

std::string http_post(std::uint16_t port, const std::string& path, const std::string& bearer_token) {
#ifdef _WIN32
  throw std::runtime_error("admin_agent_service_smoke_not_supported_on_windows_yet");
#else
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("http_socket_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("http_connect_failed");
  }

  const std::string request =
      "POST " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer " + bearer_token + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  if (::send(sock, request.data(), request.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("http_send_failed");
  }

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
#endif
}

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_admin_agent_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_admin_agent_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path config_path = root / "admin_agent.json";
    const fs::path state_path = root / "room_server_state.json";
    const fs::path log_path = root / "room_server.log";
    const fs::path systemctl_script = root / "systemctl.sh";
    const fs::path control_log = root / "control.log";

    {
      std::ofstream state_stream(state_path);
      state_stream << R"({"ok":true,"effective_tcp_port":43123,"effective_http_port":18081})";
    }
    {
      std::ofstream log_stream(log_path);
      log_stream << "alpha\n";
      log_stream << "beta\n";
    }
    {
      std::ofstream script_stream(systemctl_script);
      script_stream << "#!/usr/bin/env bash\n";
      script_stream << "printf '%s ' \"$@\" > \"" << control_log.string() << "\"\n";
      script_stream << "exit 0\n";
    }
    fs::permissions(
        systemctl_script,
        fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec,
        fs::perm_options::replace);
    {
      std::ofstream config_stream(config_path);
      config_stream << R"({
  "http_port": 19092,
  "admin_token": "agent-service-secret",
  "systemctl_path": ")" << systemctl_script.string() << R"(",
  "services": [
    {
      "name": "room-server",
      "state_file": ")" << state_path.string() << R"(",
      "log_file": ")" << log_path.string() << R"(",
      "systemd_unit": "sekailink-room-server.service"
    }
  ]
}
)";
    }

    const std::string binary = "/home/nobara-user/sekailink/build-server-core/sekailink_admin_agent_service";
    const pid_t pid = ::fork();
    if (pid < 0) {
      throw std::runtime_error("fork_failed");
    }
    if (pid == 0) {
      ::execl(binary.c_str(), binary.c_str(), "--config", config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }

    bool ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto health = http_get(19092, "/health", "ignored");
        if (health.find("200 OK") != std::string::npos) {
          ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!ready) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("admin_agent_not_ready");
    }

    const auto services = http_get(19092, "/services", "agent-service-secret");
    if (services.find("room-server") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("services_failed");
    }

    const auto system = http_get(19092, "/system", "agent-service-secret");
    if (system.find("hostname") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("system_failed");
    }

    const auto stop = http_post(19092, "/services/room-server/stop", "agent-service-secret");
    if (stop.find("200 OK") == std::string::npos || stop.find("\"stop\"") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("stop_failed");
    }
    {
      std::ifstream control_stream(control_log);
      std::string content;
      std::getline(control_stream, content);
      if (content.find("stop sekailink-room-server.service") == std::string::npos) {
        ::kill(pid, SIGTERM);
        ::waitpid(pid, nullptr, 0);
        throw std::runtime_error("stop_log_failed");
      }
    }

    const auto start = http_post(19092, "/services/room-server/start", "agent-service-secret");
    if (start.find("200 OK") == std::string::npos || start.find("\"start\"") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("start_failed");
    }
    {
      std::ifstream control_stream(control_log);
      std::string content;
      std::getline(control_stream, content);
      if (content.find("start sekailink-room-server.service") == std::string::npos) {
        ::kill(pid, SIGTERM);
        ::waitpid(pid, nullptr, 0);
        throw std::runtime_error("start_log_failed");
      }
    }

    const auto restart = http_post(19092, "/services/room-server/restart", "agent-service-secret");
    if (restart.find("200 OK") == std::string::npos || restart.find("\"restart\"") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("restart_failed");
    }
    {
      std::ifstream restart_stream(control_log);
      std::string content;
      std::getline(restart_stream, content);
      if (content.find("restart sekailink-room-server.service") == std::string::npos) {
        ::kill(pid, SIGTERM);
        ::waitpid(pid, nullptr, 0);
        throw std::runtime_error("restart_log_failed");
      }
    }

    const auto unauthorized = http_get(19092, "/services", "wrong-token");
    if (unauthorized.find("401 Unauthorized") == std::string::npos) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("unauthorized_failed");
    }

    ::kill(pid, SIGTERM);
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      throw std::runtime_error("service_exit_failed");
    }

    std::cout << "admin_agent_service_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_admin_agent_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
