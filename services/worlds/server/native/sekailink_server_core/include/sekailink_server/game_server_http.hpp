#pragma once

#include "sekailink_server/game_server_protocol.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace sekailink_server {

struct GameHttpResponse {
  int status_code = 200;
  std::string content_type = "application/json";
  std::string body;
};

class GameServerHttpService {
 public:
  explicit GameServerHttpService(const GameSessionRegistry& registry,
                                 const GameServerAuthPolicy* auth_policy = nullptr);

  [[nodiscard]] GameHttpResponse handle_request(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token = std::nullopt) const;
  [[nodiscard]] static std::string build_http_response(const GameHttpResponse& response);

 private:
  [[nodiscard]] GameHttpResponse handle_get(const std::string& path,
                                            const std::optional<std::string>& bearer_token) const;

  const GameSessionRegistry& registry_;
  const GameServerAuthPolicy* auth_policy_;
};

class GameServerHttpTcpServer {
 public:
  GameServerHttpTcpServer(const GameSessionRegistry& registry,
                          std::uint16_t port,
                          const GameServerAuthPolicy* auth_policy = nullptr);
  ~GameServerHttpTcpServer();

  GameServerHttpTcpServer(const GameServerHttpTcpServer&) = delete;
  GameServerHttpTcpServer& operator=(const GameServerHttpTcpServer&) = delete;

  void serve_one() const;
  void stop();
  [[nodiscard]] std::uint16_t port() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  GameServerHttpService service_;
};

}  // namespace sekailink_server
