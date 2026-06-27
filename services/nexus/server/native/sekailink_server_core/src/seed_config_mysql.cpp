#include "sekailink_server/seed_config_mysql.hpp"

#include "sekailink_server/seed_config_sql.hpp"

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#else
#error "mysql header not found"
#endif

#include <algorithm>
#include <array>
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

std::string mysql_stmt_error_message(MYSQL_STMT* stmt, const char* fallback) {
  if (stmt == nullptr) {
    return fallback;
  }
  const auto* message = mysql_stmt_error(stmt);
  if (message == nullptr || *message == '\0') {
    return fallback;
  }
  return message;
}

void bind_text(MYSQL_BIND& binding, unsigned long& length, my_bool& is_null, const SeedConfigMysqlStore::QueryValue& value) {
  binding = {};
  binding.buffer_type = MYSQL_TYPE_STRING;
  if (value.is_null) {
    is_null = 1;
    binding.is_null = &is_null;
    return;
  }
  binding.buffer = const_cast<char*>(value.value.data());
  binding.buffer_length = value.value.size();
  length = static_cast<unsigned long>(value.value.size());
  binding.length = &length;
  is_null = 0;
  binding.is_null = &is_null;
}

}  // namespace

SeedConfigMysqlStore::SeedConfigMysqlStore(MysqlConnectionConfig config)
    : config_(std::move(config)) {
  open();
  init_schema();
}

SeedConfigMysqlStore::~SeedConfigMysqlStore() {
  close();
}

const MysqlConnectionConfig& SeedConfigMysqlStore::config() const {
  return config_;
}

std::int64_t SeedConfigMysqlStore::upsert_game(
    const std::string& game_key,
    const std::string& display_name,
    const std::string& system_key,
    const std::string& active_linkedworld_id,
    const std::string& status) {
  execute_prepared(
      "INSERT INTO games (game_key, display_name, system_key, active_linkedworld_id, status) "
      "VALUES (?, ?, ?, ?, ?) "
      "ON DUPLICATE KEY UPDATE display_name = VALUES(display_name), system_key = VALUES(system_key), "
      "active_linkedworld_id = VALUES(active_linkedworld_id), status = VALUES(status)",
      {
          string_value(game_key),
          string_value(display_name),
          string_value(system_key),
          string_value(active_linkedworld_id),
          string_value(status),
      });
  const auto id = query_single_id("SELECT id FROM games WHERE game_key = ?", {string_value(game_key)});
  if (!id.has_value()) {
    throw std::runtime_error("seed_config_game_upsert_missing_id");
  }
  return *id;
}

std::optional<SeedConfigGameRecord> SeedConfigMysqlStore::find_game_by_key(const std::string& game_key) const {
  ensure_connected();
  MYSQL_STMT* stmt = mysql_stmt_init(connection_);
  if (stmt == nullptr) {
    throw std::runtime_error("seed_config_mysql_stmt_init_failed");
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      mysql_stmt_close(stmt);
      stmt = nullptr;
    }
  };

  try {
    const std::string sql = "SELECT id, game_key, display_name, system_key, COALESCE(active_linkedworld_id, ''), status FROM games WHERE game_key = ?";
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_find_game_prepare_failed"));
    }
    std::vector<MYSQL_BIND> params(1);
    std::vector<unsigned long> lengths(1, 0);
    std::vector<my_bool> nulls(1, 0);
    const auto key_value = string_value(game_key);
    bind_text(params[0], lengths[0], nulls[0], key_value);
    if (mysql_stmt_bind_param(stmt, params.data()) != 0 || mysql_stmt_execute(stmt) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_find_game_execute_failed"));
    }

    long long id = 0;
    std::array<char, 256> key{};
    std::array<char, 512> display{};
    std::array<char, 128> system{};
    std::array<char, 256> linkedworld{};
    std::array<char, 64> status{};
    std::array<unsigned long, 6> out_lengths{};
    std::array<my_bool, 6> out_nulls{};
    std::array<MYSQL_BIND, 6> results{};
    results[0].buffer_type = MYSQL_TYPE_LONGLONG;
    results[0].buffer = &id;
    results[0].is_null = &out_nulls[0];
    const auto bind_string_result = [&](int index, auto& buffer) {
      results[index].buffer_type = MYSQL_TYPE_STRING;
      results[index].buffer = buffer.data();
      results[index].buffer_length = buffer.size();
      results[index].length = &out_lengths[index];
      results[index].is_null = &out_nulls[index];
    };
    bind_string_result(1, key);
    bind_string_result(2, display);
    bind_string_result(3, system);
    bind_string_result(4, linkedworld);
    bind_string_result(5, status);
    if (mysql_stmt_bind_result(stmt, results.data()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_find_game_bind_result_failed"));
    }
    const auto fetch = mysql_stmt_fetch(stmt);
    if (fetch == MYSQL_NO_DATA) {
      cleanup();
      return std::nullopt;
    }
    if (fetch != 0 && fetch != MYSQL_DATA_TRUNCATED) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_find_game_fetch_failed"));
    }
    SeedConfigGameRecord record{
        .id = static_cast<std::int64_t>(id),
        .game_key = std::string(key.data(), out_lengths[1]),
        .display_name = std::string(display.data(), out_lengths[2]),
        .system_key = std::string(system.data(), out_lengths[3]),
        .active_linkedworld_id = std::string(linkedworld.data(), out_lengths[4]),
        .status = std::string(status.data(), out_lengths[5]),
    };
    cleanup();
    return record;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::vector<SeedConfigGameRecord> SeedConfigMysqlStore::list_games() const {
  ensure_connected();
  const std::string sql =
      "SELECT id, game_key, display_name, system_key, COALESCE(active_linkedworld_id, ''), status "
      "FROM games ORDER BY display_name ASC";
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_list_games_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    if (mysql_field_count(connection_) == 0) {
      return {};
    }
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_list_games_store_failed"));
  }
  std::vector<SeedConfigGameRecord> records;
  while (auto* row = mysql_fetch_row(result)) {
    const auto lengths = mysql_fetch_lengths(result);
    records.push_back(SeedConfigGameRecord{
        .id = row[0] == nullptr ? 0 : std::stoll(row[0]),
        .game_key = row[1] == nullptr ? "" : std::string(row[1], lengths[1]),
        .display_name = row[2] == nullptr ? "" : std::string(row[2], lengths[2]),
        .system_key = row[3] == nullptr ? "" : std::string(row[3], lengths[3]),
        .active_linkedworld_id = row[4] == nullptr ? "" : std::string(row[4], lengths[4]),
        .status = row[5] == nullptr ? "" : std::string(row[5], lengths[5]),
    });
  }
  mysql_free_result(result);
  return records;
}

std::int64_t SeedConfigMysqlStore::create_option_schema_version(
    std::int64_t game_id,
    const std::string& schema_version,
    const std::string& source_kind,
    const std::string& source_hash,
    const std::string& source_ref) {
  return execute_insert(
      "INSERT INTO game_option_schema_versions (game_id, schema_version, source_kind, source_hash, source_ref) VALUES (?, ?, ?, ?, ?)",
      {
          int64_value(game_id),
          string_value(schema_version),
          string_value(source_kind),
          string_value(source_hash),
          string_value(source_ref),
      });
}

std::int64_t SeedConfigMysqlStore::create_option_group(
    std::int64_t schema_version_id,
    const std::string& group_key,
    const std::string& label,
    const std::string& description,
    int sort_order) {
  return execute_insert(
      "INSERT INTO game_option_groups (schema_version_id, group_key, label, description, sort_order) VALUES (?, ?, ?, ?, ?)",
      {
          int64_value(schema_version_id),
          string_value(group_key),
          string_value(label),
          string_value(description),
          int64_value(sort_order),
      });
}

std::int64_t SeedConfigMysqlStore::create_option_definition(
    std::int64_t schema_version_id,
    const std::optional<std::int64_t>& group_id,
    const SeedConfigOptionDefinition& definition,
    int sort_order,
    const nlohmann::json& visibility_rules,
    const nlohmann::json& validation_rules) {
  return execute_insert(
      "INSERT INTO game_option_definitions "
      "(schema_version_id, group_id, option_key, yaml_key, label, description, option_type, default_value_json, required, sort_order, visibility_rules_json, validation_rules_json) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
      {
          int64_value(schema_version_id),
          nullable_int64_value(group_id),
          string_value(definition.option_key),
          string_value(definition.yaml_key),
          string_value(definition.label),
          string_value(definition.description),
          string_value(definition.type),
          json_value(definition.default_value),
          bool_value(definition.required),
          int64_value(sort_order),
          visibility_rules.is_null() ? QueryValue{.is_null = true} : json_value(visibility_rules),
          validation_rules.is_null() ? QueryValue{.is_null = true} : json_value(validation_rules),
      });
}

void SeedConfigMysqlStore::create_option_choice(std::int64_t option_id, const SeedConfigOptionChoice& choice, int sort_order) {
  execute_prepared(
      "INSERT INTO game_option_choices (option_id, choice_key, yaml_value, label, description, sort_order) VALUES (?, ?, ?, ?, ?, ?)",
      {
          int64_value(option_id),
          string_value(choice.choice_key),
          string_value(choice.yaml_value),
          string_value(choice.label),
          string_value(choice.description),
          int64_value(sort_order),
      });
}

void SeedConfigMysqlStore::set_active_option_schema(std::int64_t game_id, std::int64_t schema_version_id) {
  execute_prepared(
      "UPDATE games SET active_option_schema_id = ? WHERE id = ?",
      {
          int64_value(schema_version_id),
          int64_value(game_id),
      });
}

std::optional<SeedConfigOptionSchemaRecord> SeedConfigMysqlStore::find_active_option_schema(const std::string& game_key) const {
  const auto sql =
      "SELECT s.id, s.game_id, g.game_key, s.schema_version "
      "FROM games g JOIN game_option_schema_versions s ON s.id = g.active_option_schema_id "
      "WHERE g.game_key = " +
      quote(game_key) + " LIMIT 1";
  ensure_connected();
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_active_schema_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_active_schema_store_failed"));
  }
  auto* row = mysql_fetch_row(result);
  if (row == nullptr) {
    mysql_free_result(result);
    return std::nullopt;
  }
  const auto lengths = mysql_fetch_lengths(result);
  SeedConfigOptionSchemaRecord record{
      .id = row[0] == nullptr ? 0 : std::stoll(row[0]),
      .game_id = row[1] == nullptr ? 0 : std::stoll(row[1]),
      .game_key = row[2] == nullptr ? "" : std::string(row[2], lengths[2]),
      .schema_version = row[3] == nullptr ? "" : std::string(row[3], lengths[3]),
  };
  mysql_free_result(result);
  return record;
}

std::vector<SeedConfigOptionDefinition> SeedConfigMysqlStore::load_option_definitions(std::int64_t schema_version_id) const {
  ensure_connected();
  const auto sql =
      "SELECT id, option_key, yaml_key, label, description, option_type, default_value_json, required, "
      "visibility_rules_json, validation_rules_json "
      "FROM game_option_definitions WHERE schema_version_id = " +
      std::to_string(schema_version_id) + " ORDER BY sort_order ASC, id ASC";
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_load_definitions_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_load_definitions_store_failed"));
  }
  struct DefinitionRow {
    std::int64_t id = 0;
    SeedConfigOptionDefinition definition;
  };
  std::vector<DefinitionRow> rows;
  while (auto* row = mysql_fetch_row(result)) {
    const auto lengths = mysql_fetch_lengths(result);
    SeedConfigOptionDefinition definition;
    definition.option_key = row[1] == nullptr ? "" : std::string(row[1], lengths[1]);
    definition.yaml_key = row[2] == nullptr ? definition.option_key : std::string(row[2], lengths[2]);
    definition.label = row[3] == nullptr ? definition.option_key : std::string(row[3], lengths[3]);
    definition.description = row[4] == nullptr ? "" : std::string(row[4], lengths[4]);
    definition.type = row[5] == nullptr ? "" : std::string(row[5], lengths[5]);
    definition.default_value = row[6] == nullptr ? nlohmann::json(nullptr) : nlohmann::json::parse(std::string(row[6], lengths[6]));
    definition.required = row[7] != nullptr && std::string(row[7], lengths[7]) == "1";
    definition.visibility_rules = row[8] == nullptr ? nlohmann::json(nullptr) : nlohmann::json::parse(std::string(row[8], lengths[8]));
    definition.validation_rules = row[9] == nullptr ? nlohmann::json(nullptr) : nlohmann::json::parse(std::string(row[9], lengths[9]));
    rows.push_back(DefinitionRow{.id = row[0] == nullptr ? 0 : std::stoll(row[0]), .definition = std::move(definition)});
  }
  mysql_free_result(result);

  for (auto& row : rows) {
    const auto choice_sql =
        "SELECT choice_key, yaml_value, label, description FROM game_option_choices WHERE option_id = " +
        std::to_string(row.id) + " ORDER BY sort_order ASC, id ASC";
    ensure_connected();
    if (mysql_real_query(connection_, choice_sql.data(), choice_sql.size()) != 0) {
      throw std::runtime_error(mysql_error_message(connection_, "seed_config_load_choices_failed"));
    }
    MYSQL_RES* choices = mysql_store_result(connection_);
    if (choices == nullptr) {
      throw std::runtime_error(mysql_error_message(connection_, "seed_config_load_choices_store_failed"));
    }
    while (auto* choice_row = mysql_fetch_row(choices)) {
      const auto lengths = mysql_fetch_lengths(choices);
      row.definition.choices.push_back(SeedConfigOptionChoice{
          .choice_key = choice_row[0] == nullptr ? "" : std::string(choice_row[0], lengths[0]),
          .yaml_value = choice_row[1] == nullptr ? "" : std::string(choice_row[1], lengths[1]),
          .label = choice_row[2] == nullptr ? "" : std::string(choice_row[2], lengths[2]),
          .description = choice_row[3] == nullptr ? "" : std::string(choice_row[3], lengths[3]),
      });
    }
    mysql_free_result(choices);
  }

  std::vector<SeedConfigOptionDefinition> definitions;
  definitions.reserve(rows.size());
  for (auto& row : rows) {
    definitions.push_back(std::move(row.definition));
  }
  return definitions;
}

std::int64_t SeedConfigMysqlStore::create_user_config(
    std::int64_t user_id,
    std::int64_t game_id,
    const std::string& name,
    const std::string& description,
    bool is_default) {
  return execute_insert(
      "INSERT INTO user_game_configs (user_id, game_id, name, description, is_default) VALUES (?, ?, ?, ?, ?)",
      {
          int64_value(user_id),
          int64_value(game_id),
          string_value(name),
          string_value(description),
          bool_value(is_default),
      });
}

std::int64_t SeedConfigMysqlStore::append_user_config_version(
    std::int64_t config_id,
    std::int64_t schema_version_id,
    const nlohmann::json& canonical_values,
    const std::string& values_hash,
    const std::optional<std::string>& source_yaml,
    const std::string& validation_status,
    const nlohmann::json& validation_errors) {
  return execute_insert(
      "INSERT INTO user_game_config_versions "
      "(config_id, schema_version_id, values_json, values_hash, source_yaml, validation_status, validation_errors_json) "
      "VALUES (?, ?, ?, ?, ?, ?, ?)",
      {
          int64_value(config_id),
          int64_value(schema_version_id),
          json_value(canonical_values),
          string_value(values_hash),
          nullable_string_value(source_yaml),
          string_value(validation_status),
          validation_errors.is_null() ? QueryValue{.is_null = true} : json_value(validation_errors),
      });
}

void SeedConfigMysqlStore::set_current_config_version(std::int64_t config_id, std::int64_t version_id) {
  execute_prepared(
      "UPDATE user_game_configs SET current_version_id = ? WHERE id = ?",
      {
          int64_value(version_id),
          int64_value(config_id),
      });
}

std::vector<SeedConfigUserConfigRecord> SeedConfigMysqlStore::list_user_configs(
    std::int64_t user_id,
    std::optional<std::int64_t> game_id) const {
  ensure_connected();
  auto sql =
      "SELECT id, user_id, game_id, name, description, is_default, current_version_id "
      "FROM user_game_configs WHERE archived_at IS NULL AND user_id = " +
      std::to_string(user_id);
  if (game_id.has_value()) {
    sql += " AND game_id = " + std::to_string(*game_id);
  }
  sql += " ORDER BY game_id ASC, name ASC";

  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_list_user_configs_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    if (mysql_field_count(connection_) == 0) {
      return {};
    }
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_list_user_configs_store_failed"));
  }

  std::vector<SeedConfigUserConfigRecord> records;
  while (auto* row = mysql_fetch_row(result)) {
    const auto lengths = mysql_fetch_lengths(result);
    SeedConfigUserConfigRecord record;
    record.id = row[0] == nullptr ? 0 : std::stoll(row[0]);
    record.user_id = row[1] == nullptr ? 0 : std::stoll(row[1]);
    record.game_id = row[2] == nullptr ? 0 : std::stoll(row[2]);
    record.name = row[3] == nullptr ? "" : std::string(row[3], lengths[3]);
    record.description = row[4] == nullptr ? "" : std::string(row[4], lengths[4]);
    record.is_default = row[5] != nullptr && std::string(row[5], lengths[5]) == "1";
    if (row[6] != nullptr) {
      record.current_version_id = std::stoll(row[6]);
    }
    records.push_back(std::move(record));
  }
  mysql_free_result(result);
  return records;
}

void SeedConfigMysqlStore::open() const {
  if (config_.database.empty()) {
    throw std::runtime_error("seed_config_mysql_database_required");
  }
  connection_ = mysql_init(nullptr);
  if (connection_ == nullptr) {
    throw std::runtime_error("seed_config_mysql_init_failed");
  }
  const auto* host = config_.host.empty() ? nullptr : config_.host.c_str();
  const auto* user = config_.user.empty() ? nullptr : config_.user.c_str();
  const auto* password = config_.password.empty() ? nullptr : config_.password.c_str();
  const auto* database = config_.database.c_str();
  const auto* socket = config_.unix_socket.empty() ? nullptr : config_.unix_socket.c_str();
  if (mysql_real_connect(connection_, host, user, password, database, config_.port, socket, 0) == nullptr) {
    const auto message = mysql_error_message(connection_, "seed_config_mysql_connect_failed");
    close();
    throw std::runtime_error(message);
  }
}

void SeedConfigMysqlStore::close() const {
  if (connection_ != nullptr) {
    mysql_close(connection_);
    connection_ = nullptr;
  }
}

void SeedConfigMysqlStore::ensure_connected() const {
  if (connection_ == nullptr) {
    open();
    return;
  }
  if (mysql_ping(connection_) != 0) {
    close();
    open();
  }
}

void SeedConfigMysqlStore::init_schema() {
  const auto schema = seed_config_mysql_schema_sql();
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

void SeedConfigMysqlStore::exec(const std::string& sql) const {
  ensure_connected();
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_mysql_query_failed"));
  }
}

void SeedConfigMysqlStore::execute_prepared(const std::string& sql, const std::vector<QueryValue>& values) const {
  ensure_connected();
  MYSQL_STMT* stmt = mysql_stmt_init(connection_);
  if (stmt == nullptr) {
    throw std::runtime_error("seed_config_mysql_stmt_init_failed");
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      mysql_stmt_close(stmt);
      stmt = nullptr;
    }
  };
  try {
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_prepare_failed"));
    }
    std::vector<MYSQL_BIND> binds(values.size());
    std::vector<unsigned long> lengths(values.size(), 0);
    std::vector<my_bool> nulls(values.size(), 0);
    for (std::size_t i = 0; i < values.size(); ++i) {
      bind_text(binds[i], lengths[i], nulls[i], values[i]);
    }
    if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_bind_failed"));
    }
    if (mysql_stmt_execute(stmt) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_execute_failed"));
    }
    cleanup();
  } catch (...) {
    cleanup();
    throw;
  }
}

std::int64_t SeedConfigMysqlStore::execute_insert(const std::string& sql, const std::vector<QueryValue>& values) const {
  execute_prepared(sql, values);
  return static_cast<std::int64_t>(mysql_insert_id(connection_));
}

std::optional<std::int64_t> SeedConfigMysqlStore::query_single_id(
    const std::string& sql,
    const std::vector<QueryValue>& values) const {
  ensure_connected();
  MYSQL_STMT* stmt = mysql_stmt_init(connection_);
  if (stmt == nullptr) {
    throw std::runtime_error("seed_config_mysql_stmt_init_failed");
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      mysql_stmt_close(stmt);
      stmt = nullptr;
    }
  };
  try {
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_id_prepare_failed"));
    }
    std::vector<MYSQL_BIND> binds(values.size());
    std::vector<unsigned long> lengths(values.size(), 0);
    std::vector<my_bool> nulls(values.size(), 0);
    for (std::size_t i = 0; i < values.size(); ++i) {
      bind_text(binds[i], lengths[i], nulls[i], values[i]);
    }
    if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_id_bind_failed"));
    }
    if (mysql_stmt_execute(stmt) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_id_execute_failed"));
    }
    long long id = 0;
    my_bool is_null = 0;
    MYSQL_BIND result{};
    result.buffer_type = MYSQL_TYPE_LONGLONG;
    result.buffer = &id;
    result.is_null = &is_null;
    if (mysql_stmt_bind_result(stmt, &result) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_id_result_bind_failed"));
    }
    const auto fetch = mysql_stmt_fetch(stmt);
    if (fetch == MYSQL_NO_DATA || is_null) {
      cleanup();
      return std::nullopt;
    }
    if (fetch != 0 && fetch != MYSQL_DATA_TRUNCATED) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_mysql_id_fetch_failed"));
    }
    cleanup();
    return static_cast<std::int64_t>(id);
  } catch (...) {
    cleanup();
    throw;
  }
}

std::vector<SeedConfigUserConfigSnapshot> SeedConfigMysqlStore::list_user_config_snapshots(
    std::int64_t user_id,
    const std::optional<std::string>& game_key) const {
  auto sql =
      "SELECT c.id, v.id, c.user_id, g.game_key, c.name, c.description, v.values_json, v.values_hash "
      "FROM user_game_configs c "
      "JOIN games g ON g.id = c.game_id "
      "JOIN user_game_config_versions v ON v.id = c.current_version_id "
      "WHERE c.archived_at IS NULL AND c.user_id = " +
      std::to_string(user_id);
  if (game_key.has_value()) {
    sql += " AND g.game_key = " + quote(*game_key);
  }
  sql += " ORDER BY g.game_key ASC, c.name ASC";
  ensure_connected();
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_list_snapshots_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    throw std::runtime_error(mysql_error_message(connection_, "seed_config_list_snapshots_store_failed"));
  }
  std::vector<SeedConfigUserConfigSnapshot> records;
  while (auto* row = mysql_fetch_row(result)) {
    const auto lengths = mysql_fetch_lengths(result);
    records.push_back(SeedConfigUserConfigSnapshot{
        .config_id = row[0] == nullptr ? 0 : std::stoll(row[0]),
        .version_id = row[1] == nullptr ? 0 : std::stoll(row[1]),
        .user_id = row[2] == nullptr ? 0 : std::stoll(row[2]),
        .game_key = row[3] == nullptr ? "" : std::string(row[3], lengths[3]),
        .name = row[4] == nullptr ? "" : std::string(row[4], lengths[4]),
        .description = row[5] == nullptr ? "" : std::string(row[5], lengths[5]),
        .values = row[6] == nullptr ? nlohmann::json::object() : nlohmann::json::parse(std::string(row[6], lengths[6])),
        .values_hash = row[7] == nullptr ? "" : std::string(row[7], lengths[7]),
    });
  }
  mysql_free_result(result);
  return records;
}

std::optional<SeedConfigUserConfigSnapshot> SeedConfigMysqlStore::find_user_config_snapshot(
    std::int64_t user_id,
    std::int64_t config_id) const {
  auto records = list_user_config_snapshots(user_id);
  const auto it = std::find_if(records.begin(), records.end(), [&](const auto& record) {
    return record.config_id == config_id;
  });
  if (it == records.end()) {
    return std::nullopt;
  }
  return *it;
}

bool SeedConfigMysqlStore::archive_user_config(std::int64_t user_id, std::int64_t config_id) {
  ensure_connected();
  MYSQL_STMT* stmt = mysql_stmt_init(connection_);
  if (stmt == nullptr) {
    throw std::runtime_error("seed_config_mysql_stmt_init_failed");
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      mysql_stmt_close(stmt);
      stmt = nullptr;
    }
  };

  try {
    const std::string sql =
        "UPDATE user_game_configs SET archived_at = CURRENT_TIMESTAMP "
        "WHERE id = ? AND user_id = ? AND archived_at IS NULL";
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_archive_user_config_prepare_failed"));
    }
    std::vector<MYSQL_BIND> params(2);
    std::vector<unsigned long> lengths(2, 0);
    std::vector<my_bool> nulls(2, 0);
    const auto config_id_value = int64_value(config_id);
    const auto user_id_value = int64_value(user_id);
    bind_text(params[0], lengths[0], nulls[0], config_id_value);
    bind_text(params[1], lengths[1], nulls[1], user_id_value);
    if (mysql_stmt_bind_param(stmt, params.data()) != 0 || mysql_stmt_execute(stmt) != 0) {
      throw std::runtime_error(mysql_stmt_error_message(stmt, "seed_config_archive_user_config_execute_failed"));
    }
    const auto affected = mysql_stmt_affected_rows(stmt);
    cleanup();
    return affected > 0;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::string SeedConfigMysqlStore::quote(const std::string& value) const {
  ensure_connected();
  std::string escaped;
  escaped.resize(value.size() * 2 + 1);
  const auto written = mysql_real_escape_string(connection_, escaped.data(), value.data(), value.size());
  escaped.resize(written);
  return "'" + escaped + "'";
}

SeedConfigMysqlStore::QueryValue SeedConfigMysqlStore::string_value(const std::string& value) {
  return QueryValue{.value = value, .is_null = false};
}

SeedConfigMysqlStore::QueryValue SeedConfigMysqlStore::int64_value(std::int64_t value) {
  return QueryValue{.value = std::to_string(value), .is_null = false};
}

SeedConfigMysqlStore::QueryValue SeedConfigMysqlStore::bool_value(bool value) {
  return QueryValue{.value = value ? "1" : "0", .is_null = false};
}

SeedConfigMysqlStore::QueryValue SeedConfigMysqlStore::json_value(const nlohmann::json& value) {
  return QueryValue{.value = value.dump(), .is_null = false};
}

SeedConfigMysqlStore::QueryValue SeedConfigMysqlStore::nullable_string_value(const std::optional<std::string>& value) {
  if (!value.has_value()) {
    return QueryValue{.is_null = true};
  }
  return string_value(*value);
}

SeedConfigMysqlStore::QueryValue SeedConfigMysqlStore::nullable_int64_value(const std::optional<std::int64_t>& value) {
  if (!value.has_value()) {
    return QueryValue{.is_null = true};
  }
  return int64_value(*value);
}

}  // namespace sekailink_server
