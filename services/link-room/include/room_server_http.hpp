#pragma once

#include "sekailink_server/room_server_protocol.hpp"
#include "sekailink_server/room_registry.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace sekailink_server {

struct HttpResponse {
  int status_code = 200;
  std::string content_type = "application/json";
  std::string body;
};

class RoomServerHttpService {
 public:
  explicit RoomServerHttpService(const RoomRegistry& registry, const RoomServerAuthPolicy* auth_policy = nullptr);

  [[nodiscard]] HttpResponse handle_request(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token = std::nullopt) const;
  [[nodiscard]] static std::string build_http_response(const HttpResponse& response);
  [[nodiscard]] const RoomRegistry& registry() const;

 private:
  [[nodiscard]] HttpResponse handle_get(const std::string& path, const std::optional<std::string>& bearer_token) const;

  const RoomRegistry& registry_;
  const RoomServerAuthPolicy* auth_policy_;
};

class RoomServerHttpTcpServer {
 public:
  RoomServerHttpTcpServer(
      const RoomRegistry& registry,
      std::uint16_t port,
      const RoomServerAuthPolicy* auth_policy = nullptr);
  ~RoomServerHttpTcpServer();

  RoomServerHttpTcpServer(const RoomServerHttpTcpServer&) = delete;
  RoomServerHttpTcpServer& operator=(const RoomServerHttpTcpServer&) = delete;

  void serve_one() const;
  void stop();
  [[nodiscard]] std::uint16_t port() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  RoomServerHttpService service_;
};

}  // namespace sekailink_server
