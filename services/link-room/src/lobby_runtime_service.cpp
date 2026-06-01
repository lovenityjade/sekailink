#include "sekailink_server/lobby_runtime_service.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  return value;
}

std::string required_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || !body.at(key).is_string()) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  const auto value = trim(body.at(key).get<std::string>());
  if (value.empty()) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  return value;
}

std::optional<std::string> optional_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_string()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  const auto value = trim(body.at(key).get<std::string>());
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

std::optional<nlohmann::json> optional_object(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_object()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return body.at(key);
}

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

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string url_decode(std::string_view value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char current = value[index];
    if (current == '+') {
      decoded.push_back(' ');
      continue;
    }
    if (current == '%' && index + 2 < value.size()) {
      const auto hex = std::string(value.substr(index + 1, 2));
      char* end = nullptr;
      const auto parsed = std::strtol(hex.c_str(), &end, 16);
      if (end != nullptr && *end == '\0') {
        decoded.push_back(static_cast<char>(parsed));
        index += 2;
        continue;
      }
    }
    decoded.push_back(current);
  }
  return decoded;
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
      const auto key = url_decode(segment.substr(0, equals));
      const auto value = equals == std::string_view::npos ? std::string() : url_decode(segment.substr(equals + 1));
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

std::optional<std::string> normalized_query_text(const nlohmann::json& query) {
  if (!query.contains("query") || !query.at("query").is_string()) {
    return std::nullopt;
  }
  auto value = trim(query.at("query").get<std::string>());
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::string> normalized_filter_text(const nlohmann::json& query, const char* key) {
  if (!query.contains(key) || !query.at(key).is_string()) {
    return std::nullopt;
  }
  auto value = lower(trim(query.at(key).get<std::string>()));
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::size_t> normalized_limit(const nlohmann::json& query, std::size_t max_value = 500) {
  if (!query.contains("limit") || !query.at("limit").is_string()) {
    return std::nullopt;
  }
  try {
    const auto parsed = std::stoll(trim(query.at("limit").get<std::string>()));
    if (parsed <= 0) {
      return std::nullopt;
    }
    return static_cast<std::size_t>(std::min<std::size_t>(static_cast<std::size_t>(parsed), max_value));
  } catch (...) {
    return std::nullopt;
  }
}

std::size_t normalized_offset(const nlohmann::json& query) {
  if (!query.contains("offset") || !query.at("offset").is_string()) {
    return 0;
  }
  try {
    const auto parsed = std::stoll(trim(query.at("offset").get<std::string>()));
    if (parsed <= 0) {
      return 0;
    }
    return static_cast<std::size_t>(parsed);
  } catch (...) {
    return 0;
  }
}

std::string utc_timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const auto now_time = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
#ifdef _WIN32
  gmtime_s(&utc_tm, &now_time);
#else
  gmtime_r(&now_time, &utc_tm);
#endif
  std::ostringstream stream;
  stream << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::string sqlite_error(sqlite3* db, const char* fallback) {
  const auto* message = sqlite3_errmsg(db);
  if (message == nullptr || *message == '\0') {
    return fallback;
  }
  return message;
}

std::optional<std::string> column_optional_text(sqlite3_stmt* stmt, int index) {
  if (sqlite3_column_type(stmt, index) == SQLITE_NULL) {
    return std::nullopt;
  }
  return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, index)));
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

}  // namespace

LobbyRuntimeServiceConfig load_lobby_runtime_service_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("lobby_runtime_config_open_failed");
  }
  nlohmann::json json;
  stream >> json;
  LobbyRuntimeServiceConfig config;
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("listen_host")) {
    config.listen_host = json.at("listen_host").get<std::string>();
  }
  if (json.contains("sqlite_path")) {
    config.sqlite_path = json.at("sqlite_path").get<std::string>();
  }
  if (json.contains("auth_token") && !json.at("auth_token").is_null()) {
    config.auth_token = json.at("auth_token").get<std::string>();
  }
  if (json.contains("state_path")) {
    config.state_path = json.at("state_path").get<std::string>();
  }
  return config;
}

nlohmann::json to_json(const LobbyRuntimeServiceConfig& config) {
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"sqlite_path", config.sqlite_path.string()},
      {"auth_token", config.auth_token.has_value() ? nlohmann::json("<redacted>") : nlohmann::json(nullptr)},
      {"state_path", config.state_path.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.state_path.string())},
  };
}

LobbyRuntimeStore::LobbyRuntimeStore(std::filesystem::path sqlite_path)
    : sqlite_path_(std::move(sqlite_path)) {
  open();
  init_schema();
}

LobbyRuntimeStore::~LobbyRuntimeStore() {
  close();
}

void LobbyRuntimeStore::open() {
  std::filesystem::create_directories(sqlite_path_.parent_path());
  if (sqlite3_open(sqlite_path_.string().c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("lobby_runtime_sqlite_open_failed");
  }
}

void LobbyRuntimeStore::close() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void LobbyRuntimeStore::exec(const std::string& sql) const {
  char* error = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error != nullptr ? error : "lobby_runtime_sql_exec_failed";
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void LobbyRuntimeStore::init_schema() {
  exec(
      "CREATE TABLE IF NOT EXISTS lobby_runtime ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "lobby_id TEXT NOT NULL UNIQUE,"
      "name TEXT NOT NULL,"
      "visibility TEXT NOT NULL DEFAULT 'private',"
      "status TEXT NOT NULL DEFAULT 'open',"
      "owner_username TEXT,"
      "description TEXT,"
      "metadata_json TEXT NOT NULL DEFAULT '{}',"
      "created_at TEXT NOT NULL,"
      "updated_at TEXT NOT NULL,"
      "closed_at TEXT,"
      "last_activity_at TEXT"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS lobby_presence ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "lobby_id TEXT NOT NULL,"
      "username TEXT NOT NULL,"
      "joined_at TEXT NOT NULL,"
      "updated_at TEXT NOT NULL,"
      "UNIQUE(lobby_id, username)"
      ");");
}

LobbyRuntimeStore::LobbyRuntimeRecord LobbyRuntimeStore::load_lobby_from_stmt(sqlite3_stmt* stmt) const {
  LobbyRuntimeRecord lobby{
      .id = sqlite3_column_int64(stmt, 0),
      .lobby_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
      .name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
      .visibility = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
      .status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
      .owner_username = column_optional_text(stmt, 5),
      .description = column_optional_text(stmt, 6),
      .metadata = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))),
      .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)),
      .updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)),
      .closed_at = column_optional_text(stmt, 10),
      .last_activity_at = column_optional_text(stmt, 11),
  };
  load_presence(lobby);
  return lobby;
}

void LobbyRuntimeStore::load_presence(LobbyRuntimeRecord& lobby) const {
  static constexpr const char* kSql =
      "SELECT username, joined_at, updated_at FROM lobby_presence WHERE lobby_id = ? ORDER BY username ASC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_presence_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, lobby.lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    lobby.presence.push_back({
        .username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
        .joined_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        .updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
    });
  }
  sqlite3_finalize(stmt);
}

void LobbyRuntimeStore::update_activity(const std::string& lobby_id) const {
  static constexpr const char* kSql =
      "UPDATE lobby_runtime SET updated_at = ?, last_activity_at = ? WHERE lobby_id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_activity_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_activity_update_failed"));
  }
  sqlite3_finalize(stmt);
}

LobbyRuntimeStore::LobbyRuntimeRecord LobbyRuntimeStore::open_lobby(
    const std::string& lobby_id,
    const std::string& name,
    const std::string& visibility,
    const std::optional<std::string>& owner_username,
    const std::optional<std::string>& description,
    const nlohmann::json& metadata) {
  static constexpr const char* kSql =
      "INSERT INTO lobby_runtime (lobby_id, name, visibility, status, owner_username, description, metadata_json, created_at, updated_at, last_activity_at) "
      "VALUES (?, ?, ?, 'open', ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_open_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  const auto metadata_json = metadata.dump();
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, visibility.c_str(), -1, SQLITE_TRANSIENT);
  if (owner_username.has_value()) sqlite3_bind_text(stmt, 4, owner_username->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
  if (description.has_value()) sqlite3_bind_text(stmt, 5, description->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 5);
  sqlite3_bind_text(stmt, 6, metadata_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    const auto message = sqlite_error(db_, "lobby_runtime_open_failed");
    sqlite3_finalize(stmt);
    if (message.find("UNIQUE constraint failed") != std::string::npos) {
      throw std::runtime_error("lobby_conflict");
    }
    throw std::runtime_error(message);
  }
  sqlite3_finalize(stmt);
  return *find_lobby(lobby_id);
}

std::optional<LobbyRuntimeStore::LobbyRuntimeRecord> LobbyRuntimeStore::find_lobby(const std::string& lobby_id) const {
  static constexpr const char* kSql =
      "SELECT id, lobby_id, name, visibility, status, owner_username, description, metadata_json, created_at, updated_at, closed_at, last_activity_at "
      "FROM lobby_runtime WHERE lobby_id = ? LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_lookup_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    auto lobby = load_lobby_from_stmt(stmt);
    sqlite3_finalize(stmt);
    return lobby;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

std::optional<LobbyRuntimeStore::LobbyRuntimeRecord> LobbyRuntimeStore::update_lobby(
    const std::string& lobby_id,
    const std::optional<std::string>& name,
    const std::optional<std::string>& visibility,
    const std::optional<std::string>& owner_username,
    const std::optional<std::string>& description,
    const std::optional<nlohmann::json>& metadata,
    const std::optional<std::string>& status) {
  static constexpr const char* kSql =
      "UPDATE lobby_runtime SET "
      "name = COALESCE(?, name), "
      "visibility = COALESCE(?, visibility), "
      "owner_username = COALESCE(?, owner_username), "
      "description = COALESCE(?, description), "
      "metadata_json = COALESCE(?, metadata_json), "
      "status = COALESCE(?, status), "
      "updated_at = ?, "
      "last_activity_at = ? "
      "WHERE lobby_id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_update_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  if (name.has_value()) sqlite3_bind_text(stmt, 1, name->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 1);
  if (visibility.has_value()) sqlite3_bind_text(stmt, 2, visibility->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 2);
  if (owner_username.has_value()) sqlite3_bind_text(stmt, 3, owner_username->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 3);
  if (description.has_value()) sqlite3_bind_text(stmt, 4, description->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
  if (metadata.has_value()) {
    const auto json = metadata->dump();
    sqlite3_bind_text(stmt, 5, json.c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 5);
  }
  if (status.has_value()) sqlite3_bind_text(stmt, 6, status->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 6);
  sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_update_failed"));
  }
  sqlite3_finalize(stmt);
  return find_lobby(lobby_id);
}

std::vector<LobbyRuntimeStore::LobbyRuntimeRecord> LobbyRuntimeStore::list_open_lobbies(
    std::optional<std::size_t> limit,
    std::size_t offset,
    const std::optional<std::string>& query,
    const std::optional<std::string>& visibility,
    const std::optional<std::string>& status) const {
  std::string sql =
      "SELECT id, lobby_id, name, visibility, status, owner_username, description, metadata_json, created_at, updated_at, closed_at, last_activity_at "
      "FROM lobby_runtime";
  std::vector<std::string> conditions;
  if (status.has_value()) {
    conditions.push_back("lower(status) = ?");
  }
  if (visibility.has_value()) {
    conditions.push_back("lower(visibility) = ?");
  }
  if (query.has_value()) {
    conditions.push_back("(lower(lobby_id) LIKE ? OR lower(name) LIKE ? OR lower(coalesce(owner_username, '')) LIKE ?)");
  }
  if (!conditions.empty()) {
    sql += " WHERE ";
    for (std::size_t index = 0; index < conditions.size(); ++index) {
      if (index > 0) {
        sql += " AND ";
      }
      sql += conditions[index];
    }
  }
  sql += " ORDER BY created_at DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_list_prepare_failed"));
  }
  int bind_index = 1;
  if (status.has_value()) {
    sqlite3_bind_text(stmt, bind_index++, status->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (visibility.has_value()) {
    sqlite3_bind_text(stmt, bind_index++, visibility->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (query.has_value()) {
    const auto pattern = "%" + lower(trim(*query)) + "%";
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(stmt, bind_index++, static_cast<int>(limit.value_or(200)));
  sqlite3_bind_int64(stmt, bind_index, static_cast<sqlite3_int64>(offset));
  std::vector<LobbyRuntimeRecord> lobbies;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    lobbies.push_back(load_lobby_from_stmt(stmt));
  }
  sqlite3_finalize(stmt);
  return lobbies;
}

std::optional<LobbyRuntimeStore::LobbyRuntimeRecord> LobbyRuntimeStore::join_lobby(
    const std::string& lobby_id,
    const std::string& username) {
  if (!find_lobby(lobby_id).has_value()) {
    return std::nullopt;
  }
  static constexpr const char* kSql =
      "INSERT INTO lobby_presence (lobby_id, username, joined_at, updated_at) VALUES (?, ?, ?, ?) "
      "ON CONFLICT(lobby_id, username) DO UPDATE SET updated_at = excluded.updated_at;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_join_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_join_failed"));
  }
  sqlite3_finalize(stmt);
  update_activity(lobby_id);
  return find_lobby(lobby_id);
}

std::optional<LobbyRuntimeStore::LobbyRuntimeRecord> LobbyRuntimeStore::leave_lobby(
    const std::string& lobby_id,
    const std::string& username) {
  if (!find_lobby(lobby_id).has_value()) {
    return std::nullopt;
  }
  static constexpr const char* kSql =
      "DELETE FROM lobby_presence WHERE lobby_id = ? AND username = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_leave_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_leave_failed"));
  }
  sqlite3_finalize(stmt);
  update_activity(lobby_id);
  return find_lobby(lobby_id);
}

std::optional<LobbyRuntimeStore::LobbyRuntimeRecord> LobbyRuntimeStore::close_lobby(const std::string& lobby_id) {
  static constexpr const char* kCloseSql =
      "UPDATE lobby_runtime SET status = 'closed', closed_at = COALESCE(closed_at, ?), updated_at = ?, last_activity_at = ? WHERE lobby_id = ?;";
  sqlite3_stmt* close_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kCloseSql, -1, &close_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_close_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(close_stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(close_stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(close_stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(close_stmt, 4, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(close_stmt) != SQLITE_DONE) {
    sqlite3_finalize(close_stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_close_failed"));
  }
  sqlite3_finalize(close_stmt);
  static constexpr const char* kPresenceSql = "DELETE FROM lobby_presence WHERE lobby_id = ?;";
  sqlite3_stmt* presence_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kPresenceSql, -1, &presence_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_close_presence_prepare_failed"));
  }
  sqlite3_bind_text(presence_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(presence_stmt) != SQLITE_DONE) {
    sqlite3_finalize(presence_stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_runtime_close_presence_failed"));
  }
  sqlite3_finalize(presence_stmt);
  return find_lobby(lobby_id);
}

LobbyRuntimeService::LobbyRuntimeService(LobbyRuntimeServiceConfig config)
    : config_(std::move(config)), store_(config_.sqlite_path) {
  write_state_file();
}

bool LobbyRuntimeService::authorized(const std::optional<std::string>& bearer_token) const {
  if (!config_.auth_token.has_value()) {
    return true;
  }
  return bearer_token.has_value() && *bearer_token == *config_.auth_token;
}

nlohmann::json LobbyRuntimeService::handle(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token,
    const std::optional<nlohmann::json>& body) const {
  record_request();
  try {
    if (path == "/health") {
      return {{"ok", true}, {"service", "sekailink_lobby_runtime_service"}};
    }
    if (!authorized(bearer_token)) {
      return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
    }
    const auto [normalized_path, query] = split_path_and_query(path);
    const auto parts = split_path(normalized_path);
    if (method == "GET" && parts.size() == 3 && parts[0] == "runtime" && parts[1] == "lobbies" && parts[2] == "open") {
      return handle_list_lobbies(
          normalized_query_text(query),
          normalized_filter_text(query, "visibility"),
          normalized_filter_text(query, "status").value_or(std::string("open")),
          normalized_limit(query),
          normalized_offset(query));
    }
    if (method == "POST" && parts.size() == 4 && parts[0] == "admin" && parts[1] == "runtime" && parts[2] == "lobbies" &&
        parts[3] == "open") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_open_lobby(*body);
    }
    if (method == "PATCH" && parts.size() == 4 && parts[0] == "admin" && parts[1] == "runtime" && parts[2] == "lobbies") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_edit_lobby(parts[3], *body);
    }
    if (method == "GET" && parts.size() == 3 && parts[0] == "runtime" && parts[1] == "lobbies") {
      return handle_lobby_info(parts[2]);
    }
    if (method == "POST" && parts.size() == 4 && parts[0] == "runtime" && parts[1] == "lobbies" && parts[3] == "close") {
      return handle_close(parts[2]);
    }
    if (parts.size() == 5 && parts[0] == "runtime" && parts[1] == "lobbies" && parts[3] == "presence") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      if (method == "POST" && parts[4] == "join") {
        return handle_join(parts[2], *body);
      }
      if (method == "POST" && parts[4] == "leave") {
        return handle_leave(parts[2], *body);
      }
    }
    return {{"ok", false}, {"status", 404}, {"error", "not_found"}};
  } catch (const std::exception& exception) {
    record_error();
    return {{"ok", false}, {"status", 500}, {"error", exception.what()}};
  }
}

nlohmann::json LobbyRuntimeService::handle_open_lobby(const nlohmann::json& body) const {
  try {
    const auto lobby = store_.open_lobby(
        required_string(body, "lobby_id"),
        required_string(body, "name"),
        body.value("visibility", "private"),
        optional_string(body, "owner_username"),
        optional_string(body, "description"),
        optional_object(body, "metadata").value_or(nlohmann::json::object()));
    return {{"ok", true}, {"lobby", lobby_to_json(lobby)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()) == "lobby_conflict") {
      return {{"ok", false}, {"status", 409}, {"error", "lobby_conflict"}};
    }
    if (std::string(exception.what()).rfind("missing_", 0) == 0 || std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json LobbyRuntimeService::handle_edit_lobby(const std::string& lobby_id, const nlohmann::json& body) const {
  try {
    const auto lobby = store_.update_lobby(
        lobby_id,
        optional_string(body, "name"),
        optional_string(body, "visibility"),
        optional_string(body, "owner_username"),
        optional_string(body, "description"),
        optional_object(body, "metadata"),
        optional_string(body, "status"));
    if (!lobby.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
    }
    return {{"ok", true}, {"lobby", lobby_to_json(*lobby)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json LobbyRuntimeService::handle_list_lobbies(
    const std::optional<std::string>& query,
    const std::optional<std::string>& visibility,
    const std::optional<std::string>& status,
    std::optional<std::size_t> limit,
    std::size_t offset) const {
  nlohmann::json lobbies = nlohmann::json::array();
  for (const auto& lobby : store_.list_open_lobbies(limit, offset, query, visibility, status)) {
    lobbies.push_back(lobby_to_json(lobby));
  }
  return {
      {"ok", true},
      {"limit", limit.has_value() ? nlohmann::json(*limit) : nlohmann::json(nullptr)},
      {"offset", offset},
      {"query", query.has_value() ? nlohmann::json(*query) : nlohmann::json(nullptr)},
      {"visibility_filter", visibility.has_value() ? nlohmann::json(*visibility) : nlohmann::json(nullptr)},
      {"status_filter", status.has_value() ? nlohmann::json(*status) : nlohmann::json(nullptr)},
      {"lobbies", std::move(lobbies)},
  };
}

nlohmann::json LobbyRuntimeService::handle_lobby_info(const std::string& lobby_id) const {
  const auto lobby = store_.find_lobby(lobby_id);
  if (!lobby.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
  }
  return {{"ok", true}, {"lobby", lobby_to_json(*lobby)}};
}

nlohmann::json LobbyRuntimeService::handle_join(const std::string& lobby_id, const nlohmann::json& body) const {
  try {
    const auto lobby = store_.join_lobby(lobby_id, required_string(body, "username"));
    if (!lobby.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
    }
    return {{"ok", true}, {"lobby", lobby_to_json(*lobby)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json LobbyRuntimeService::handle_leave(const std::string& lobby_id, const nlohmann::json& body) const {
  try {
    const auto lobby = store_.leave_lobby(lobby_id, required_string(body, "username"));
    if (!lobby.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
    }
    return {{"ok", true}, {"lobby", lobby_to_json(*lobby)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json LobbyRuntimeService::handle_close(const std::string& lobby_id) const {
  const auto lobby = store_.close_lobby(lobby_id);
  if (!lobby.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
  }
  return {{"ok", true}, {"closed", true}, {"lobby", lobby_to_json(*lobby)}};
}

nlohmann::json LobbyRuntimeService::lobby_to_json(const LobbyRuntimeStore::LobbyRuntimeRecord& lobby) {
  nlohmann::json presence = nlohmann::json::array();
  for (const auto& item : lobby.presence) {
    presence.push_back({
        {"username", item.username},
        {"joined_at", item.joined_at},
        {"updated_at", item.updated_at},
    });
  }
  return {
      {"id", lobby.id},
      {"lobby_id", lobby.lobby_id},
      {"name", lobby.name},
      {"visibility", lobby.visibility},
      {"status", lobby.status},
      {"owner_username", lobby.owner_username.has_value() ? nlohmann::json(*lobby.owner_username) : nlohmann::json(nullptr)},
      {"description", lobby.description.has_value() ? nlohmann::json(*lobby.description) : nlohmann::json(nullptr)},
      {"metadata", lobby.metadata},
      {"created_at", lobby.created_at},
      {"updated_at", lobby.updated_at},
      {"closed_at", lobby.closed_at.has_value() ? nlohmann::json(*lobby.closed_at) : nlohmann::json(nullptr)},
      {"last_activity_at", lobby.last_activity_at.has_value() ? nlohmann::json(*lobby.last_activity_at) : nlohmann::json(nullptr)},
      {"presence_count", lobby.presence.size()},
      {"presence", std::move(presence)},
  };
}

void LobbyRuntimeService::record_request() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_requests_;
  }
  write_state_file();
}

void LobbyRuntimeService::record_error() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_errors_;
  }
  write_state_file();
}

void LobbyRuntimeService::write_state_file() const {
  if (config_.state_path.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(state_mutex_);
  std::filesystem::create_directories(config_.state_path.parent_path());
  const auto open_lobbies = store_.list_open_lobbies();
  nlohmann::json state = {
      {"ok", true},
      {"service", "sekailink_lobby_runtime_service"},
      {"listen_host", config_.listen_host},
      {"http_port", config_.http_port},
      {"sqlite_path", config_.sqlite_path.string()},
      {"state_path", config_.state_path.string()},
      {"open_lobby_count", open_lobbies.size()},
      {"total_requests", total_requests_},
      {"total_errors", total_errors_},
      {"updated_at", utc_timestamp_now()},
  };
  std::ofstream stream(config_.state_path);
  stream << state.dump(2) << "\n";
}

LobbyRuntimeHttpServer::LobbyRuntimeHttpServer(LobbyRuntimeServiceConfig config)
    : service_(config), config_(std::move(config)) {}

LobbyRuntimeHttpServer::~LobbyRuntimeHttpServer() {
  stop();
}

bool LobbyRuntimeHttpServer::start() {
#ifdef _WIN32
  throw std::runtime_error("lobby_runtime_http_server_not_supported_on_windows_yet");
#else
  if (listen_fd_ >= 0) {
    return true;
  }
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return false;
  }
  int opt = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.http_port);
  if (config_.listen_host == "0.0.0.0") {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else if (::inet_pton(AF_INET, config_.listen_host.c_str(), &addr.sin_addr) != 1) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (::listen(listen_fd_, 16) != 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  bound_port_ = config_.http_port;
  return true;
#endif
}

void LobbyRuntimeHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
    bound_port_ = 0;
  }
#endif
}

std::uint16_t LobbyRuntimeHttpServer::port() const {
  return bound_port_;
}

void LobbyRuntimeHttpServer::serve_one() const {
#ifdef _WIN32
  throw std::runtime_error("lobby_runtime_http_server_not_supported_on_windows_yet");
#else
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(listen_fd_, &readfds);
  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 500000;
  const auto ready = ::select(listen_fd_ + 1, &readfds, nullptr, nullptr, &timeout);
  if (ready <= 0) {
    return;
  }
  sockaddr_in client_addr{};
  socket_len_t client_length = sizeof(client_addr);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_length);
  if (client_fd < 0) {
    throw std::runtime_error("lobby_runtime_accept_failed");
  }
  timeval recv_timeout{};
  recv_timeout.tv_sec = 2;
  recv_timeout.tv_usec = 0;
  ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
  std::string request;
  char buffer[4096];
  while (request.find("\r\n\r\n") == std::string::npos) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    request.append(buffer, static_cast<std::size_t>(received));
    if (request.size() > 1024 * 1024) {
      break;
    }
  }

  std::istringstream stream(request);
  std::string request_line;
  std::getline(stream, request_line);
  if (!request_line.empty() && request_line.back() == '\r') {
    request_line.pop_back();
  }
  std::istringstream request_line_stream(request_line);
  std::string method;
  std::string path;
  request_line_stream >> method >> path;

  std::optional<std::string> bearer_token;
  std::size_t content_length = 0;
  std::string header;
  while (std::getline(stream, header)) {
    if (!header.empty() && header.back() == '\r') {
      header.pop_back();
    }
    if (header.empty()) {
      break;
    }
    const auto separator = header.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    auto name = header.substr(0, separator);
    auto value = trim(header.substr(separator + 1));
    for (auto& c : name) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (name == "authorization" && value.rfind("Bearer ", 0) == 0) {
      bearer_token = value.substr(7);
    } else if (name == "content-length") {
      content_length = static_cast<std::size_t>(std::stoul(value));
    }
  }
  std::string body;
  if (content_length > 0) {
    body.resize(content_length);
    stream.read(body.data(), static_cast<std::streamsize>(content_length));
    auto already_read = static_cast<std::size_t>(stream.gcount());
    while (already_read < content_length) {
      const auto received = ::recv(client_fd, body.data() + already_read, content_length - already_read, 0);
      if (received <= 0) {
        break;
      }
      already_read += static_cast<std::size_t>(received);
    }
    body.resize(already_read);
  }

  std::optional<nlohmann::json> json_body;
  if (!body.empty()) {
    json_body = nlohmann::json::parse(body);
  }
  const auto response = service_.handle(method, path, bearer_token, json_body);
  const auto status_code = response.value("status", response.value("ok", false) ? 200 : 500);
  const auto http_response = json_http_response(status_code, response);
  ::send(client_fd, http_response.data(), http_response.size(), 0);
  ::close(client_fd);
#endif
}

}  // namespace sekailink_server
