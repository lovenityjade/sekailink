#include "sekailink_server/room_projection_sqlite.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace sekailink_server {

namespace {

void bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
  if (sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error("sqlite_bind_text_failed");
  }
}

}  // namespace

RoomProjectionSqliteStore::RoomProjectionSqliteStore(std::filesystem::path db_path)
    : db_path_(std::move(db_path)) {
  open();
  init_schema();
}

RoomProjectionSqliteStore::~RoomProjectionSqliteStore() {
  close();
}

void RoomProjectionSqliteStore::append_batch(const RoomProjectionBatch& batch) {
  exec("BEGIN IMMEDIATE TRANSACTION;");
  try {
    insert_record("room_records", batch.room_record);
    for (const auto& record : batch.room_event_records) {
      insert_record("room_event_records", record);
    }
    for (const auto& record : batch.client_report_records) {
      insert_record("client_report_records", record);
    }
    exec("COMMIT;");
  } catch (...) {
    exec("ROLLBACK;");
    throw;
  }
}

std::vector<nlohmann::json> RoomProjectionSqliteStore::read_room_records() const {
  return read_records("room_records");
}

std::vector<nlohmann::json> RoomProjectionSqliteStore::read_room_event_records() const {
  return read_records("room_event_records");
}

std::vector<nlohmann::json> RoomProjectionSqliteStore::read_client_report_records() const {
  return read_records("client_report_records");
}

std::filesystem::path RoomProjectionSqliteStore::db_path() const {
  return db_path_;
}

void RoomProjectionSqliteStore::open() {
  std::filesystem::create_directories(db_path_.parent_path());
  if (sqlite3_open(db_path_.string().c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("sqlite_open_failed");
  }
}

void RoomProjectionSqliteStore::close() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void RoomProjectionSqliteStore::init_schema() {
  exec(
      "CREATE TABLE IF NOT EXISTS room_records ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "room_id TEXT NOT NULL,"
      "record_json TEXT NOT NULL"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS room_event_records ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "room_id TEXT NOT NULL,"
      "record_json TEXT NOT NULL"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS client_report_records ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "room_id TEXT NOT NULL,"
      "record_json TEXT NOT NULL"
      ");");
}

void RoomProjectionSqliteStore::exec(const std::string& sql) const {
  char* error = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error != nullptr ? error : "sqlite_exec_failed";
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void RoomProjectionSqliteStore::insert_record(const std::string& table, const nlohmann::json& record) {
  const auto sql = "INSERT INTO " + table + " (room_id, record_json) VALUES (?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("sqlite_prepare_failed");
  }
  const auto finalize = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };

  try {
    bind_text(stmt, 1, record.value("room_id", ""));
    bind_text(stmt, 2, record.dump());
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      throw std::runtime_error("sqlite_step_failed");
    }
    finalize();
  } catch (...) {
    finalize();
    throw;
  }
}

std::vector<nlohmann::json> RoomProjectionSqliteStore::read_records(const std::string& table) const {
  const auto sql = "SELECT record_json FROM " + table + " ORDER BY id ASC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("sqlite_prepare_failed");
  }
  std::vector<nlohmann::json> rows;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (text != nullptr) {
      rows.push_back(nlohmann::json::parse(text));
    }
  }
  sqlite3_finalize(stmt);
  return rows;
}

}  // namespace sekailink_server
