#pragma once

#include "sekailink_server/seed_config_mysql.hpp"
#include "sekailink_server/seed_config_service.hpp"

#include "nlohmann/json.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sekailink_server {

struct SeedConfigApiConfig {
  std::uint16_t http_port = 19106;
  std::string listen_host = "127.0.0.1";
  std::string admin_token;
  std::string user_token;
  std::filesystem::path state_path;
  std::optional<MysqlConnectionConfig> mysql;
};

SeedConfigApiConfig load_seed_config_api_config(const std::filesystem::path& path);
nlohmann::json to_json(const SeedConfigApiConfig& config);

class SeedConfigApiService {
 public:
  explicit SeedConfigApiService(SeedConfigApiConfig config);

  [[nodiscard]] nlohmann::json handle(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token,
      const std::optional<nlohmann::json>& body);

 private:
  struct GameCatalogEntry {
    std::string game_key;
    std::string display_name;
    std::string system_key;
    std::string linkedworld_id;
    std::string schema_version;
    std::string source_kind;
    std::string source_hash;
    std::vector<SeedConfigOptionDefinition> definitions;
  };

  struct UserConfigEntry {
    std::int64_t config_id = 0;
    std::int64_t version_id = 0;
    std::int64_t user_id = 0;
    std::string game_key;
    std::string name;
    nlohmann::json values = nlohmann::json::object();
    std::string values_hash;
  };

  struct CommonPresetEntry {
    std::int64_t preset_id = 0;
    std::int64_t version_id = 0;
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

  [[nodiscard]] bool authorized_admin(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] bool authorized_user(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] bool persistent() const;

  [[nodiscard]] nlohmann::json handle_import_game(const nlohmann::json& body);
  [[nodiscard]] nlohmann::json handle_import_common_preset(const std::string& game_key, const nlohmann::json& body);
  [[nodiscard]] nlohmann::json handle_delete_common_preset(const std::string& game_key, const std::string& preset_key);
  [[nodiscard]] nlohmann::json handle_list_games() const;
  [[nodiscard]] nlohmann::json handle_game_options(const std::string& game_key) const;
  [[nodiscard]] nlohmann::json handle_list_common_presets(const std::string& game_key) const;
  [[nodiscard]] nlohmann::json handle_save_user_config(std::int64_t user_id, const nlohmann::json& body);
  [[nodiscard]] nlohmann::json handle_duplicate_common_preset(std::int64_t user_id, const nlohmann::json& body);
  [[nodiscard]] nlohmann::json handle_list_user_configs(std::int64_t user_id, const std::optional<std::string>& game_key) const;
  [[nodiscard]] nlohmann::json handle_delete_user_config(std::int64_t user_id, std::int64_t config_id);
  [[nodiscard]] nlohmann::json handle_export_user_config_yaml(std::int64_t user_id, std::int64_t config_id) const;
  [[nodiscard]] nlohmann::json handle_sklconf_manifest(std::int64_t user_id) const;

  [[nodiscard]] nlohmann::json game_to_json(const GameCatalogEntry& game, bool include_options) const;
  [[nodiscard]] nlohmann::json user_config_to_json(const UserConfigEntry& config) const;
  [[nodiscard]] nlohmann::json common_preset_to_json(const CommonPresetEntry& preset) const;
  [[nodiscard]] std::optional<UserConfigEntry> find_user_config(std::int64_t user_id, std::int64_t config_id) const;
  [[nodiscard]] std::optional<CommonPresetEntry> find_common_preset(std::int64_t preset_id) const;

  SeedConfigApiConfig config_;
  std::unique_ptr<SeedConfigMysqlStore> store_;
  std::unordered_map<std::string, GameCatalogEntry> games_;
  std::vector<UserConfigEntry> user_configs_;
  std::vector<CommonPresetEntry> common_presets_;
  std::int64_t next_config_id_ = 1;
  std::int64_t next_version_id_ = 1;
  std::int64_t next_common_preset_id_ = 1;
  std::int64_t next_common_preset_version_id_ = 1;
};

class SeedConfigHttpServer {
 public:
  explicit SeedConfigHttpServer(SeedConfigApiConfig config);
  ~SeedConfigHttpServer();

  SeedConfigHttpServer(const SeedConfigHttpServer&) = delete;
  SeedConfigHttpServer& operator=(const SeedConfigHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one();

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  SeedConfigApiService service_;
  SeedConfigApiConfig config_;
};

}  // namespace sekailink_server
