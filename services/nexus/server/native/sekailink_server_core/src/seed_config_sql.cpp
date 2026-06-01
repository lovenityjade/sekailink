#include "sekailink_server/seed_config_sql.hpp"

namespace sekailink_server {

std::string seed_config_mysql_schema_sql() {
  return R"SQL(
CREATE TABLE IF NOT EXISTS games (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  game_key VARCHAR(96) NOT NULL UNIQUE,
  display_name VARCHAR(255) NOT NULL,
  system_key VARCHAR(64) NOT NULL,
  active_linkedworld_id VARCHAR(128),
  active_option_schema_id BIGINT,
  status VARCHAR(32) NOT NULL DEFAULT 'active',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_games_status (status),
  INDEX idx_games_system (system_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS game_option_schema_versions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  game_id BIGINT NOT NULL,
  schema_version VARCHAR(96) NOT NULL,
  source_kind VARCHAR(64) NOT NULL,
  source_hash VARCHAR(128) NOT NULL,
  source_ref VARCHAR(512) NOT NULL DEFAULT '',
  imported_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  deprecated_at TIMESTAMP NULL,
  UNIQUE KEY uq_game_option_schema_version (game_id, schema_version),
  INDEX idx_game_option_schema_active (game_id, deprecated_at),
  CONSTRAINT fk_game_option_schema_game FOREIGN KEY (game_id) REFERENCES games(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS game_option_groups (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  schema_version_id BIGINT NOT NULL,
  group_key VARCHAR(128) NOT NULL,
  label VARCHAR(255) NOT NULL,
  description TEXT NOT NULL,
  sort_order INT NOT NULL DEFAULT 0,
  UNIQUE KEY uq_game_option_group (schema_version_id, group_key),
  INDEX idx_game_option_group_order (schema_version_id, sort_order),
  CONSTRAINT fk_game_option_group_schema FOREIGN KEY (schema_version_id) REFERENCES game_option_schema_versions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS game_option_definitions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  schema_version_id BIGINT NOT NULL,
  group_id BIGINT,
  option_key VARCHAR(160) NOT NULL,
  yaml_key VARCHAR(160) NOT NULL,
  label VARCHAR(255) NOT NULL,
  description TEXT NOT NULL,
  option_type VARCHAR(48) NOT NULL,
  default_value_json JSON NOT NULL,
  required TINYINT(1) NOT NULL DEFAULT 1,
  sort_order INT NOT NULL DEFAULT 0,
  visibility_rules_json JSON,
  validation_rules_json JSON,
  UNIQUE KEY uq_game_option_definition (schema_version_id, option_key),
  UNIQUE KEY uq_game_option_yaml_key (schema_version_id, yaml_key),
  INDEX idx_game_option_definition_order (schema_version_id, sort_order),
  CONSTRAINT fk_game_option_definition_schema FOREIGN KEY (schema_version_id) REFERENCES game_option_schema_versions(id),
  CONSTRAINT fk_game_option_definition_group FOREIGN KEY (group_id) REFERENCES game_option_groups(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS game_option_choices (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  option_id BIGINT NOT NULL,
  choice_key VARCHAR(160) NOT NULL,
  yaml_value VARCHAR(255) NOT NULL,
  label VARCHAR(255) NOT NULL,
  description TEXT NOT NULL,
  sort_order INT NOT NULL DEFAULT 0,
  UNIQUE KEY uq_game_option_choice (option_id, choice_key),
  UNIQUE KEY uq_game_option_choice_yaml (option_id, yaml_value),
  INDEX idx_game_option_choice_order (option_id, sort_order),
  CONSTRAINT fk_game_option_choice_option FOREIGN KEY (option_id) REFERENCES game_option_definitions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS user_game_configs (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  game_id BIGINT NOT NULL,
  name VARCHAR(255) NOT NULL,
  description TEXT NOT NULL,
  is_default TINYINT(1) NOT NULL DEFAULT 0,
  current_version_id BIGINT,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  archived_at TIMESTAMP NULL,
  active_name_key VARCHAR(255) GENERATED ALWAYS AS (IF(archived_at IS NULL, LOWER(name), NULL)) STORED,
  UNIQUE KEY uq_user_game_config_active_name (user_id, game_id, active_name_key),
  INDEX idx_user_game_configs_user_game (user_id, game_id, archived_at),
  CONSTRAINT fk_user_game_config_game FOREIGN KEY (game_id) REFERENCES games(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS user_game_config_versions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  config_id BIGINT NOT NULL,
  schema_version_id BIGINT NOT NULL,
  values_json JSON NOT NULL,
  values_hash VARCHAR(128) NOT NULL,
  source_yaml MEDIUMTEXT,
  validation_status VARCHAR(32) NOT NULL DEFAULT 'valid',
  validation_errors_json JSON,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_user_game_config_version_hash (config_id, values_hash),
  INDEX idx_user_game_config_versions_config (config_id, created_at),
  CONSTRAINT fk_user_game_config_version_config FOREIGN KEY (config_id) REFERENCES user_game_configs(id),
  CONSTRAINT fk_user_game_config_version_schema FOREIGN KEY (schema_version_id) REFERENCES game_option_schema_versions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS common_game_presets (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  game_id BIGINT NOT NULL,
  preset_key VARCHAR(160) NOT NULL,
  name VARCHAR(255) NOT NULL,
  description TEXT NOT NULL,
  category VARCHAR(128) NOT NULL DEFAULT '',
  visibility VARCHAR(32) NOT NULL DEFAULT 'official',
  sort_order INT NOT NULL DEFAULT 0,
  active_version_id BIGINT,
  status VARCHAR(32) NOT NULL DEFAULT 'active',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  archived_at TIMESTAMP NULL,
  UNIQUE KEY uq_common_game_preset_key (game_id, preset_key),
  INDEX idx_common_game_presets_game (game_id, status, sort_order),
  CONSTRAINT fk_common_game_preset_game FOREIGN KEY (game_id) REFERENCES games(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS common_game_preset_versions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  preset_id BIGINT NOT NULL,
  schema_version_id BIGINT NOT NULL,
  values_json JSON NOT NULL,
  values_hash VARCHAR(128) NOT NULL,
  validation_status VARCHAR(32) NOT NULL DEFAULT 'valid',
  validation_errors_json JSON,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_common_game_preset_version_hash (preset_id, values_hash),
  INDEX idx_common_game_preset_versions_preset (preset_id, created_at),
  CONSTRAINT fk_common_game_preset_version_preset FOREIGN KEY (preset_id) REFERENCES common_game_presets(id),
  CONSTRAINT fk_common_game_preset_version_schema FOREIGN KEY (schema_version_id) REFERENCES game_option_schema_versions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS user_seed_instances (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  game_id BIGINT NOT NULL,
  config_version_id BIGINT NOT NULL,
  seed_id VARCHAR(160),
  room_id VARCHAR(160),
  slot_id INT,
  status VARCHAR(32) NOT NULL DEFAULT 'pending',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  completed_at TIMESTAMP NULL,
  archived_at TIMESTAMP NULL,
  INDEX idx_user_seed_instances_user_game (user_id, game_id, created_at),
  INDEX idx_user_seed_instances_seed (seed_id),
  INDEX idx_user_seed_instances_room (room_id),
  CONSTRAINT fk_user_seed_instance_game FOREIGN KEY (game_id) REFERENCES games(id),
  CONSTRAINT fk_user_seed_instance_config_version FOREIGN KEY (config_version_id) REFERENCES user_game_config_versions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS seed_config_audit (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT,
  event_type VARCHAR(96) NOT NULL,
  target_type VARCHAR(96) NOT NULL,
  target_id VARCHAR(160) NOT NULL,
  payload_json JSON NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_seed_config_audit_user (user_id, created_at),
  INDEX idx_seed_config_audit_target (target_type, target_id),
  INDEX idx_seed_config_audit_event (event_type, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
)SQL";
}

}  // namespace sekailink_server
