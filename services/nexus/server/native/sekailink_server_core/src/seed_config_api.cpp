#include "sekailink_server/seed_config_api.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <openssl/sha.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace sekailink_server {

namespace {

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < path.size()) {
    while (start < path.size() && path[start] == '/') {
      ++start;
    }
    if (start >= path.size()) {
      break;
    }
    auto end = path.find('/', start);
    if (end == std::string_view::npos) {
      end = path.size();
    }
    parts.emplace_back(path.substr(start, end - start));
    start = end;
  }
  return parts;
}

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  return value;
}

std::pair<std::string, nlohmann::json> split_path_and_query(std::string_view raw_path) {
  const auto query_pos = raw_path.find('?');
  if (query_pos == std::string_view::npos) {
    return {std::string(raw_path), nlohmann::json::object()};
  }
  nlohmann::json query = nlohmann::json::object();
  const auto query_string = raw_path.substr(query_pos + 1);
  std::size_t start = 0;
  while (start <= query_string.size()) {
    const auto end = query_string.find('&', start);
    const auto segment = query_string.substr(start, end == std::string_view::npos ? query_string.size() - start : end - start);
    if (!segment.empty()) {
      const auto equals = segment.find('=');
      const auto key = std::string(segment.substr(0, equals));
      const auto value = equals == std::string_view::npos ? std::string() : std::string(segment.substr(equals + 1));
      if (!key.empty()) {
        query[key] = value;
      }
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return {std::string(raw_path.substr(0, query_pos)), std::move(query)};
}

std::string http_status_text(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 404:
      return "Not Found";
    case 409:
      return "Conflict";
    default:
      return "Internal Server Error";
  }
}

std::string json_http_response(int status_code, const nlohmann::json& body) {
  const auto payload = body.dump();
  std::ostringstream stream;
  stream << "HTTP/1.1 " << status_code << ' ' << http_status_text(status_code) << "\r\n";
  stream << "Content-Type: application/json\r\n";
  stream << "Content-Length: " << payload.size() << "\r\n";
  stream << "Connection: close\r\n\r\n";
  stream << payload;
  return stream.str();
}

std::string required_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || !body.at(key).is_string() || body.at(key).get<std::string>().empty()) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  return body.at(key).get<std::string>();
}

std::int64_t parse_int64(const std::string& value, const char* error) {
  try {
    std::size_t parsed = 0;
    const auto out = std::stoll(value, &parsed);
    if (parsed != value.size()) {
      throw std::runtime_error(error);
    }
    return out;
  } catch (...) {
    throw std::runtime_error(error);
  }
}

std::string stable_hash(const nlohmann::json& value) {
  const auto dumped = value.dump();
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(dumped.data()), dumped.size(), hash);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : hash) {
    out << std::setw(2) << static_cast<int>(byte);
  }
  return out.str();
}

SeedConfigOptionDefinition option_definition_from_json(const nlohmann::json& json) {
  SeedConfigOptionDefinition definition;
  definition.option_key = required_string(json, "option_key");
  definition.yaml_key = json.value("yaml_key", definition.option_key);
  definition.label = json.value("label", definition.option_key);
  definition.description = json.value("description", "");
  definition.type = required_string(json, "type");
  definition.default_value = json.contains("default_value") ? json.at("default_value") : nlohmann::json(nullptr);
  definition.required = json.value("required", true);
  definition.visibility_rules = json.contains("visibility_rules") ? json.at("visibility_rules") : nlohmann::json(nullptr);
  definition.validation_rules = json.contains("validation_rules") ? json.at("validation_rules") : nlohmann::json(nullptr);
  if (json.contains("choices")) {
    if (!json.at("choices").is_array()) {
      throw std::runtime_error("invalid_choices");
    }
    for (const auto& choice_json : json.at("choices")) {
      definition.choices.push_back(SeedConfigOptionChoice{
          .choice_key = required_string(choice_json, "choice_key"),
          .yaml_value = choice_json.value("yaml_value", required_string(choice_json, "choice_key")),
          .label = choice_json.value("label", required_string(choice_json, "choice_key")),
          .description = choice_json.value("description", ""),
      });
    }
  }
  return definition;
}

}  // namespace

SeedConfigApiConfig load_seed_config_api_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("seed_config_api_config_open_failed");
  }
  nlohmann::json json;
  stream >> json;
  SeedConfigApiConfig config;
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("listen_host")) {
    config.listen_host = json.at("listen_host").get<std::string>();
  }
  if (json.contains("admin_token")) {
    config.admin_token = json.at("admin_token").get<std::string>();
  }
  if (json.contains("user_token")) {
    config.user_token = json.at("user_token").get<std::string>();
  }
  if (json.contains("state_path")) {
    config.state_path = json.at("state_path").get<std::string>();
  }
  if (json.contains("mysql") && json.at("mysql").is_object() && json.at("mysql").value("enabled", false)) {
    const auto& mysql = json.at("mysql");
    MysqlConnectionConfig mysql_config;
    mysql_config.host = mysql.value("host", "127.0.0.1");
    mysql_config.user = mysql.value("user", "");
    mysql_config.password = mysql.value("password", "");
    mysql_config.database = mysql.value("database", "");
    if (mysql.contains("port")) {
      mysql_config.port = mysql.at("port").get<std::uint32_t>();
    }
    mysql_config.unix_socket = mysql.value("unix_socket", "");
    config.mysql = mysql_config;
  }
  return config;
}

nlohmann::json to_json(const SeedConfigApiConfig& config) {
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"admin_token", config.admin_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"user_token", config.user_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"state_path", config.state_path.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.state_path.string())},
      {"mysql",
       config.mysql.has_value()
           ? nlohmann::json{
                 {"enabled", true},
                 {"host", config.mysql->host},
                 {"port", config.mysql->port},
                 {"database", config.mysql->database},
                 {"user", config.mysql->user.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
                 {"password", config.mysql->password.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
                 {"unix_socket", config.mysql->unix_socket.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.mysql->unix_socket)},
             }
           : nlohmann::json{{"enabled", false}}},
  };
}

SeedConfigApiService::SeedConfigApiService(SeedConfigApiConfig config)
    : config_(std::move(config)) {
  if (config_.mysql.has_value()) {
    store_ = std::make_unique<SeedConfigMysqlStore>(*config_.mysql);
  }
}

nlohmann::json SeedConfigApiService::handle(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token,
    const std::optional<nlohmann::json>& body) {
  try {
    if (path == "/seed-configs/health") {
      return {{"ok", true}, {"service", "sekailink_seed_config_api"}, {"format", "seed-config-api-v1"}};
    }

    const auto [normalized_path, query] = split_path_and_query(path);
    const auto parts = split_path(normalized_path);

    if (parts.size() == 3 && parts[0] == "admin" && parts[1] == "seed-configs" && parts[2] == "games" && method == "POST") {
      if (!authorized_admin(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      return handle_import_game(*body);
    }
    if (parts.size() == 5 && parts[0] == "admin" && parts[1] == "seed-configs" && parts[2] == "games" && parts[4] == "presets" && method == "POST") {
      if (!authorized_admin(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      return handle_import_common_preset(parts[3], *body);
    }
    if (parts.size() == 5 && parts[0] == "admin" && parts[1] == "seed-configs" && parts[2] == "games" && parts[4] == "presets" && method == "GET") {
      if (!authorized_admin(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_list_common_presets(parts[3]);
    }
    if (parts.size() == 6 && parts[0] == "admin" && parts[1] == "seed-configs" && parts[2] == "games" && parts[4] == "presets" && method == "PUT") {
      if (!authorized_admin(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      auto patched_body = *body;
      patched_body["preset_key"] = parts[5];
      return handle_import_common_preset(parts[3], patched_body);
    }
    if (parts.size() == 6 && parts[0] == "admin" && parts[1] == "seed-configs" && parts[2] == "games" && parts[4] == "presets" && method == "DELETE") {
      if (!authorized_admin(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_delete_common_preset(parts[3], parts[5]);
    }
    if (parts.size() == 2 && parts[0] == "seed-configs" && parts[1] == "games" && method == "GET") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_list_games();
    }
    if (parts.size() == 4 && parts[0] == "seed-configs" && parts[1] == "games" && parts[3] == "options" && method == "GET") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_game_options(parts[2]);
    }
    if (parts.size() == 4 && parts[0] == "seed-configs" && parts[1] == "games" && parts[3] == "presets" && method == "GET") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_list_common_presets(parts[2]);
    }
    if (parts.size() == 3 && parts[0] == "users" && parts[2] == "seed-configs" && method == "POST") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      return handle_save_user_config(parse_int64(parts[1], "invalid_user_id"), *body);
    }
    if (parts.size() == 4 && parts[0] == "users" && parts[2] == "seed-configs" && parts[3] == "from-preset" && method == "POST") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      return handle_duplicate_common_preset(parse_int64(parts[1], "invalid_user_id"), *body);
    }
    if (parts.size() == 3 && parts[0] == "users" && parts[2] == "seed-configs" && method == "GET") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      std::optional<std::string> game_key;
      if (query.contains("game_key") && query.at("game_key").is_string() && !query.at("game_key").get<std::string>().empty()) {
        game_key = query.at("game_key").get<std::string>();
      }
      return handle_list_user_configs(parse_int64(parts[1], "invalid_user_id"), game_key);
    }
    if (parts.size() == 4 && parts[0] == "users" && parts[2] == "seed-configs" && method == "DELETE") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_delete_user_config(
          parse_int64(parts[1], "invalid_user_id"),
          parse_int64(parts[3], "invalid_config_id"));
    }
    if (parts.size() == 5 && parts[0] == "users" && parts[2] == "seed-configs" && parts[4] == "export-yaml" && method == "POST") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_export_user_config_yaml(
          parse_int64(parts[1], "invalid_user_id"),
          parse_int64(parts[3], "invalid_config_id"));
    }
    if (parts.size() == 4 && parts[0] == "users" && parts[2] == "sklconf" && parts[3] == "manifest" && method == "GET") {
      if (!authorized_user(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
      return handle_sklconf_manifest(parse_int64(parts[1], "invalid_user_id"));
    }
    return {{"ok", false}, {"status", 404}, {"error", "not_found"}};
  } catch (const std::exception& exception) {
    const std::string error = exception.what();
    if (error.rfind("missing_", 0) == 0 || error.rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", error}};
    }
    return {{"ok", false}, {"status", 500}, {"error", error}};
  }
}

bool SeedConfigApiService::authorized_admin(const std::optional<std::string>& bearer_token) const {
  return bearer_token.has_value() && !config_.admin_token.empty() && *bearer_token == config_.admin_token;
}

bool SeedConfigApiService::authorized_user(const std::optional<std::string>& bearer_token) const {
  return bearer_token.has_value() && !config_.user_token.empty() && *bearer_token == config_.user_token;
}

bool SeedConfigApiService::persistent() const {
  return store_ != nullptr;
}

nlohmann::json SeedConfigApiService::handle_import_game(const nlohmann::json& body) {
  if (!body.contains("options") || !body.at("options").is_array()) {
    return {{"ok", false}, {"status", 400}, {"error", "invalid_options"}};
  }
  GameCatalogEntry game{
      .game_key = required_string(body, "game_key"),
      .display_name = required_string(body, "display_name"),
      .system_key = required_string(body, "system_key"),
      .linkedworld_id = body.value("linkedworld_id", ""),
      .schema_version = required_string(body, "schema_version"),
      .source_kind = body.value("source_kind", "linkedworld"),
      .source_hash = required_string(body, "source_hash"),
      .definitions = {},
  };
  for (const auto& option_json : body.at("options")) {
    game.definitions.push_back(option_definition_from_json(option_json));
  }
  if (persistent()) {
    const auto game_id = store_->upsert_game(game.game_key, game.display_name, game.system_key, game.linkedworld_id, "active");
    const auto schema_id = store_->create_option_schema_version(
        game_id,
        game.schema_version,
        game.source_kind,
        game.source_hash,
        body.value("source_ref", ""));
    std::unordered_map<std::string, std::int64_t> group_ids;
    if (body.contains("groups") && body.at("groups").is_array()) {
      for (std::size_t index = 0; index < body.at("groups").size(); ++index) {
        const auto& group_json = body.at("groups").at(index);
        const auto group_key = required_string(group_json, "group_key");
        group_ids[group_key] = store_->create_option_group(
            schema_id,
            group_key,
            group_json.value("label", group_key),
            group_json.value("description", ""),
            group_json.value("sort_order", static_cast<int>(index)));
      }
    }
    for (std::size_t index = 0; index < game.definitions.size(); ++index) {
      std::optional<std::int64_t> group_id;
      if (game.definitions[index].validation_rules.is_object()) {
        const auto group_it = game.definitions[index].validation_rules.find("group_key");
        if (group_it != game.definitions[index].validation_rules.end() && group_it->is_string()) {
          const auto found_group = group_ids.find(group_it->get<std::string>());
          if (found_group != group_ids.end()) {
            group_id = found_group->second;
          }
        }
      }
      const auto option_id = store_->create_option_definition(
          schema_id,
          group_id,
          game.definitions[index],
          static_cast<int>(index),
          game.definitions[index].visibility_rules,
          game.definitions[index].validation_rules);
      for (std::size_t choice_index = 0; choice_index < game.definitions[index].choices.size(); ++choice_index) {
        store_->create_option_choice(option_id, game.definitions[index].choices[choice_index], static_cast<int>(choice_index));
      }
    }
    store_->set_active_option_schema(game_id, schema_id);
    return {{"ok", true}, {"game", game_to_json(game, true)}, {"persistent", true}};
  }

  games_[game.game_key] = std::move(game);
  return {{"ok", true}, {"game", game_to_json(games_.at(required_string(body, "game_key")), true)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::handle_list_games() const {
  nlohmann::json games = nlohmann::json::array();
  if (persistent()) {
    for (const auto& game : store_->list_games()) {
      games.push_back({
          {"game_key", game.game_key},
          {"display_name", game.display_name},
          {"system_key", game.system_key},
          {"linkedworld_id", game.active_linkedworld_id},
          {"status", game.status},
      });
    }
    return {{"ok", true}, {"games", std::move(games)}, {"persistent", true}};
  }
  for (const auto& [_, game] : games_) {
    games.push_back(game_to_json(game, false));
  }
  return {{"ok", true}, {"games", std::move(games)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::handle_game_options(const std::string& game_key) const {
  if (persistent()) {
    const auto schema = store_->find_active_option_schema(game_key);
    const auto game = store_->find_game_by_key(game_key);
    if (!schema.has_value() || !game.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
    }
    GameCatalogEntry entry{
        .game_key = game->game_key,
        .display_name = game->display_name,
        .system_key = game->system_key,
        .linkedworld_id = game->active_linkedworld_id,
        .schema_version = schema->schema_version,
        .source_kind = "mariadb",
        .source_hash = "",
        .definitions = store_->load_option_definitions(schema->id),
    };
    return {{"ok", true}, {"game", game_to_json(entry, true)}, {"persistent", true}};
  }
  const auto it = games_.find(game_key);
  if (it == games_.end()) {
    return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
  }
  return {{"ok", true}, {"game", game_to_json(it->second, true)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::handle_save_user_config(std::int64_t user_id, const nlohmann::json& body) {
  const auto game_key = required_string(body, "game_key");
  const auto name = required_string(body, "name");
  if (!body.contains("values")) {
    return {{"ok", false}, {"status", 400}, {"error", "missing_values"}};
  }
  if (persistent()) {
    const auto schema = store_->find_active_option_schema(game_key);
    if (!schema.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
    }
    const auto definitions = store_->load_option_definitions(schema->id);
    auto validation = validate_seed_config_values(definitions, body.at("values"));
    if (!validation.ok) {
      return {
          {"ok", false},
          {"status", 400},
          {"error", "invalid_seed_config"},
          {"issues", seed_config_validation_issues_to_json(validation.issues)},
      };
    }
    const auto hash = stable_hash(validation.canonical_values);
    try {
      const auto config_id = store_->create_user_config(user_id, schema->game_id, name, body.value("description", ""), body.value("is_default", false));
      const auto version_id = store_->append_user_config_version(
          config_id,
          schema->id,
          validation.canonical_values,
          hash,
          std::nullopt,
          "valid",
          nlohmann::json(nullptr));
      store_->set_current_config_version(config_id, version_id);
      return {
          {"ok", true},
          {"persistent", true},
          {"config",
           {
               {"config_id", config_id},
               {"version_id", version_id},
               {"user_id", user_id},
               {"game_key", game_key},
               {"name", name},
               {"values", validation.canonical_values},
               {"values_hash", hash},
           }},
      };
    } catch (const std::exception& exception) {
      if (std::string(exception.what()).find("Duplicate") != std::string::npos) {
        return {{"ok", false}, {"status", 409}, {"error", "config_name_conflict"}};
      }
      throw;
    }
  }

  const auto game_it = games_.find(game_key);
  if (game_it == games_.end()) {
    return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
  }
  const auto duplicate = std::any_of(user_configs_.begin(), user_configs_.end(), [&](const auto& config) {
    return config.user_id == user_id && config.game_key == game_key && config.name == name;
  });
  if (duplicate) {
    return {{"ok", false}, {"status", 409}, {"error", "config_name_conflict"}};
  }
  auto validation = validate_seed_config_values(game_it->second.definitions, body.at("values"));
  if (!validation.ok) {
    return {
        {"ok", false},
        {"status", 400},
        {"error", "invalid_seed_config"},
        {"issues", seed_config_validation_issues_to_json(validation.issues)},
    };
  }

  UserConfigEntry config{
      .config_id = next_config_id_++,
      .version_id = next_version_id_++,
      .user_id = user_id,
      .game_key = game_key,
      .name = name,
      .values = validation.canonical_values,
      .values_hash = stable_hash(validation.canonical_values),
  };
  user_configs_.push_back(config);
  return {{"ok", true}, {"config", user_config_to_json(config)}};
}

nlohmann::json SeedConfigApiService::handle_list_user_configs(
    std::int64_t user_id,
    const std::optional<std::string>& game_key) const {
  nlohmann::json configs = nlohmann::json::array();
  if (persistent()) {
    for (const auto& config : store_->list_user_config_snapshots(user_id, game_key)) {
      configs.push_back({
          {"config_id", config.config_id},
          {"version_id", config.version_id},
          {"user_id", config.user_id},
          {"game_key", config.game_key},
          {"name", config.name},
          {"description", config.description},
          {"values", config.values},
          {"values_hash", config.values_hash},
      });
    }
    return {{"ok", true}, {"configs", std::move(configs)}, {"persistent", true}};
  }
  for (const auto& config : user_configs_) {
    if (config.user_id != user_id) {
      continue;
    }
    if (game_key.has_value() && config.game_key != *game_key) {
      continue;
    }
    configs.push_back(user_config_to_json(config));
  }
  return {{"ok", true}, {"configs", std::move(configs)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::handle_delete_user_config(std::int64_t user_id, std::int64_t config_id) {
  if (persistent()) {
    const auto archived = store_->archive_user_config(user_id, config_id);
    if (!archived) return {{"ok", false}, {"status", 404}, {"error", "config_not_found"}};
    return {{"ok", true}, {"persistent", true}, {"config_id", config_id}, {"archived", true}};
  }
  const auto before = user_configs_.size();
  user_configs_.erase(
      std::remove_if(
          user_configs_.begin(),
          user_configs_.end(),
          [&](const auto& config) {
            return config.user_id == user_id && config.config_id == config_id;
          }),
      user_configs_.end());
  if (user_configs_.size() == before) return {{"ok", false}, {"status", 404}, {"error", "config_not_found"}};
  return {{"ok", true}, {"persistent", false}, {"config_id", config_id}, {"archived", true}};
}

nlohmann::json SeedConfigApiService::handle_export_user_config_yaml(std::int64_t user_id, std::int64_t config_id) const {
  if (persistent()) {
    const auto config = store_->find_user_config_snapshot(user_id, config_id);
    if (!config.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "config_not_found"}};
    }
    const auto schema = store_->find_active_option_schema(config->game_key);
    if (!schema.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
    }
    return {
        {"ok", true},
        {"persistent", true},
        {"config_id", config->config_id},
        {"game_key", config->game_key},
        {"yaml", export_seed_config_yaml(store_->load_option_definitions(schema->id), config->values)},
    };
  }
  const auto config = find_user_config(user_id, config_id);
  if (!config.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "config_not_found"}};
  }
  const auto game_it = games_.find(config->game_key);
  if (game_it == games_.end()) {
    return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
  }
  return {
      {"ok", true},
      {"config_id", config->config_id},
      {"game_key", config->game_key},
      {"yaml", export_seed_config_yaml(game_it->second.definitions, config->values)},
  };
}

nlohmann::json SeedConfigApiService::handle_sklconf_manifest(std::int64_t user_id) const {
  std::vector<SeedConfigSyncEntry> entries;
  if (persistent()) {
    for (const auto& game : store_->list_games()) {
      const auto schema = store_->find_active_option_schema(game.game_key);
      if (!schema.has_value()) {
        continue;
      }
      entries.push_back(SeedConfigSyncEntry{
          .entity_type = "game_option_schema",
          .entity_id = game.game_key + ":" + schema->schema_version,
          .version = schema->schema_version,
          .hash = "",
          .updated_at = "",
      });
      for (const auto& preset : store_->list_common_preset_snapshots(game.game_key)) {
        entries.push_back(SeedConfigSyncEntry{
            .entity_type = "common_game_preset",
            .entity_id = std::to_string(preset.preset_id),
            .version = std::to_string(preset.version_id),
            .hash = preset.values_hash,
            .updated_at = "",
        });
      }
    }
    for (const auto& config : store_->list_user_config_snapshots(user_id)) {
      entries.push_back(SeedConfigSyncEntry{
          .entity_type = "user_seed_config",
          .entity_id = std::to_string(config.config_id),
          .version = std::to_string(config.version_id),
          .hash = config.values_hash,
          .updated_at = "",
      });
    }
    return {{"ok", true}, {"persistent", true}, {"manifest", seed_config_sync_manifest(user_id, entries)}};
  }
  for (const auto& [_, game] : games_) {
    entries.push_back(SeedConfigSyncEntry{
        .entity_type = "game_option_schema",
        .entity_id = game.game_key + ":" + game.schema_version,
        .version = game.schema_version,
        .hash = game.source_hash,
        .updated_at = "",
    });
  }
  for (const auto& preset : common_presets_) {
    entries.push_back(SeedConfigSyncEntry{
        .entity_type = "common_game_preset",
        .entity_id = std::to_string(preset.preset_id),
        .version = std::to_string(preset.version_id),
        .hash = preset.values_hash,
        .updated_at = "",
    });
  }
  for (const auto& config : user_configs_) {
    if (config.user_id != user_id) {
      continue;
    }
    entries.push_back(SeedConfigSyncEntry{
        .entity_type = "user_seed_config",
        .entity_id = std::to_string(config.config_id),
        .version = std::to_string(config.version_id),
        .hash = config.values_hash,
        .updated_at = "",
    });
  }
  return {{"ok", true}, {"manifest", seed_config_sync_manifest(user_id, entries)}};
}

nlohmann::json SeedConfigApiService::game_to_json(const GameCatalogEntry& game, bool include_options) const {
  nlohmann::json out = {
      {"game_key", game.game_key},
      {"display_name", game.display_name},
      {"system_key", game.system_key},
      {"linkedworld_id", game.linkedworld_id},
      {"schema_version", game.schema_version},
      {"source_kind", game.source_kind},
      {"source_hash", game.source_hash},
  };
  if (include_options) {
    out["options"] = nlohmann::json::array();
    for (const auto& definition : game.definitions) {
      nlohmann::json option = {
          {"option_key", definition.option_key},
          {"yaml_key", definition.yaml_key},
          {"label", definition.label},
          {"description", definition.description},
          {"type", definition.type},
          {"default_value", definition.default_value},
          {"required", definition.required},
          {"visibility_rules", definition.visibility_rules},
          {"validation_rules", definition.validation_rules},
          {"choices", nlohmann::json::array()},
      };
      for (const auto& choice : definition.choices) {
        option["choices"].push_back({
            {"choice_key", choice.choice_key},
            {"yaml_value", choice.yaml_value},
            {"label", choice.label},
            {"description", choice.description},
        });
      }
      out["options"].push_back(std::move(option));
    }
  }
  return out;
}

nlohmann::json SeedConfigApiService::user_config_to_json(const UserConfigEntry& config) const {
  return {
      {"config_id", config.config_id},
      {"version_id", config.version_id},
      {"user_id", config.user_id},
      {"game_key", config.game_key},
      {"name", config.name},
      {"values", config.values},
      {"values_hash", config.values_hash},
  };
}

std::optional<SeedConfigApiService::UserConfigEntry> SeedConfigApiService::find_user_config(
    std::int64_t user_id,
    std::int64_t config_id) const {
  const auto it = std::find_if(user_configs_.begin(), user_configs_.end(), [&](const auto& config) {
    return config.user_id == user_id && config.config_id == config_id;
  });
  if (it == user_configs_.end()) {
    return std::nullopt;
  }
  return *it;
}

SeedConfigHttpServer::SeedConfigHttpServer(SeedConfigApiConfig config)
    : service_(config), config_(std::move(config)) {}

SeedConfigHttpServer::~SeedConfigHttpServer() {
  stop();
}

bool SeedConfigHttpServer::start() {
#ifdef _WIN32
  return false;
#else
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return false;
  }
  int reuse = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config_.http_port);
  if (config_.listen_host == "0.0.0.0") {
    address.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  }
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    stop();
    return false;
  }
  if (::listen(listen_fd_, 16) != 0) {
    stop();
    return false;
  }

  sockaddr_in bound_address{};
  socklen_t bound_length = sizeof(bound_address);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&bound_address), &bound_length) == 0) {
    bound_port_ = ntohs(bound_address.sin_port);
  } else {
    bound_port_ = config_.http_port;
  }
  if (!config_.state_path.empty()) {
    std::filesystem::create_directories(config_.state_path.parent_path());
    std::ofstream state(config_.state_path);
    state << nlohmann::json{
        {"service", "sekailink_seed_config_api"},
        {"ok", true},
        {"listen_host", config_.listen_host},
        {"http_port", bound_port_},
        {"config", to_json(config_)},
    }.dump(2);
  }
  return true;
#endif
}

void SeedConfigHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
#endif
}

std::uint16_t SeedConfigHttpServer::port() const {
  return bound_port_;
}

void SeedConfigHttpServer::serve_one() {
#ifdef _WIN32
  throw std::runtime_error("seed_config_http_not_supported_on_windows_yet");
#else
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(listen_fd_, &read_fds);
  timeval timeout{};
  timeout.tv_sec = 1;
  const auto ready = ::select(listen_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
  if (ready <= 0) {
    return;
  }
  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (client_fd < 0) {
    throw std::runtime_error("seed_config_accept_failed");
  }

  std::string request;
  char buffer[4096];
  std::size_t content_length = 0;
  while (request.find("\r\n\r\n") == std::string::npos) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      ::close(client_fd);
      throw std::runtime_error("seed_config_recv_failed");
    }
    request.append(buffer, static_cast<std::size_t>(received));
    const auto content_length_pos = request.find("Content-Length:");
    if (content_length_pos != std::string::npos) {
      const auto line_end = request.find("\r\n", content_length_pos);
      const auto value = trim(request.substr(content_length_pos + 15, line_end - (content_length_pos + 15)));
      if (!value.empty()) {
        content_length = static_cast<std::size_t>(std::stoul(value));
      }
    }
  }

  const auto headers_end = request.find("\r\n\r\n");
  const auto body_start = headers_end + 4;
  while (request.size() < body_start + content_length) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    request.append(buffer, static_cast<std::size_t>(received));
  }

  std::istringstream request_stream(request.substr(0, headers_end));
  std::string method;
  std::string path;
  std::string version;
  request_stream >> method >> path >> version;

  std::optional<std::string> bearer_token;
  std::string header_line;
  std::getline(request_stream, header_line);
  while (std::getline(request_stream, header_line)) {
    if (!header_line.empty() && header_line.back() == '\r') {
      header_line.pop_back();
    }
    if (header_line.rfind("Authorization: Bearer ", 0) == 0) {
      bearer_token = header_line.substr(22);
    }
  }

  std::optional<nlohmann::json> body;
  if (content_length > 0 && request.size() >= body_start + content_length) {
    body = nlohmann::json::parse(request.substr(body_start, content_length));
  }
  const auto response_json = service_.handle(method, path, bearer_token, body);
  const auto status_code = response_json.value("status", response_json.value("ok", false) ? 200 : 500);
  const auto response = json_http_response(status_code, response_json);
  ::send(client_fd, response.data(), response.size(), 0);
  ::close(client_fd);
#endif
}

}  // namespace sekailink_server
