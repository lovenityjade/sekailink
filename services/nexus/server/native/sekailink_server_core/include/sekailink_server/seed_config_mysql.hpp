#pragma once

#include "sekailink_server/room_projection_mysql.hpp"
#include "sekailink_server/seed_config_service.hpp"

#include "nlohmann/json.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

typedef struct st_mysql MYSQL;

namespace sekailink_server {

struct SeedConfigGameRecord {
  std::int64_t id = 0;
  std::string game_key;
  std::string display_name;
  std::string system_key;
  std::string active_linkedworld_id;
  std::string status;
};

struct SeedConfigUserConfigRecord {
  std::int64_t id = 0;
  std::int64_t user_id = 0;
  std::int64_t game_id = 0;
  std::string name;
  std::string description;
  bool is_default = false;
  std::optional<std::int64_t> current_version_id;
};

struct SeedConfigOptionSchemaRecord {
  std::int64_t id = 0;
  std::int64_t game_id = 0;
  std::string game_key;
  std::string schema_version;
};

struct SeedConfigUserConfigSnapshot {
  std::int64_t config_id = 0;
  std::int64_t version_id = 0;
  std::int64_t user_id = 0;
  std::string game_key;
  std::string name;
  nlohmann::json values = nlohmann::json::object();
  std::string values_hash;
};

struct SeedConfigCommonPresetSnapshot {
  std::int64_t preset_id = 0;
  std::int64_t version_id = 0;
  std::int64_t game_id = 0;
  std::int64_t schema_version_id = 0;
  std::string game_key;
  std::string preset_key;
  std::string name;
  std::string description;
  std::string category;
  std::string visibility;
  int sort_order = 0;
  nlohmann::json values = nlohmann::json::object();
  std::string values_hash;
};

class SeedConfigMysqlStore {
 public:
  struct QueryValue {
    std::string value;
    bool is_null = false;
  };

  explicit SeedConfigMysqlStore(MysqlConnectionConfig config);
  ~SeedConfigMysqlStore();

  SeedConfigMysqlStore(const SeedConfigMysqlStore&) = delete;
  SeedConfigMysqlStore& operator=(const SeedConfigMysqlStore&) = delete;

  [[nodiscard]] const MysqlConnectionConfig& config() const;

  [[nodiscard]] std::int64_t upsert_game(
      const std::string& game_key,
      const std::string& display_name,
      const std::string& system_key,
      const std::string& active_linkedworld_id,
      const std::string& status = "active");
  [[nodiscard]] std::optional<SeedConfigGameRecord> find_game_by_key(const std::string& game_key) const;
  [[nodiscard]] std::vector<SeedConfigGameRecord> list_games() const;

  [[nodiscard]] std::int64_t create_option_schema_version(
      std::int64_t game_id,
      const std::string& schema_version,
      const std::string& source_kind,
      const std::string& source_hash,
      const std::string& source_ref);
  [[nodiscard]] std::int64_t create_option_group(
      std::int64_t schema_version_id,
      const std::string& group_key,
      const std::string& label,
      const std::string& description,
      int sort_order);
  [[nodiscard]] std::int64_t create_option_definition(
      std::int64_t schema_version_id,
      const std::optional<std::int64_t>& group_id,
      const SeedConfigOptionDefinition& definition,
      int sort_order,
      const nlohmann::json& visibility_rules = nullptr,
      const nlohmann::json& validation_rules = nullptr);
  void create_option_choice(
      std::int64_t option_id,
      const SeedConfigOptionChoice& choice,
      int sort_order);
  void set_active_option_schema(std::int64_t game_id, std::int64_t schema_version_id);
  [[nodiscard]] std::optional<SeedConfigOptionSchemaRecord> find_active_option_schema(const std::string& game_key) const;
  [[nodiscard]] std::vector<SeedConfigOptionDefinition> load_option_definitions(std::int64_t schema_version_id) const;

  [[nodiscard]] std::int64_t create_user_config(
      std::int64_t user_id,
      std::int64_t game_id,
      const std::string& name,
      const std::string& description,
      bool is_default);
  [[nodiscard]] std::int64_t append_user_config_version(
      std::int64_t config_id,
      std::int64_t schema_version_id,
      const nlohmann::json& canonical_values,
      const std::string& values_hash,
      const std::optional<std::string>& source_yaml,
      const std::string& validation_status,
      const nlohmann::json& validation_errors);
  void set_current_config_version(std::int64_t config_id, std::int64_t version_id);
  [[nodiscard]] std::vector<SeedConfigUserConfigRecord> list_user_configs(
      std::int64_t user_id,
      std::optional<std::int64_t> game_id = std::nullopt) const;
  [[nodiscard]] std::vector<SeedConfigUserConfigSnapshot> list_user_config_snapshots(
      std::int64_t user_id,
      const std::optional<std::string>& game_key = std::nullopt) const;
  [[nodiscard]] std::optional<SeedConfigUserConfigSnapshot> find_user_config_snapshot(
      std::int64_t user_id,
      std::int64_t config_id) const;
  [[nodiscard]] bool archive_user_config(
      std::int64_t user_id,
      std::int64_t config_id);
  [[nodiscard]] std::int64_t upsert_common_preset(
      std::int64_t game_id,
      const std::string& preset_key,
      const std::string& name,
      const std::string& description,
      const std::string& category,
      const std::string& visibility,
      int sort_order,
      const std::string& status);
  [[nodiscard]] std::int64_t append_common_preset_version(
      std::int64_t preset_id,
      std::int64_t schema_version_id,
      const nlohmann::json& canonical_values,
      const std::string& values_hash,
      const std::string& validation_status,
      const nlohmann::json& validation_errors);
  void set_current_common_preset_version(std::int64_t preset_id, std::int64_t version_id);
  [[nodiscard]] std::vector<SeedConfigCommonPresetSnapshot> list_common_preset_snapshots(
      const std::string& game_key) const;
  [[nodiscard]] std::optional<SeedConfigCommonPresetSnapshot> find_common_preset_snapshot(
      std::int64_t preset_id) const;
  [[nodiscard]] std::optional<SeedConfigCommonPresetSnapshot> find_common_preset_snapshot_by_key(
      const std::string& game_key,
      const std::string& preset_key) const;
  [[nodiscard]] bool delete_common_preset(
      const std::string& game_key,
      const std::string& preset_key);

 private:
  void open();
  void close();
  void init_schema();
  void exec(const std::string& sql) const;
  void execute_prepared(const std::string& sql, const std::vector<QueryValue>& values) const;
  [[nodiscard]] std::int64_t execute_insert(const std::string& sql, const std::vector<QueryValue>& values) const;
  [[nodiscard]] std::optional<std::int64_t> query_single_id(
      const std::string& sql,
      const std::vector<QueryValue>& values) const;
  [[nodiscard]] std::string quote(const std::string& value) const;

  [[nodiscard]] static QueryValue string_value(const std::string& value);
  [[nodiscard]] static QueryValue int64_value(std::int64_t value);
  [[nodiscard]] static QueryValue bool_value(bool value);
  [[nodiscard]] static QueryValue json_value(const nlohmann::json& value);
  [[nodiscard]] static QueryValue nullable_string_value(const std::optional<std::string>& value);
  [[nodiscard]] static QueryValue nullable_int64_value(const std::optional<std::int64_t>& value);

  MysqlConnectionConfig config_;
  MYSQL* connection_ = nullptr;
};

}  // namespace sekailink_server
