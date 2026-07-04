#pragma once

#include "nlohmann/json.hpp"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace sekailink_server {

struct ChatGatewayServiceConfig {
  std::uint16_t http_port = 19098;
  std::string listen_host = "127.0.0.1";
  std::string irc_host = "127.0.0.1";
  std::uint16_t irc_port = 16667;
  std::optional<std::string> auth_token;
};

ChatGatewayServiceConfig load_chat_gateway_service_config(const std::filesystem::path& path);
nlohmann::json to_json(const ChatGatewayServiceConfig& config);

class ChatGatewayService {
 public:
  explicit ChatGatewayService(ChatGatewayServiceConfig config);
  ~ChatGatewayService();

  [[nodiscard]] nlohmann::json handle(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token,
      const std::optional<nlohmann::json>& body) const;

 private:
  [[nodiscard]] bool authorized(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_channels() const;
  [[nodiscard]] nlohmann::json handle_list_messages(const std::string& channel_id) const;
  [[nodiscard]] nlohmann::json handle_list_presence(const std::string& channel_id) const;
  [[nodiscard]] nlohmann::json handle_touch_presence(const std::string& channel_id, const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_send_message(const std::string& channel_id, const nlohmann::json& body) const;
  [[nodiscard]] bool send_irc_message(const std::string& channel_id, const std::string& author, const std::string& content, std::string& error) const;
  [[nodiscard]] bool send_persistent_irc_message(const std::string& channel_id, const nlohmann::json& body, std::string& error) const;
  [[nodiscard]] bool ensure_irc_presence(const std::string& channel_id, const nlohmann::json& body, std::string& error) const;
  void apply_channel_modes(const std::string& channel_id) const;
  void remember_message(const std::string& channel_id, const std::string& author, const std::string& content) const;
  void stop_sessions() const;

  struct IrcSession;

  ChatGatewayServiceConfig config_;
  mutable std::mutex messages_mutex_;
  mutable std::uint64_t next_message_id_ = 1;
  mutable std::map<std::string, std::deque<nlohmann::json>> recent_messages_;
  mutable std::mutex sessions_mutex_;
  mutable std::unordered_map<std::string, std::shared_ptr<IrcSession>> sessions_;
};

class ChatGatewayHttpServer {
 public:
  explicit ChatGatewayHttpServer(ChatGatewayServiceConfig config);
  ~ChatGatewayHttpServer();

  ChatGatewayHttpServer(const ChatGatewayHttpServer&) = delete;
  ChatGatewayHttpServer& operator=(const ChatGatewayHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  ChatGatewayService service_;
  ChatGatewayServiceConfig config_;
};

}  // namespace sekailink_server
