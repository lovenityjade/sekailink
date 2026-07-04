#include "sekailink_server/seed_config_mysql.hpp"

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#else
#error "mysql header not found"
#endif

#include <algorithm>
#include <stdexcept>

namespace sekailink_server {

namespace {

std::string preset_mysql_error_message(MYSQL* connection, const char* fallback) {
  if (connection == nullptr) {
    return fallback;
  }
  const auto* message = mysql_error(connection);
  if (message == nullptr || *message == '\0') {
    return fallback;
  }
  return message;
}

SeedConfigCommonPresetSnapshot preset_snapshot_from_row(MYSQL_ROW row, unsigned long* lengths) {
  return SeedConfigCommonPresetSnapshot{
      .preset_id = row[0] == nullptr ? 0 : std::stoll(row[0]),
      .version_id = row[1] == nullptr ? 0 : std::stoll(row[1]),
      .game_id = row[2] == nullptr ? 0 : std::stoll(row[2]),
      .schema_version_id = row[3] == nullptr ? 0 : std::stoll(row[3]),
      .game_key = row[4] == nullptr ? "" : std::string(row[4], lengths[4]),
      .preset_key = row[5] == nullptr ? "" : std::string(row[5], lengths[5]),
      .name = row[6] == nullptr ? "" : std::string(row[6], lengths[6]),
      .description = row[7] == nullptr ? "" : std::string(row[7], lengths[7]),
      .category = row[8] == nullptr ? "" : std::string(row[8], lengths[8]),
      .visibility = row[9] == nullptr ? "" : std::string(row[9], lengths[9]),
      .sort_order = row[10] == nullptr ? 0 : std::stoi(row[10]),
      .values = row[11] == nullptr ? nlohmann::json::object() : nlohmann::json::parse(std::string(row[11], lengths[11])),
      .values_hash = row[12] == nullptr ? "" : std::string(row[12], lengths[12]),
  };
}

std::string common_preset_select_sql() {
  return "SELECT p.id, v.id, p.game_id, v.schema_version_id, g.game_key, p.preset_key, p.name, "
         "p.description, p.category, p.visibility, p.sort_order, v.values_json, v.values_hash "
         "FROM common_game_presets p "
         "JOIN games g ON g.id = p.game_id "
         "JOIN common_game_preset_versions v ON v.id = p.active_version_id ";
}

}  // namespace

std::int64_t SeedConfigMysqlStore::upsert_common_preset(
    std::int64_t game_id,
    const std::string& preset_key,
    const std::string& name,
    const std::string& description,
    const std::string& category,
    const std::string& visibility,
    int sort_order,
    const std::string& status) {
  execute_prepared(
      "INSERT INTO common_game_presets "
      "(game_id, preset_key, name, description, category, visibility, sort_order, status) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON DUPLICATE KEY UPDATE name = VALUES(name), description = VALUES(description), "
      "category = VALUES(category), visibility = VALUES(visibility), sort_order = VALUES(sort_order), "
      "status = VALUES(status), archived_at = NULL",
      {
          int64_value(game_id),
          string_value(preset_key),
          string_value(name),
          string_value(description),
          string_value(category),
          string_value(visibility),
          int64_value(sort_order),
          string_value(status),
      });
  const auto id = query_single_id(
      "SELECT id FROM common_game_presets WHERE game_id = ? AND preset_key = ?",
      {
          int64_value(game_id),
          string_value(preset_key),
      });
  if (!id.has_value()) {
    throw std::runtime_error("seed_config_common_preset_upsert_missing_id");
  }
  return *id;
}

std::int64_t SeedConfigMysqlStore::append_common_preset_version(
    std::int64_t preset_id,
    std::int64_t schema_version_id,
    const nlohmann::json& canonical_values,
    const std::string& values_hash,
    const std::string& validation_status,
    const nlohmann::json& validation_errors) {
  const auto existing = query_single_id(
      "SELECT id FROM common_game_preset_versions WHERE preset_id = ? AND values_hash = ?",
      {
          int64_value(preset_id),
          string_value(values_hash),
      });
  if (existing.has_value()) {
    return *existing;
  }
  return execute_insert(
      "INSERT INTO common_game_preset_versions "
      "(preset_id, schema_version_id, values_json, values_hash, validation_status, validation_errors_json) "
      "VALUES (?, ?, ?, ?, ?, ?)",
      {
          int64_value(preset_id),
          int64_value(schema_version_id),
          json_value(canonical_values),
          string_value(values_hash),
          string_value(validation_status),
          validation_errors.is_null() ? QueryValue{.is_null = true} : json_value(validation_errors),
      });
}

void SeedConfigMysqlStore::set_current_common_preset_version(std::int64_t preset_id, std::int64_t version_id) {
  execute_prepared(
      "UPDATE common_game_presets SET active_version_id = ? WHERE id = ?",
      {
          int64_value(version_id),
          int64_value(preset_id),
      });
}

std::vector<SeedConfigCommonPresetSnapshot> SeedConfigMysqlStore::list_common_preset_snapshots(
    const std::string& game_key) const {
  auto sql = common_preset_select_sql() +
             "WHERE p.archived_at IS NULL AND p.status = 'active' AND g.game_key = " + quote(game_key) +
             " ORDER BY p.sort_order ASC, p.name ASC";
  ensure_connected();
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(preset_mysql_error_message(connection_, "seed_config_list_common_presets_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    throw std::runtime_error(preset_mysql_error_message(connection_, "seed_config_list_common_presets_store_failed"));
  }
  std::vector<SeedConfigCommonPresetSnapshot> records;
  while (auto* row = mysql_fetch_row(result)) {
    records.push_back(preset_snapshot_from_row(row, mysql_fetch_lengths(result)));
  }
  mysql_free_result(result);
  return records;
}

std::optional<SeedConfigCommonPresetSnapshot> SeedConfigMysqlStore::find_common_preset_snapshot(
    std::int64_t preset_id) const {
  auto sql = common_preset_select_sql() +
             "WHERE p.archived_at IS NULL AND p.status = 'active' AND p.id = " + std::to_string(preset_id);
  ensure_connected();
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(preset_mysql_error_message(connection_, "seed_config_find_common_preset_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    throw std::runtime_error(preset_mysql_error_message(connection_, "seed_config_find_common_preset_store_failed"));
  }
  std::optional<SeedConfigCommonPresetSnapshot> record;
  if (auto* row = mysql_fetch_row(result)) {
    record = preset_snapshot_from_row(row, mysql_fetch_lengths(result));
  }
  mysql_free_result(result);
  return record;
}

std::optional<SeedConfigCommonPresetSnapshot> SeedConfigMysqlStore::find_common_preset_snapshot_by_key(
    const std::string& game_key,
    const std::string& preset_key) const {
  auto sql = common_preset_select_sql() +
             "WHERE p.archived_at IS NULL AND p.status = 'active' AND g.game_key = " + quote(game_key) +
             " AND p.preset_key = " + quote(preset_key);
  ensure_connected();
  if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
    throw std::runtime_error(preset_mysql_error_message(connection_, "seed_config_find_common_preset_by_key_failed"));
  }
  MYSQL_RES* result = mysql_store_result(connection_);
  if (result == nullptr) {
    throw std::runtime_error(preset_mysql_error_message(connection_, "seed_config_find_common_preset_by_key_store_failed"));
  }
  std::optional<SeedConfigCommonPresetSnapshot> record;
  if (auto* row = mysql_fetch_row(result)) {
    record = preset_snapshot_from_row(row, mysql_fetch_lengths(result));
  }
  mysql_free_result(result);
  return record;
}

bool SeedConfigMysqlStore::delete_common_preset(const std::string& game_key, const std::string& preset_key) {
  const auto preset = find_common_preset_snapshot_by_key(game_key, preset_key);
  if (!preset.has_value()) {
    return false;
  }
  execute_prepared(
      "DELETE FROM common_game_preset_versions WHERE preset_id = ?",
      {
          int64_value(preset->preset_id),
      });
  execute_prepared(
      "DELETE FROM common_game_presets WHERE id = ?",
      {
          int64_value(preset->preset_id),
      });
  return true;
}

}  // namespace sekailink_server
