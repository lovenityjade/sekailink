#pragma once

#include "sekailink_server/room_projection_store.hpp"

#include "sekailink_server/room_projection.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

namespace sekailink_server {

class RoomProjectionSqliteStore : public RoomProjectionStore {
 public:
  explicit RoomProjectionSqliteStore(std::filesystem::path db_path);
  ~RoomProjectionSqliteStore();

  RoomProjectionSqliteStore(const RoomProjectionSqliteStore&) = delete;
  RoomProjectionSqliteStore& operator=(const RoomProjectionSqliteStore&) = delete;

  void append_batch(const RoomProjectionBatch& batch) override;

  [[nodiscard]] std::vector<nlohmann::json> read_room_records() const;
  [[nodiscard]] std::vector<nlohmann::json> read_room_event_records() const;
  [[nodiscard]] std::vector<nlohmann::json> read_client_report_records() const;
  [[nodiscard]] std::filesystem::path db_path() const;

 private:
  void open();
  void close();
  void init_schema();
  void exec(const std::string& sql) const;
  void insert_record(const std::string& table, const nlohmann::json& record);
  [[nodiscard]] std::vector<nlohmann::json> read_records(const std::string& table) const;

  std::filesystem::path db_path_;
  sqlite3* db_ = nullptr;
};

}  // namespace sekailink_server
