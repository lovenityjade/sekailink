#pragma once

#include "nlohmann/json.hpp"
#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace sekailink_server {

struct LobbyRuntimeServiceConfig {
  std::uint16_t http_port = 19097;
  std::string listen_host = "127.0.0.1";
  std::filesystem::path sqlite_path;
  std::optional<std::string> auth_token;
  std::filesystem::path state_path;
};

LobbyRuntimeServiceConfig load_lobby_runtime_service_config(const std::filesystem::path& path);
nlohmann::json to_json(const LobbyRuntimeServiceConfig& config);

class LobbyRuntimeStore {
 public:
  struct LobbyPresenceRecord {
    std::string username;
    std::string joined_at;
    std::string updated_at;
  };

  struct LobbyRuntimeRecord {
    std::int64_t id = 0;
    std::string lobby_id;
    std::string name;
    std::string visibility = "private";
    std::string status = "open";
    std::optional<std::string> owner_username;
    std::optional<std::string> description;
    nlohmann::json metadata = nlohmann::json::object();
    std::string created_at;
    std::string updated_at;
    std::optional<std::string> closed_at;
    std::optional<std::string> last_activity_at;
    std::vector<LobbyPresenceRecord> presence;
  };

  explicit LobbyRuntimeStore(std::filesystem::path sqlite_path);
  ~LobbyRuntimeStore();

  LobbyRuntimeStore(const LobbyRuntimeStore&) = delete;
  LobbyRuntimeStore& operator=(const LobbyRuntimeStore&) = delete;

  LobbyRuntimeRecord open_lobby(
      const std::string& lobby_id,
      const std::string& name,
      const std::string& visibility,
      const std::optional<std::string>& owner_username,
      const std::optional<std::string>& description,
      const nlohmann::json& metadata);
  std::optional<LobbyRuntimeRecord> find_lobby(const std::string& lobby_id) const;
  std::optional<LobbyRuntimeRecord> update_lobby(
      const std::string& lobby_id,
      const std::optional<std::string>& name,
      const std::optional<std::string>& visibility,
      const std::optional<std::string>& owner_username,
      const std::optional<std::string>& description,
      const std::optional<nlohmann::json>& metadata,
      const std::optional<std::string>& status);
  std::vector<LobbyRuntimeRecord> list_open_lobbies(
      std::optional<std::size_t> limit = std::nullopt,
      std::size_t offset = 0,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& visibility = std::nullopt,
      const std::optional<std::string>& status = std::optional<std::string>("open")) const;
  std::optional<LobbyRuntimeRecord> join_lobby(const std::string& lobby_id, const std::string& username);
  std::optional<LobbyRuntimeRecord> leave_lobby(const std::string& lobby_id, const std::string& username);
  std::optional<LobbyRuntimeRecord> close_lobby(const std::string& lobby_id);

 private:
  void open();
  void close();
  void init_schema();
  void exec(const std::string& sql) const;
  LobbyRuntimeRecord load_lobby_from_stmt(sqlite3_stmt* stmt) const;
  void load_presence(LobbyRuntimeRecord& lobby) const;
  void update_activity(const std::string& lobby_id) const;

  std::filesystem::path sqlite_path_;
  sqlite3* db_ = nullptr;
};

class LobbyRuntimeService {
 public:
  explicit LobbyRuntimeService(LobbyRuntimeServiceConfig config);

  [[nodiscard]] nlohmann::json handle(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token,
      const std::optional<nlohmann::json>& body) const;

 private:
  [[nodiscard]] bool authorized(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_open_lobby(const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_edit_lobby(const std::string& lobby_id, const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_list_lobbies(
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& visibility = std::nullopt,
      const std::optional<std::string>& status = std::optional<std::string>("open"),
      std::optional<std::size_t> limit = std::nullopt,
      std::size_t offset = 0) const;
  [[nodiscard]] nlohmann::json handle_lobby_info(const std::string& lobby_id) const;
  [[nodiscard]] nlohmann::json handle_join(const std::string& lobby_id, const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_leave(const std::string& lobby_id, const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_close(const std::string& lobby_id) const;
  [[nodiscard]] static nlohmann::json lobby_to_json(const LobbyRuntimeStore::LobbyRuntimeRecord& lobby);
  void record_request() const;
  void record_error() const;
  void write_state_file() const;

  LobbyRuntimeServiceConfig config_;
  mutable LobbyRuntimeStore store_;
  mutable std::mutex state_mutex_;
  mutable std::uint64_t total_requests_ = 0;
  mutable std::uint64_t total_errors_ = 0;
};

class LobbyRuntimeHttpServer {
 public:
  explicit LobbyRuntimeHttpServer(LobbyRuntimeServiceConfig config);
  ~LobbyRuntimeHttpServer();

  LobbyRuntimeHttpServer(const LobbyRuntimeHttpServer&) = delete;
  LobbyRuntimeHttpServer& operator=(const LobbyRuntimeHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  LobbyRuntimeService service_;
  LobbyRuntimeServiceConfig config_;
};

}  // namespace sekailink_server
