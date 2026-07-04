#include "sekailink_server/room_server_tcp.hpp"

#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_len_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

namespace sekailink_server {

namespace {

constexpr int kClientReadTimeoutMs = 3000;

void close_socket_fd(int fd) {
  if (fd < 0) {
    return;
  }
#if defined(_WIN32)
  closesocket(fd);
#else
  close(fd);
#endif
}

void shutdown_socket_fd(int fd) {
  if (fd < 0) {
    return;
  }
#if defined(_WIN32)
  shutdown(fd, SD_BOTH);
#else
  shutdown(fd, SHUT_RDWR);
#endif
}

bool set_socket_recv_timeout(int fd, int timeout_ms) {
#if defined(_WIN32)
  const DWORD timeout = static_cast<DWORD>(timeout_ms);
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
#else
  timeval timeout{};
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

}  // namespace

RoomServerTcpService::RoomServerTcpService(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy)
    : registry_(registry), audit_store_(audit_store), projection_store_(projection_store), auth_policy_(auth_policy) {}

RoomServerTcpService::~RoomServerTcpService() {
  stop();
}

bool RoomServerTcpService::start(std::uint16_t port) {
  if (running_) {
    return false;
  }
#if defined(_WIN32)
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    return false;
  }
#endif
  listen_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) {
    return false;
  }

  int reuse = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    stop();
    return false;
  }
  if (::listen(listen_fd_, 8) != 0) {
    stop();
    return false;
  }

  socket_len_t len = sizeof(addr);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    stop();
    return false;
  }

  bound_port_ = ntohs(addr.sin_port);
  running_ = true;
  return true;
}

void RoomServerTcpService::stop() {
  running_ = false;
  shutdown_socket_fd(listen_fd_);
  close_socket_fd(listen_fd_);
  listen_fd_ = -1;
  bound_port_ = 0;
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
#if defined(_WIN32)
  WSACleanup();
#endif
}

bool RoomServerTcpService::serve_one() {
  if (!running_ || listen_fd_ < 0) {
    return false;
  }
  sockaddr_in client_addr{};
  socket_len_t client_len = sizeof(client_addr);
  const int client_fd = static_cast<int>(::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len));
  if (client_fd < 0) {
    return false;
  }

  set_socket_recv_timeout(client_fd, kClientReadTimeoutMs);

  std::string payload;
  char buffer[4096];
  while (true) {
    const auto read_count = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (read_count <= 0) {
      break;
    }
    payload.append(buffer, static_cast<std::size_t>(read_count));
    if (payload.find('\n') != std::string::npos) {
      break;
    }
  }

  const auto newline = payload.find('\n');
  if (newline == std::string::npos) {
    const auto response = nlohmann::json({{"ok", false}, {"error", "incomplete_request"}}).dump() + "\n";
    ::send(client_fd, response.data(), static_cast<int>(response.size()), 0);
    close_socket_fd(client_fd);
    return true;
  }
  payload.resize(newline);

  nlohmann::json response;
  try {
    response = handle_protocol_json(registry_, audit_store_, projection_store_, auth_policy_, nlohmann::json::parse(payload));
  } catch (const std::exception& e) {
    response = {{"ok", false}, {"error", e.what()}};
  }
  const auto serialized = response.dump() + "\n";
  ::send(client_fd, serialized.data(), static_cast<int>(serialized.size()), 0);
  close_socket_fd(client_fd);
  return true;
}

bool RoomServerTcpService::serve_forever() {
  if (!running_ || listen_fd_ < 0) {
    return false;
  }
  while (running_) {
    if (!serve_one()) {
      if (!running_) {
        break;
      }
    }
  }
  return true;
}

bool RoomServerTcpService::start_background(std::uint16_t port) {
  if (!start(port)) {
    return false;
  }
  server_thread_ = std::thread([this]() {
    serve_forever();
  });
  return true;
}

void RoomServerTcpService::join() {
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
}

std::uint16_t RoomServerTcpService::port() const {
  return bound_port_;
}

bool RoomServerTcpService::running() const {
  return running_;
}

std::string tcp_send_json_line(const std::string& host, std::uint16_t port, const nlohmann::json& payload) {
#if defined(_WIN32)
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    throw std::runtime_error("wsa_startup_failed");
  }
#endif
  const int fd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (fd < 0) {
    throw std::runtime_error("socket_create_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    close_socket_fd(fd);
    throw std::runtime_error("inet_pton_failed");
  }
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_socket_fd(fd);
    throw std::runtime_error("connect_failed");
  }

  const auto data = payload.dump() + "\n";
  ::send(fd, data.data(), static_cast<int>(data.size()), 0);

  std::string response;
  char buffer[4096];
  while (true) {
    const auto read_count = ::recv(fd, buffer, sizeof(buffer), 0);
    if (read_count <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(read_count));
    if (response.find('\n') != std::string::npos) {
      break;
    }
  }
  close_socket_fd(fd);
#if defined(_WIN32)
  WSACleanup();
#endif
  const auto newline = response.find('\n');
  if (newline != std::string::npos) {
    response.resize(newline);
  }
  return response;
}

}  // namespace sekailink_server
