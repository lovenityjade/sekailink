#pragma once

#include "sekailink_server/room_projection_factory.hpp"
#include "sekailink_server/room_projection_mysql.hpp"
#include "sekailink_server/room_projection_query.hpp"
#include "sekailink_server/room_projection_sqlite.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>

namespace sekailink_server {

struct EvolutionRoomQueryConfig {
  std::uint16_t http_port = 18090;
  std::string listen_host = "127.0.0.1";
  std::optional<std::string> auth_token;
  ProjectionBackend projection_backend = ProjectionBackend::Sqlite;
  std::filesystem::path projection_target;
  MysqlConnectionConfig mysql_config;
  std::filesystem::path state_path;
};

EvolutionRoomQueryConfig load_evolution_room_query_config(const std::filesystem::path& path);

class EvolutionRoomQueryStore {
 public:
  explicit EvolutionRoomQueryStore(EvolutionRoomQueryConfig config);

  [[nodiscard]] std::vector<RoomStateSnapshot> list_latest_snapshots() const;
  [[nodiscard]] std::optional<RoomStateSnapshot> latest_snapshot(const std::string& room_id) const;
  [[nodiscard]] std::vector<RoomEvent> events(const std::string& room_id) const;
  [[nodiscard]] std::vector<ClientReport> reports(const std::string& room_id) const;

 private:
  EvolutionRoomQueryConfig config_;
  std::variant<std::monostate, RoomProjectionSqliteStore, RoomProjectionMysqlStore> store_;
};

class EvolutionRoomQueryHttpService {
 public:
  explicit EvolutionRoomQueryHttpService(EvolutionRoomQueryConfig config);

  [[nodiscard]] nlohmann::json handle_get(
      const std::string& path,
      const std::optional<std::string>& bearer_token) const;

 private:
  [[nodiscard]] bool authorized(const std::optional<std::string>& bearer_token) const;
  void record_request() const;
  void record_error() const;
  void write_state_file() const;

  EvolutionRoomQueryConfig config_;
  EvolutionRoomQueryStore store_;
  mutable std::mutex state_mutex_;
  mutable std::uint64_t total_requests_ = 0;
  mutable std::uint64_t total_errors_ = 0;
};

class EvolutionRoomQueryHttpServer {
 public:
  explicit EvolutionRoomQueryHttpServer(EvolutionRoomQueryConfig config);
  ~EvolutionRoomQueryHttpServer();

  EvolutionRoomQueryHttpServer(const EvolutionRoomQueryHttpServer&) = delete;
  EvolutionRoomQueryHttpServer& operator=(const EvolutionRoomQueryHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  EvolutionRoomQueryHttpService service_;
  EvolutionRoomQueryConfig config_;
};

}  // namespace sekailink_server
