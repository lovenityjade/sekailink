#include "sekailink_server/room_projection_sql.hpp"

#include <stdexcept>

namespace sekailink_server {

namespace {

std::string stringify_json(const nlohmann::json& value) {
  return value.dump();
}

SqlStatement build_insert_sql(const std::string& table, const nlohmann::json& record) {
  if (!record.contains("room_id")) {
    throw std::runtime_error("projection_record_missing_room_id");
  }
  return SqlStatement{
      .sql = "INSERT INTO " + table + " (room_id, record_json) VALUES (?, ?);",
      .bindings = {
          record.at("room_id").is_null() ? std::string{} : record.at("room_id").get<std::string>(),
          stringify_json(record),
      },
  };
}

}  // namespace

std::string projection_mysql_schema_sql() {
  return
      "CREATE TABLE IF NOT EXISTS room_records ("
      "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
      "room_id VARCHAR(191) NOT NULL,"
      "record_json LONGTEXT NOT NULL,"
      "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "INDEX idx_room_records_room_id (room_id)"
      ");"
      "\n"
      "CREATE TABLE IF NOT EXISTS room_event_records ("
      "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
      "room_id VARCHAR(191) NOT NULL,"
      "record_json LONGTEXT NOT NULL,"
      "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "INDEX idx_room_event_records_room_id (room_id)"
      ");"
      "\n"
      "CREATE TABLE IF NOT EXISTS client_report_records ("
      "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
      "room_id VARCHAR(191) NOT NULL,"
      "record_json LONGTEXT NOT NULL,"
      "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "INDEX idx_client_report_records_room_id (room_id)"
      ");";
}

SqlStatement build_room_record_insert_sql(const nlohmann::json& record) {
  return build_insert_sql("room_records", record);
}

SqlStatement build_room_event_record_insert_sql(const nlohmann::json& record) {
  return build_insert_sql("room_event_records", record);
}

SqlStatement build_client_report_record_insert_sql(const nlohmann::json& record) {
  return build_insert_sql("client_report_records", record);
}

}  // namespace sekailink_server
