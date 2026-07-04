#pragma once

#include "nlohmann/json.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sekailink_server {

struct ChatApiServiceConfig {
  std::uint16_t http_port = 19099;
  std::string listen_host = "127.0.0.1";
  std::string identity_host = "149.202.61.90";
  std::uint16_t identity_port = 19095;
  std::string chat_gateway_host = "127.0.0.1";
  std::uint16_t chat_gateway_port = 19098;
  std::string chat_gateway_token;
  std::string lobby_runtime_host = "127.0.0.1";
  std::uint16_t lobby_runtime_port = 19097;
  std::string lobby_runtime_token;
  std::string seed_config_host = "127.0.0.1";
  std::uint16_t seed_config_port = 19106;
  std::string seed_config_user_token;
  std::string social_bots_host = "127.0.0.1";
  std::uint16_t social_bots_port = 8095;
  std::string social_bots_control_api_key;
  std::string room_admin_tool_token;
  std::filesystem::path generation_handoff_root;
  std::vector<std::string> generation_handoff_command;
  std::filesystem::path sqlite_path;
  std::filesystem::path client_release_manifest_path;
};

struct ChatApiHttpResponse {
  int status = 500;
  std::string body;
};

ChatApiServiceConfig load_chat_api_service_config(const std::filesystem::path& path);
nlohmann::json to_json(const ChatApiServiceConfig& config);

class ChatApiService {
 public:
  explicit ChatApiService(ChatApiServiceConfig config);

  [[nodiscard]] ChatApiHttpResponse handle(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token,
      const std::optional<std::string>& device_id,
      const std::optional<nlohmann::json>& body) const;

 private:
  struct SessionCacheEntry {
    nlohmann::json identity;
    std::chrono::steady_clock::time_point expires_at;
  };

  void initialize_store() const;
  [[nodiscard]] std::optional<nlohmann::json> validate_session(
      const std::string& session_token,
      const std::optional<std::string>& device_id) const;
  [[nodiscard]] ChatApiHttpResponse handle_list_messages(const std::string& channel_id) const;
  [[nodiscard]] ChatApiHttpResponse handle_list_presence(const std::string& channel_id) const;
  [[nodiscard]] ChatApiHttpResponse touch_presence(
      const std::string& channel_id,
      const nlohmann::json& identity,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse remember_message(
      const std::string& channel_id,
      const nlohmann::json& identity,
      const std::string& content) const;
  [[nodiscard]] ChatApiHttpResponse forward_to_gateway(
      const std::string& method,
      const std::string& path,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse forward_to_lobby_runtime(
      const std::string& method,
      const std::string& path,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse forward_to_seed_config(
      const std::string& method,
      const std::string& path,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse forward_to_social_bots(
      const std::string& method,
      const std::string& path,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse handle_legacy_me(const nlohmann::json& identity) const;
  [[nodiscard]] ChatApiHttpResponse handle_legacy_lobbies(
      const std::string& method,
      const std::vector<std::string>& parts,
      const nlohmann::json& identity,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse handle_social(
      const std::string& method,
      const std::string& raw_path,
      const std::vector<std::string>& parts,
      const nlohmann::json& identity,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse handle_world_config(
      const std::string& method,
      const std::string& raw_path,
      const std::vector<std::string>& parts,
      const nlohmann::json& identity,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse handle_client_compat(
      const std::string& method,
      const std::string& raw_path,
      const std::vector<std::string>& parts,
      const std::optional<nlohmann::json>& body) const;
  [[nodiscard]] ChatApiHttpResponse handle_misc_compat(
      const std::string& method,
      const std::string& raw_path,
      const std::vector<std::string>& parts,
      const nlohmann::json& identity,
      const std::optional<nlohmann::json>& body) const;
  void upsert_social_profile(const nlohmann::json& identity) const;

  ChatApiServiceConfig config_;
  mutable std::mutex store_mutex_;
  mutable std::mutex session_cache_mutex_;
  mutable std::unordered_map<std::string, SessionCacheEntry> session_cache_;
};

class ChatApiHttpServer {
 public:
  explicit ChatApiHttpServer(ChatApiServiceConfig config);
  ~ChatApiHttpServer();

  ChatApiHttpServer(const ChatApiHttpServer&) = delete;
  ChatApiHttpServer& operator=(const ChatApiHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  ChatApiService service_;
  ChatApiServiceConfig config_;
};

}  // namespace sekailink_server
