#include "sekailink_server/room_projection_mysql.hpp"

#include "sekailink_server/room_projection_sql.hpp"

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#else
#error "mysql header not found"
#endif

#include <stdexcept>

namespace sekailink_server {

namespace {

std::string mysql_error_message(MYSQL* connection, const char* fallback) {
  if (connection == nullptr) {
    return fallback;
  }
  const auto* message = mysql_error(connection);
  if (message == nullptr || *message == '\0') {
    return fallback;
  }
  return message;
}

void bind_string(MYSQL_BIND& binding, unsigned long& length, my_bool& is_null, const std::string& value) {
  binding = {};
  binding.buffer_type = MYSQL_TYPE_STRING;
  binding.buffer = const_cast<char*>(value.data());
  binding.buffer_length = value.size();
  length = static_cast<unsigned long>(value.size());
  binding.length = &length;
  is_null = 0;
  binding.is_null = &is_null;
}

}  // namespace

RoomProjectionMysqlStore::RoomProjectionMysqlStore(MysqlConnectionConfig config)
    : config_(std::move(config)) {
  open();
  init_schema();
}

RoomProjectionMysqlStore::~RoomProjectionMysqlStore() {
  close();
}

void RoomProjectionMysqlStore::append_batch(const RoomProjectionBatch& batch) {
  exec("START TRANSACTION");
  try {
    insert_record("room_records", batch.room_record);
    for (const auto& record : batch.room_event_records) {
      insert_record("room_event_records", record);
    }
    for (const auto& record : batch.client_report_records) {
      insert_record("client_report_records", record);
    }
    exec("COMMIT");
  } catch (...) {
    try {
      exec("ROLLBACK");
    } catch (...) {
    }
    throw;
  }
}

std::vector<nlohmann::json> RoomProjectionMysqlStore::read_room_records() const {
  return read_records("room_records");
}

std::vector<nlohmann::json> RoomProjectionMysqlStore::read_room_event_records() const {
  return read_records("room_event_records");
}

std::vector<nlohmann::json> RoomProjectionMysqlStore::read_client_report_records() const {
  return read_records("client_report_records");
}

const MysqlConnectionConfig& RoomProjectionMysqlStore::config() const {
  return config_;
}

void RoomProjectionMysqlStore::open() {
  if (config_.database.empty()) {
    throw std::runtime_error("mysql_database_required");
  }
  connection_ = mysql_init(nullptr);
  if (connection_ == nullptr) {
    throw std::runtime_error("mysql_init_failed");
  }

  const auto* host = config_.host.empty() ? nullptr : config_.host.c_str();
  const auto* user = config_.user.empty() ? nullptr : config_.user.c_str();
  const auto* password = config_.password.empty() ? nullptr : config_.password.c_str();
  const auto* database = config_.database.c_str();
  const auto* socket = config_.unix_socket.empty() ? nullptr : config_.unix_socket.c_str();
  if (mysql_real_connect(connection_, host, user, password, database, config_.port, socket, 0) == nullptr) {
    const auto message = mysql_error_message(connection_, "mysql_connect_failed");
    close();
    throw std::runtime_error(message);
  }
}

void RoomProjectionMysqlStore::close() {
  if (connection_ != nullptr) {
    mysql_close(connection_);
    connection_ = nullptr;
  }
}

void RoomProjectionMysqlStore::init_schema() {
  const auto schema = projection_mysql_schema_sql();
  std::size_t offset = 0;
  while (offset < schema.size()) {
    const auto end = schema.find(';', offset);
    const auto statement = schema.substr(offset, end == std::string::npos ? std::string::npos : end - offset + 1);
    if (!statement.empty()) {
      exec(statement);
    }
    if (end == std::string::npos) {
      break;
    }
    offset = end + 1;
    while (offset < schema.size() && (schema[offset] == '\n' || schema[offset] == ' ' || schema[offset] == '\t')) {
      ++offset;
    }
  }
}

void RoomProjectionMysqlStore::exec(const std::string& sql) const {
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "mysql_query_failed"));
  }
}

void RoomProjectionMysqlStore::insert_record(const std::string& table, const nlohmann::json& record) {
  SqlStatement statement;
  if (table == "room_records") {
    statement = build_room_record_insert_sql(record);
  } else if (table == "room_event_records") {
    statement = build_room_event_record_insert_sql(record);
  } else if (table == "client_report_records") {
    statement = build_client_report_record_insert_sql(record);
  } else {
    throw std::runtime_error("mysql_unknown_projection_table");
  }

  MYSQL_STMT* stmt = mysql_stmt_init(connection_);
  if (stmt == nullptr) {
    throw std::runtime_error("mysql_stmt_init_failed");
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      mysql_stmt_close(stmt);
      stmt = nullptr;
    }
  };

  try {
    if (mysql_stmt_prepare(stmt, statement.sql.c_str(), statement.sql.size()) != 0) {
      throw std::runtime_error(mysql_stmt_error(stmt));
    }

    std::vector<MYSQL_BIND> binds(statement.bindings.size());
    std::vector<unsigned long> lengths(statement.bindings.size(), 0);
    std::vector<my_bool> nulls(statement.bindings.size(), 0);
    for (std::size_t i = 0; i < statement.bindings.size(); ++i) {
      bind_string(binds[i], lengths[i], nulls[i], statement.bindings[i]);
    }

    if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
      throw std::runtime_error(mysql_stmt_error(stmt));
    }
    if (mysql_stmt_execute(stmt) != 0) {
      throw std::runtime_error(mysql_stmt_error(stmt));
    }
    cleanup();
  } catch (...) {
    cleanup();
    throw;
  }
}

std::vector<nlohmann::json> RoomProjectionMysqlStore::read_records(const std::string& table) const {
  const auto sql = "SELECT record_json FROM " + table + " ORDER BY id ASC";
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "mysql_query_failed"));
  }

  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    if (mysql_field_count(connection_) == 0) {
      return {};
    }
    throw std::runtime_error(mysql_error_message(connection_, "mysql_store_result_failed"));
  }

  std::vector<nlohmann::json> rows;
  while (auto* row = mysql_fetch_row(result)) {
    const auto lengths = mysql_fetch_lengths(result);
    if (row[0] != nullptr) {
      rows.push_back(nlohmann::json::parse(std::string(row[0], lengths[0])));
    }
  }
  mysql_free_result(result);
  return rows;
}

}  // namespace sekailink_server
