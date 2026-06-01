#include "sekailink_server/seed_config_sql.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    const auto schema = sekailink_server::seed_config_mysql_schema_sql();
    const std::vector<std::string> required_tables = {
        "CREATE TABLE IF NOT EXISTS games",
        "CREATE TABLE IF NOT EXISTS game_option_schema_versions",
        "CREATE TABLE IF NOT EXISTS game_option_groups",
        "CREATE TABLE IF NOT EXISTS game_option_definitions",
        "CREATE TABLE IF NOT EXISTS game_option_choices",
        "CREATE TABLE IF NOT EXISTS user_game_configs",
        "CREATE TABLE IF NOT EXISTS user_game_config_versions",
        "CREATE TABLE IF NOT EXISTS common_game_presets",
        "CREATE TABLE IF NOT EXISTS common_game_preset_versions",
        "CREATE TABLE IF NOT EXISTS user_seed_instances",
        "CREATE TABLE IF NOT EXISTS seed_config_audit",
    };
    for (const auto& table : required_tables) {
      require(schema.find(table) != std::string::npos, "missing_table:" + table);
    }
    require(schema.find("uq_user_game_config_active_name") != std::string::npos, "missing_active_name_unique_key");
    require(schema.find("uq_common_game_preset_key") != std::string::npos, "missing_common_preset_unique_key");
    require(schema.find("uq_common_game_preset_version_hash") != std::string::npos, "missing_common_preset_version_hash_key");
    require(schema.find("FOREIGN KEY") != std::string::npos, "missing_foreign_keys");
    require(schema.find("ENGINE=InnoDB") != std::string::npos, "missing_innodb");
    require(schema.find("DEFAULT CHARSET=utf8mb4") != std::string::npos, "missing_utf8mb4");
    std::cout << "seed_config_sql_smoke_ok\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "seed_config_sql_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
