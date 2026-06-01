#pragma once

#include "sekailink_server/room_projection_factory.hpp"
#include "sekailink_server/room_server_http.hpp"
#include "sekailink_server/room_server_tcp.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>

namespace sekailink_server {

struct RoomServerNodeConfig {
  std::uint16_t tcp_port = 0;
  std::uint16_t http_port = 0;
  std::filesystem::path audit_root;
  std::filesystem::path projection_root;
  ProjectionBackend projection_backend = ProjectionBackend::Jsonl;
  bool restore_from_audit = false;
  bool restore_from_projection = false;
  bool purge_expired_periodically = false;
  std::uint32_t purge_interval_ms = 1000;
  RoomServerAuthPolicy auth_policy;
};

class RoomServerNode {
 public:
  explicit RoomServerNode(RoomServerNodeConfig config);
  ~RoomServerNode();

  RoomServerNode(const RoomServerNode&) = delete;
  RoomServerNode& operator=(const RoomServerNode&) = delete;

  bool start();
  void stop();
  [[nodiscard]] bool running() const;
  [[nodiscard]] std::uint16_t tcp_port() const;
  [[nodiscard]] std::uint16_t http_port() const;
  [[nodiscard]] const RoomRegistry& registry() const;

 private:
  void bootstrap_restore();
  void purge_loop();

  RoomServerNodeConfig config_;
  RoomRegistry registry_;
  std::optional<RoomAuditStore> audit_store_;
  std::unique_ptr<RoomProjectionStore> projection_store_;
  std::unique_ptr<RoomServerTcpService> tcp_service_;
  std::unique_ptr<RoomServerHttpTcpServer> http_server_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread http_thread_;
  std::thread purge_thread_;
};

}  // namespace sekailink_server
