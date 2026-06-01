#pragma once

#include "sekailink_server/room_projection_store.hpp"

#include "sekailink_server/room_projection.hpp"

#include <cstdint>
#include <string>
#include <vector>

typedef struct st_mysql MYSQL;

namespace sekailink_server {

struct MysqlConnectionConfig {
  std::string host = "127.0.0.1";
  std::string user;
  std::string password;
  std::string database;
  std::uint32_t port = 3306;
  std::string unix_socket;
};

class RoomProjectionMysqlStore : public RoomProjectionStore {
 public:
  explicit RoomProjectionMysqlStore(MysqlConnectionConfig config);
  ~RoomProjectionMysqlStore();

  RoomProjectionMysqlStore(const RoomProjectionMysqlStore&) = delete;
  RoomProjectionMysqlStore& operator=(const RoomProjectionMysqlStore&) = delete;

  void append_batch(const RoomProjectionBatch& batch) override;

  [[nodiscard]] std::vector<nlohmann::json> read_room_records() const;
  [[nodiscard]] std::vector<nlohmann::json> read_room_event_records() const;
  [[nodiscard]] std::vector<nlohmann::json> read_client_report_records() const;
  [[nodiscard]] const MysqlConnectionConfig& config() const;

 private:
  void open();
  void close();
  void init_schema();
  void exec(const std::string& sql) const;
  void insert_record(const std::string& table, const nlohmann::json& record);
  [[nodiscard]] std::vector<nlohmann::json> read_records(const std::string& table) const;

  MysqlConnectionConfig config_;
  MYSQL* connection_ = nullptr;
};

}  // namespace sekailink_server
