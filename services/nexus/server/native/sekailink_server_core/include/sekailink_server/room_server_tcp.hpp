#pragma once

#include "sekailink_server/room_server_protocol.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace sekailink_server {

class RoomServerTcpService {
 public:
  RoomServerTcpService(
      RoomRegistry& registry,
      RoomAuditStore* audit_store = nullptr,
      RoomProjectionStore* projection_store = nullptr,
      const RoomServerAuthPolicy* auth_policy = nullptr);
  ~RoomServerTcpService();

  bool start(std::uint16_t port = 0);
  void stop();
  bool serve_one();
  bool serve_forever();
  bool start_background(std::uint16_t port = 0);
  void join();
  [[nodiscard]] std::uint16_t port() const;
  [[nodiscard]] bool running() const;

 private:
  RoomRegistry& registry_;
  RoomAuditStore* audit_store_;
  RoomProjectionStore* projection_store_;
  const RoomServerAuthPolicy* auth_policy_;
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  std::atomic<bool> running_{false};
  std::thread server_thread_;
};

std::string tcp_send_json_line(const std::string& host, std::uint16_t port, const nlohmann::json& payload);

}  // namespace sekailink_server
