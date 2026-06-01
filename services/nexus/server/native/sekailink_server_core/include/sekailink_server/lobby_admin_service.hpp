#pragma once

#include "nlohmann/json.hpp"
#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace sekailink_server {

struct LobbyAdminRequestContext {
  std::optional<std::string> user_agent;
  std::optional<std::string> client_name;
  std::optional<std::string> client_version;
  std::optional<std::string> device_id;
  std::optional<std::string> requested_locale;
};

struct LobbyAdminServiceConfig {
  struct RuntimeBridgeConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    std::uint16_t port = 19097;
    std::string admin_token;
  };

  std::uint16_t http_port = 19096;
  std::string listen_host = "127.0.0.1";
  std::filesystem::path sqlite_path;
  std::string admin_token;
  std::filesystem::path state_path;
  RuntimeBridgeConfig runtime_bridge;
};

LobbyAdminServiceConfig load_lobby_admin_service_config(const std::filesystem::path& path);
nlohmann::json to_json(const LobbyAdminServiceConfig& config);

class LobbyAdminStore {
 public:
  struct LobbyRecord {
    std::int64_t id = 0;
    std::string lobby_id;
    std::string name;
    std::string visibility = "private";
    std::string status = "open";
    std::optional<std::string> owner_username;
    std::optional<std::string> description;
    nlohmann::json rules = nlohmann::json::object();
    nlohmann::json metadata = nlohmann::json::object();
    std::string created_at;
    std::string updated_at;
    std::optional<std::string> closed_at;
  };

  struct AuditRecord {
    std::int64_t id = 0;
    std::string event_type;
    std::string lobby_id;
    std::string actor_type = "admin_token";
    nlohmann::json request_context = nlohmann::json::object();
    nlohmann::json payload = nlohmann::json::object();
    std::string created_at;
  };

  explicit LobbyAdminStore(std::filesystem::path sqlite_path);
  ~LobbyAdminStore();

  LobbyAdminStore(const LobbyAdminStore&) = delete;
  LobbyAdminStore& operator=(const LobbyAdminStore&) = delete;

  LobbyRecord create_lobby(
      const std::string& lobby_id,
      const std::string& name,
      const std::string& visibility,
      const std::optional<std::string>& owner_username,
      const std::optional<std::string>& description,
      const nlohmann::json& rules,
      const nlohmann::json& metadata);
  std::optional<LobbyRecord> find_lobby(const std::string& lobby_id) const;
  std::optional<LobbyRecord> update_lobby(
      const std::string& lobby_id,
      const std::optional<std::string>& name,
      const std::optional<std::string>& visibility,
      const std::optional<std::string>& owner_username,
      const std::optional<std::string>& description,
      const std::optional<nlohmann::json>& rules,
      const std::optional<nlohmann::json>& metadata,
      const std::optional<std::string>& status);
  std::optional<LobbyRecord> close_lobby(const std::string& lobby_id);
  std::vector<LobbyRecord> list_lobbies(
      int limit = 100,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& visibility = std::nullopt,
      const std::optional<std::string>& status = std::nullopt,
      int offset = 0) const;
  void record_audit(
      const std::string& event_type,
      const std::string& lobby_id,
      const nlohmann::json& request_context,
      const nlohmann::json& payload);
  std::vector<AuditRecord> read_audit(const std::string& lobby_id, int limit = 20) const;

 private:
  void open();
  void close();
  void init_schema();
  void exec(const std::string& sql) const;

  std::filesystem::path sqlite_path_;
  sqlite3* db_ = nullptr;
};

class LobbyAdminService {
 public:
  explicit LobbyAdminService(LobbyAdminServiceConfig config);

  [[nodiscard]] nlohmann::json handle(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token,
      const std::optional<nlohmann::json>& body,
      const LobbyAdminRequestContext& context) const;

 private:
  [[nodiscard]] bool authorized(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_add_lobby(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body,
      const LobbyAdminRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_list_lobbies(
      const std::optional<std::string>& bearer_token,
      const LobbyAdminRequestContext& context,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& visibility = std::nullopt,
      const std::optional<std::string>& status = std::nullopt,
      int limit = 100,
      int offset = 0) const;
  [[nodiscard]] nlohmann::json handle_lobby_info(
      const std::optional<std::string>& bearer_token,
      const std::string& lobby_id,
      const LobbyAdminRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_edit_lobby(
      const std::optional<std::string>& bearer_token,
      const std::string& lobby_id,
      const nlohmann::json& body,
      const LobbyAdminRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_close_lobby(
      const std::optional<std::string>& bearer_token,
      const std::string& lobby_id,
      const LobbyAdminRequestContext& context) const;
  void sync_runtime_open(const LobbyAdminStore::LobbyRecord& lobby) const;
  void sync_runtime_edit(const LobbyAdminStore::LobbyRecord& lobby) const;
  void sync_runtime_close(const std::string& lobby_id) const;
  [[nodiscard]] nlohmann::json fetch_runtime_info(const std::string& lobby_id) const;
  void record_request() const;
  void record_error() const;
  void write_state_file() const;
  [[nodiscard]] static nlohmann::json lobby_to_json(const LobbyAdminStore::LobbyRecord& lobby);

  LobbyAdminServiceConfig config_;
  mutable LobbyAdminStore store_;
  mutable std::mutex state_mutex_;
  mutable std::uint64_t total_requests_ = 0;
  mutable std::uint64_t total_errors_ = 0;
};

class LobbyAdminHttpServer {
 public:
  explicit LobbyAdminHttpServer(LobbyAdminServiceConfig config);
  ~LobbyAdminHttpServer();

  LobbyAdminHttpServer(const LobbyAdminHttpServer&) = delete;
  LobbyAdminHttpServer& operator=(const LobbyAdminHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  LobbyAdminService service_;
  LobbyAdminServiceConfig config_;
};

}  // namespace sekailink_server
