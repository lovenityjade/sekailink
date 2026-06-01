#include "bridge_ipc.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>

namespace sekaiemu::spike {

namespace {

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

}  // namespace

BridgeIpc::~BridgeIpc() {
  Shutdown();
}

bool BridgeIpc::Initialize(const std::filesystem::path& bridge_root, std::string& error) {
  Shutdown();

  std::error_code ec;
  std::filesystem::create_directories(bridge_root, ec);
  if (ec) {
    error = "Failed to create bridge directory: " + ec.message();
    return false;
  }

  socket_path_ = bridge_root / "sekaiemu.sock";
  std::filesystem::remove(socket_path_, ec);

  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    error = "Failed to create Unix socket.";
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  const auto socket_string = socket_path_.string();
  if (socket_string.size() >= sizeof(addr.sun_path)) {
    error = "Bridge socket path is too long.";
    Shutdown();
    return false;
  }
  std::strncpy(addr.sun_path, socket_string.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    error = "Failed to bind Unix socket.";
    Shutdown();
    return false;
  }

  if (!SetNonBlocking(server_fd_)) {
    error = "Failed to mark bridge socket non-blocking.";
    Shutdown();
    return false;
  }

  if (listen(server_fd_, 4) != 0) {
    error = "Failed to listen on bridge socket.";
    Shutdown();
    return false;
  }

  return true;
}

void BridgeIpc::Shutdown() {
  for (int fd : client_fds_) {
    close(fd);
  }
  client_fds_.clear();
  pending_buffers_.clear();

  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }

  if (!socket_path_.empty()) {
    std::error_code ec;
    std::filesystem::remove(socket_path_, ec);
    socket_path_.clear();
  }
}

void BridgeIpc::CloseClient(int fd) {
  close(fd);
  pending_buffers_.erase(fd);
  client_fds_.erase(std::remove(client_fds_.begin(), client_fds_.end(), fd), client_fds_.end());
}

void BridgeIpc::AcceptClients() {
  if (server_fd_ < 0) {
    return;
  }

  while (true) {
    const int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      break;
    }
    if (!SetNonBlocking(client_fd)) {
      close(client_fd);
      continue;
    }
    client_fds_.push_back(client_fd);
    pending_buffers_[client_fd] = std::string();
  }
}

std::vector<std::string> BridgeIpc::DrainCommands() {
  AcceptClients();

  std::vector<std::string> commands;
  std::vector<int> to_close;

  std::array<char, 1024> buffer{};
  for (int fd : client_fds_) {
    while (true) {
      const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
      if (count == 0) {
        to_close.push_back(fd);
        break;
      }
      if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        to_close.push_back(fd);
        break;
      }

      auto& pending = pending_buffers_[fd];
      pending.append(buffer.data(), static_cast<std::size_t>(count));

      std::size_t newline = 0;
      while ((newline = pending.find('\n')) != std::string::npos) {
        std::string line = pending.substr(0, newline);
        pending.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        if (!line.empty()) {
          commands.push_back(std::move(line));
        }
      }
    }
  }

  for (int fd : to_close) {
    CloseClient(fd);
  }

  return commands;
}

void BridgeIpc::PublishEvent(std::string_view line) {
  AcceptClients();

  std::string payload(line);
  payload.push_back('\n');

  std::vector<int> to_close;
  for (int fd : client_fds_) {
    const ssize_t sent = send(fd, payload.data(), payload.size(), MSG_NOSIGNAL);
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      to_close.push_back(fd);
    }
  }

  for (int fd : to_close) {
    CloseClient(fd);
  }
}

const std::filesystem::path& BridgeIpc::SocketPath() const {
  return socket_path_;
}

}  // namespace sekaiemu::spike
