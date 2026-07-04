#include "sekailink_server/lobby_admin_service.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

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

int normalized_limit(const nlohmann::json& query, int fallback = 100, int max_value = 500) {
  if (!query.contains("limit") || !query.at("limit").is_string()) {
    return fallback;
  }
  try {
    const auto parsed = std::stoi(trim(query.at("limit").get<std::string>()));
    if (parsed <= 0) {
      return fallback;
    }
    return std::min(parsed, max_value);
  } catch (...) {
    return fallback;
  }
}

int normalized_offset(const nlohmann::json& query) {
  if (!query.contains("offset") || !query.at("offset").is_string()) {
    return 0;
  }
  try {
    const auto parsed = std::stoi(trim(query.at("offset").get<std::string>()));
    if (parsed <= 0) {
      return 0;
    }
    return parsed;
  } catch (...) {
    return 0;
  }
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

std::string required_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || !body.at(key).is_string()) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  return trim(body.at(key).get<std::string>());
}

std::optional<std::string> optional_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_string()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return trim(body.at(key).get<std::string>());
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

nlohmann::json request_context_to_json(const LobbyAdminRequestContext& context) {
  nlohmann::json json = nlohmann::json::object();
  if (context.user_agent.has_value()) {
    json["user_agent"] = *context.user_agent;
  }
  if (context.client_name.has_value()) {
    json["client_name"] = *context.client_name;
  }
  if (context.client_version.has_value()) {
    json["client_version"] = *context.client_version;
  }
  if (context.device_id.has_value()) {
    json["device_id"] = *context.device_id;
  }
  if (context.requested_locale.has_value()) {
    json["requested_locale"] = *context.requested_locale;
  }
  return json;
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

struct HttpBridgeResult {
  int status_code = 0;
  nlohmann::json body = nlohmann::json::object();
};

HttpBridgeResult parse_http_result(const std::string& response) {
  const auto line_end = response.find("\r\n");
  if (line_end == std::string::npos) {
    throw std::runtime_error("lobby_admin_bridge_invalid_http");
  }
  const auto status_line = response.substr(0, line_end);
  std::istringstream status_stream(status_line);
  std::string version;
  int status_code = 0;
  status_stream >> version >> status_code;
  const auto body_pos = response.find("\r\n\r\n");
  if (body_pos == std::string::npos) {
    throw std::runtime_error("lobby_admin_bridge_missing_body");
  }
  HttpBridgeResult result;
  result.status_code = status_code;
  const auto body = response.substr(body_pos + 4);
  result.body = body.empty() ? nlohmann::json::object() : nlohmann::json::parse(body);
  return result;
}

HttpBridgeResult http_bridge_request(
    const std::string& host,
    std::uint16_t port,
    const std::string& method,
    const std::string& path,
    const std::string& bearer_token,
    const std::optional<nlohmann::json>& body) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* addresses = nullptr;
  const auto port_string = std::to_string(port);
  if (::getaddrinfo(host.c_str(), port_string.c_str(), &hints, &addresses) != 0) {
    throw std::runtime_error("lobby_admin_bridge_resolve_failed");
  }
  int sock = -1;
  for (addrinfo* current = addresses; current != nullptr; current = current->ai_next) {
    sock = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (sock < 0) {
      continue;
    }
    if (::connect(sock, current->ai_addr, current->ai_addrlen) == 0) {
      break;
    }
    ::close(sock);
    sock = -1;
  }
  ::freeaddrinfo(addresses);
  if (sock < 0) {
    throw std::runtime_error("lobby_admin_bridge_connect_failed");
  }

  std::ostringstream request;
  request << method << " " << path << " HTTP/1.1\r\n";
  request << "Host: " << host << "\r\n";
  request << "Connection: close\r\n";
  if (!bearer_token.empty()) {
    request << "Authorization: Bearer " << bearer_token << "\r\n";
  }
  if (body.has_value()) {
    const auto payload = body->dump();
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << payload.size() << "\r\n\r\n";
    request << payload;
  } else {
    request << "Content-Length: 0\r\n\r\n";
  }

  const auto payload = request.str();
  if (::send(sock, payload.data(), payload.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("lobby_admin_bridge_send_failed");
  }

  std::string response;
  char buffer[4096];
  while (true) {
    const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(sock);
  return parse_http_result(response);
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

}  // namespace

LobbyAdminServiceConfig load_lobby_admin_service_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("lobby_admin_config_open_failed");
  }
  nlohmann::json json;
  stream >> json;
  LobbyAdminServiceConfig config;
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("listen_host")) {
    config.listen_host = json.at("listen_host").get<std::string>();
  }
  if (json.contains("sqlite_path")) {
    config.sqlite_path = json.at("sqlite_path").get<std::string>();
  }
  if (json.contains("admin_token")) {
    config.admin_token = json.at("admin_token").get<std::string>();
  }
  if (json.contains("state_path")) {
    config.state_path = json.at("state_path").get<std::string>();
  }
  if (json.contains("runtime_bridge") && json.at("runtime_bridge").is_object()) {
    const auto& bridge = json.at("runtime_bridge");
    config.runtime_bridge.enabled = bridge.value("enabled", true);
    config.runtime_bridge.host = bridge.value("host", "127.0.0.1");
    if (bridge.contains("port")) {
      config.runtime_bridge.port = bridge.at("port").get<std::uint16_t>();
    }
    if (bridge.contains("admin_token") && !bridge.at("admin_token").is_null()) {
      config.runtime_bridge.admin_token = bridge.at("admin_token").get<std::string>();
    }
  }
  return config;
}

nlohmann::json to_json(const LobbyAdminServiceConfig& config) {
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"sqlite_path", config.sqlite_path.string()},
      {"admin_token", config.admin_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"state_path", config.state_path.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.state_path.string())},
      {"runtime_bridge",
       {
           {"enabled", config.runtime_bridge.enabled},
           {"host", config.runtime_bridge.host},
           {"port", config.runtime_bridge.port},
           {"admin_token", config.runtime_bridge.admin_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
       }},
  };
}

LobbyAdminStore::LobbyAdminStore(std::filesystem::path sqlite_path)
    : sqlite_path_(std::move(sqlite_path)) {
  open();
  init_schema();
}

LobbyAdminStore::~LobbyAdminStore() {
  close();
}

void LobbyAdminStore::open() {
  std::filesystem::create_directories(sqlite_path_.parent_path());
  if (sqlite3_open(sqlite_path_.string().c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("lobby_admin_sqlite_open_failed");
  }
}

void LobbyAdminStore::close() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void LobbyAdminStore::exec(const std::string& sql) const {
  char* error = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error != nullptr ? error : "lobby_admin_sql_exec_failed";
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void LobbyAdminStore::init_schema() {
  exec(
      "CREATE TABLE IF NOT EXISTS lobbies ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "lobby_id TEXT NOT NULL UNIQUE,"
      "name TEXT NOT NULL,"
      "visibility TEXT NOT NULL DEFAULT 'private',"
      "status TEXT NOT NULL DEFAULT 'open',"
      "owner_username TEXT,"
      "description TEXT,"
      "rules_json TEXT NOT NULL DEFAULT '{}',"
      "metadata_json TEXT NOT NULL DEFAULT '{}',"
      "created_at TEXT NOT NULL,"
      "updated_at TEXT NOT NULL,"
      "closed_at TEXT"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS lobby_admin_audit ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "event_type TEXT NOT NULL,"
      "lobby_id TEXT NOT NULL,"
      "actor_type TEXT NOT NULL DEFAULT 'admin_token',"
      "request_context_json TEXT NOT NULL DEFAULT '{}',"
      "payload_json TEXT NOT NULL DEFAULT '{}',"
      "created_at TEXT NOT NULL"
      ");");
}

LobbyAdminStore::LobbyRecord LobbyAdminStore::create_lobby(
    const std::string& lobby_id,
    const std::string& name,
    const std::string& visibility,
    const std::optional<std::string>& owner_username,
    const std::optional<std::string>& description,
    const nlohmann::json& rules,
    const nlohmann::json& metadata) {
  static constexpr const char* kSql =
      "INSERT INTO lobbies (lobby_id, name, visibility, status, owner_username, description, rules_json, metadata_json, created_at, updated_at) "
      "VALUES (?, ?, ?, 'open', ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_insert_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  const auto rules_json = rules.dump();
  const auto metadata_json = metadata.dump();
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, visibility.c_str(), -1, SQLITE_TRANSIENT);
  if (owner_username.has_value()) sqlite3_bind_text(stmt, 4, owner_username->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
  if (description.has_value()) sqlite3_bind_text(stmt, 5, description->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 5);
  sqlite3_bind_text(stmt, 6, rules_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, metadata_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    const auto message = sqlite_error(db_, "lobby_admin_insert_failed");
    sqlite3_finalize(stmt);
    if (message.find("UNIQUE constraint failed") != std::string::npos) {
      throw std::runtime_error("lobby_conflict");
    }
    throw std::runtime_error(message);
  }
  sqlite3_finalize(stmt);
  return *find_lobby(lobby_id);
}

std::optional<LobbyAdminStore::LobbyRecord> LobbyAdminStore::find_lobby(const std::string& lobby_id) const {
  static constexpr const char* kSql =
      "SELECT id, lobby_id, name, visibility, status, owner_username, description, rules_json, metadata_json, created_at, updated_at, closed_at "
      "FROM lobbies WHERE lobby_id = ? LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_lookup_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    LobbyRecord lobby{
        .id = sqlite3_column_int64(stmt, 0),
        .lobby_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        .name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        .visibility = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
        .status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
        .owner_username = column_optional_text(stmt, 5),
        .description = column_optional_text(stmt, 6),
        .rules = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))),
        .metadata = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8))),
        .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)),
        .updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)),
        .closed_at = column_optional_text(stmt, 11),
    };
    sqlite3_finalize(stmt);
    return lobby;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

std::optional<LobbyAdminStore::LobbyRecord> LobbyAdminStore::update_lobby(
    const std::string& lobby_id,
    const std::optional<std::string>& name,
    const std::optional<std::string>& visibility,
    const std::optional<std::string>& owner_username,
    const std::optional<std::string>& description,
    const std::optional<nlohmann::json>& rules,
    const std::optional<nlohmann::json>& metadata,
    const std::optional<std::string>& status) {
  static constexpr const char* kSql =
      "UPDATE lobbies SET "
      "name = COALESCE(?, name), "
      "visibility = COALESCE(?, visibility), "
      "owner_username = COALESCE(?, owner_username), "
      "description = COALESCE(?, description), "
      "rules_json = COALESCE(?, rules_json), "
      "metadata_json = COALESCE(?, metadata_json), "
      "status = COALESCE(?, status), "
      "updated_at = ? "
      "WHERE lobby_id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_update_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  if (name.has_value()) sqlite3_bind_text(stmt, 1, name->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 1);
  if (visibility.has_value()) sqlite3_bind_text(stmt, 2, visibility->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 2);
  if (owner_username.has_value()) sqlite3_bind_text(stmt, 3, owner_username->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 3);
  if (description.has_value()) sqlite3_bind_text(stmt, 4, description->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
  if (rules.has_value()) {
    const auto json = rules->dump();
    sqlite3_bind_text(stmt, 5, json.c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 5);
  }
  if (metadata.has_value()) {
    const auto json = metadata->dump();
    sqlite3_bind_text(stmt, 6, json.c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 6);
  }
  if (status.has_value()) sqlite3_bind_text(stmt, 7, status->c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 7);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_update_failed"));
  }
  sqlite3_finalize(stmt);
  return find_lobby(lobby_id);
}

std::optional<LobbyAdminStore::LobbyRecord> LobbyAdminStore::close_lobby(const std::string& lobby_id) {
  static constexpr const char* kSql =
      "UPDATE lobbies SET status = 'closed', closed_at = COALESCE(closed_at, ?), updated_at = ? WHERE lobby_id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_close_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_close_failed"));
  }
  sqlite3_finalize(stmt);
  return find_lobby(lobby_id);
}

std::vector<LobbyAdminStore::LobbyRecord> LobbyAdminStore::list_lobbies(
    int limit,
    const std::optional<std::string>& query,
    const std::optional<std::string>& visibility,
    const std::optional<std::string>& status,
    int offset) const {
  std::string sql =
      "SELECT id, lobby_id, name, visibility, status, owner_username, description, rules_json, metadata_json, created_at, updated_at, closed_at "
      "FROM lobbies";
  std::vector<std::string> conditions;
  if (query.has_value()) {
    conditions.push_back("(lower(lobby_id) LIKE ? OR lower(name) LIKE ? OR lower(coalesce(owner_username, '')) LIKE ?)");
  }
  if (visibility.has_value()) {
    conditions.push_back("lower(visibility) = ?");
  }
  if (status.has_value()) {
    conditions.push_back("lower(status) = ?");
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
  sql += " ORDER BY id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_list_prepare_failed"));
  }
  int bind_index = 1;
  if (query.has_value()) {
    const auto pattern = "%" + lower(trim(*query)) + "%";
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (visibility.has_value()) {
    sqlite3_bind_text(stmt, bind_index++, visibility->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (status.has_value()) {
    sqlite3_bind_text(stmt, bind_index++, status->c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(stmt, bind_index++, limit > 0 ? limit : 100);
  sqlite3_bind_int(stmt, bind_index, offset > 0 ? offset : 0);
  std::vector<LobbyRecord> lobbies;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    lobbies.push_back(LobbyRecord{
        .id = sqlite3_column_int64(stmt, 0),
        .lobby_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        .name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        .visibility = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
        .status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
        .owner_username = column_optional_text(stmt, 5),
        .description = column_optional_text(stmt, 6),
        .rules = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))),
        .metadata = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8))),
        .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)),
        .updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)),
        .closed_at = column_optional_text(stmt, 11),
    });
  }
  sqlite3_finalize(stmt);
  std::reverse(lobbies.begin(), lobbies.end());
  return lobbies;
}

void LobbyAdminStore::record_audit(
    const std::string& event_type,
    const std::string& lobby_id,
    const nlohmann::json& request_context,
    const nlohmann::json& payload) {
  static constexpr const char* kSql =
      "INSERT INTO lobby_admin_audit (event_type, lobby_id, actor_type, request_context_json, payload_json, created_at) "
      "VALUES (?, ?, 'admin_token', ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_audit_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  const auto request_context_json = request_context.dump();
  const auto payload_json = payload.dump();
  sqlite3_bind_text(stmt, 1, event_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, request_context_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, payload_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    const auto message = sqlite_error(db_, "lobby_admin_audit_insert_failed");
    sqlite3_finalize(stmt);
    throw std::runtime_error(message);
  }
  sqlite3_finalize(stmt);
}

std::vector<LobbyAdminStore::AuditRecord> LobbyAdminStore::read_audit(const std::string& lobby_id, int limit) const {
  static constexpr const char* kSql =
      "SELECT id, event_type, lobby_id, actor_type, request_context_json, payload_json, created_at "
      "FROM lobby_admin_audit WHERE lobby_id = ? ORDER BY id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "lobby_admin_audit_lookup_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit > 0 ? limit : 20);
  std::vector<AuditRecord> records;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    records.push_back(AuditRecord{
        .id = sqlite3_column_int64(stmt, 0),
        .event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        .lobby_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        .actor_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
        .request_context = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))),
        .payload = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))),
        .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)),
    });
  }
  sqlite3_finalize(stmt);
  std::reverse(records.begin(), records.end());
  return records;
}

LobbyAdminService::LobbyAdminService(LobbyAdminServiceConfig config)
    : config_(std::move(config)), store_(config_.sqlite_path) {
  write_state_file();
}

bool LobbyAdminService::authorized(const std::optional<std::string>& bearer_token) const {
  return bearer_token.has_value() && !config_.admin_token.empty() && *bearer_token == config_.admin_token;
}

nlohmann::json LobbyAdminService::handle(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token,
    const std::optional<nlohmann::json>& body,
    const LobbyAdminRequestContext& context) const {
  record_request();
  try {
    if (path == "/health") {
      return {{"ok", true}, {"service", "sekailink_lobby_admin_service"}};
    }
    const auto [normalized_path, query] = split_path_and_query(path);
    const auto parts = split_path(normalized_path);
    if (parts.size() == 2 && parts[0] == "admin" && parts[1] == "lobbies" && method == "POST") {
      if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      return handle_add_lobby(bearer_token, *body, context);
    }
    if (parts.size() == 2 && parts[0] == "admin" && parts[1] == "lobbies" && method == "GET") {
      return handle_list_lobbies(
          bearer_token,
          context,
          normalized_query_text(query),
          normalized_filter_text(query, "visibility"),
          normalized_filter_text(query, "status"),
          normalized_limit(query),
          normalized_offset(query));
    }
    if (parts.size() == 3 && parts[0] == "admin" && parts[1] == "lobbies") {
      if (method == "GET") return handle_lobby_info(bearer_token, parts[2], context);
      if (method == "PATCH") {
        if (!body.has_value()) return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
        return handle_edit_lobby(bearer_token, parts[2], *body, context);
      }
    }
    if (parts.size() == 4 && parts[0] == "admin" && parts[1] == "lobbies" && parts[3] == "close" && method == "POST") {
      return handle_close_lobby(bearer_token, parts[2], context);
    }
    return {{"ok", false}, {"status", 404}, {"error", "not_found"}};
  } catch (const std::exception& exception) {
    record_error();
    return {{"ok", false}, {"status", 500}, {"error", exception.what()}};
  }
}

nlohmann::json LobbyAdminService::handle_add_lobby(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body,
    const LobbyAdminRequestContext& context) const {
  if (!authorized(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  try {
    const auto lobby = store_.create_lobby(
        required_string(body, "lobby_id"),
        required_string(body, "name"),
        body.value("visibility", "private"),
        optional_string(body, "owner_username"),
        optional_string(body, "description"),
        optional_object(body, "rules").value_or(nlohmann::json::object()),
        optional_object(body, "metadata").value_or(nlohmann::json::object()));
    store_.record_audit(
        "admin_lobby_created",
        lobby.lobby_id,
        request_context_to_json(context),
        {
            {"admin_context",
             {
                 {"actor_type", "admin_token"},
                 {"request_context", request_context_to_json(context)},
             }},
            {"lobby", lobby_to_json(lobby)},
        });
    sync_runtime_open(lobby);
    return {{"ok", true}, {"lobby", lobby_to_json(lobby)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()) == "lobby_conflict") {
      return {{"ok", false}, {"status", 409}, {"error", "lobby_conflict"}};
    }
    if (std::string(exception.what()).rfind("missing_", 0) == 0 ||
        std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json LobbyAdminService::handle_list_lobbies(
    const std::optional<std::string>& bearer_token,
    const LobbyAdminRequestContext&,
    const std::optional<std::string>& query,
    const std::optional<std::string>& visibility,
    const std::optional<std::string>& status,
    int limit,
    int offset) const {
  if (!authorized(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  nlohmann::json lobbies = nlohmann::json::array();
  for (const auto& lobby : store_.list_lobbies(limit, query, visibility, status, offset)) {
    lobbies.push_back(lobby_to_json(lobby));
  }
  return {
      {"ok", true},
      {"limit", limit},
      {"offset", offset},
      {"query", query.has_value() ? nlohmann::json(*query) : nlohmann::json(nullptr)},
      {"visibility_filter", visibility.has_value() ? nlohmann::json(*visibility) : nlohmann::json(nullptr)},
      {"status_filter", status.has_value() ? nlohmann::json(*status) : nlohmann::json(nullptr)},
      {"lobbies", std::move(lobbies)},
  };
}

nlohmann::json LobbyAdminService::handle_lobby_info(
    const std::optional<std::string>& bearer_token,
    const std::string& lobby_id,
    const LobbyAdminRequestContext& context) const {
  if (!authorized(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  const auto lobby = store_.find_lobby(lobby_id);
  if (!lobby.has_value()) return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
  store_.record_audit(
      "admin_lobby_info_viewed",
      lobby_id,
      request_context_to_json(context),
      {
          {"admin_context",
           {
               {"actor_type", "admin_token"},
               {"request_context", request_context_to_json(context)},
           }},
      });
  nlohmann::json response = {{"ok", true}, {"lobby", lobby_to_json(*lobby)}};
  try {
    const auto runtime = fetch_runtime_info(lobby_id);
    if (!runtime.is_null()) {
      response["runtime"] = runtime;
    }
  } catch (const std::exception& exception) {
    response["runtime_error"] = exception.what();
  }
  nlohmann::json audit_events = nlohmann::json::array();
  for (const auto& audit : store_.read_audit(lobby_id)) {
    audit_events.push_back({
        {"id", audit.id},
        {"event_type", audit.event_type},
        {"lobby_id", audit.lobby_id},
        {"actor_type", audit.actor_type},
        {"request_context", audit.request_context},
        {"payload", audit.payload},
        {"created_at", audit.created_at},
    });
  }
  response["audit_events"] = std::move(audit_events);
  return response;
}

nlohmann::json LobbyAdminService::handle_edit_lobby(
    const std::optional<std::string>& bearer_token,
    const std::string& lobby_id,
    const nlohmann::json& body,
    const LobbyAdminRequestContext& context) const {
  if (!authorized(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  try {
    const auto lobby = store_.update_lobby(
        lobby_id,
        optional_string(body, "name"),
        optional_string(body, "visibility"),
        optional_string(body, "owner_username"),
        optional_string(body, "description"),
        optional_object(body, "rules"),
        optional_object(body, "metadata"),
        optional_string(body, "status"));
    if (!lobby.has_value()) return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
    store_.record_audit(
        "admin_lobby_updated",
        lobby_id,
        request_context_to_json(context),
        {
            {"admin_context",
             {
                 {"actor_type", "admin_token"},
                 {"request_context", request_context_to_json(context)},
             }},
            {"lobby", lobby_to_json(*lobby)},
        });
    if (lobby->status == "closed") {
      sync_runtime_close(lobby_id);
    } else {
      sync_runtime_edit(*lobby);
    }
    return {{"ok", true}, {"lobby", lobby_to_json(*lobby)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json LobbyAdminService::handle_close_lobby(
    const std::optional<std::string>& bearer_token,
    const std::string& lobby_id,
    const LobbyAdminRequestContext& context) const {
  if (!authorized(bearer_token)) return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  const auto lobby = store_.close_lobby(lobby_id);
  if (!lobby.has_value()) return {{"ok", false}, {"status", 404}, {"error", "lobby_not_found"}};
  store_.record_audit(
      "admin_lobby_closed",
      lobby_id,
      request_context_to_json(context),
      {
          {"admin_context",
           {
               {"actor_type", "admin_token"},
               {"request_context", request_context_to_json(context)},
           }},
          {"lobby", lobby_to_json(*lobby)},
      });
  sync_runtime_close(lobby_id);
  return {{"ok", true}, {"closed", true}, {"lobby", lobby_to_json(*lobby)}};
}

void LobbyAdminService::sync_runtime_open(const LobbyAdminStore::LobbyRecord& lobby) const {
  if (!config_.runtime_bridge.enabled) {
    return;
  }
  const auto result = http_bridge_request(
      config_.runtime_bridge.host,
      config_.runtime_bridge.port,
      "POST",
      "/admin/runtime/lobbies/open",
      config_.runtime_bridge.admin_token,
      nlohmann::json{
          {"lobby_id", lobby.lobby_id},
          {"name", lobby.name},
          {"visibility", lobby.visibility},
          {"owner_username", lobby.owner_username.has_value() ? nlohmann::json(*lobby.owner_username) : nlohmann::json(nullptr)},
          {"description", lobby.description.has_value() ? nlohmann::json(*lobby.description) : nlohmann::json(nullptr)},
          {"metadata", lobby.metadata},
      });
  if (result.status_code != 200) {
    throw std::runtime_error("lobby_runtime_bridge_open_failed");
  }
}

void LobbyAdminService::sync_runtime_edit(const LobbyAdminStore::LobbyRecord& lobby) const {
  if (!config_.runtime_bridge.enabled) {
    return;
  }
  const auto result = http_bridge_request(
      config_.runtime_bridge.host,
      config_.runtime_bridge.port,
      "PATCH",
      "/admin/runtime/lobbies/" + lobby.lobby_id,
      config_.runtime_bridge.admin_token,
      nlohmann::json{
          {"name", lobby.name},
          {"visibility", lobby.visibility},
          {"owner_username", lobby.owner_username.has_value() ? nlohmann::json(*lobby.owner_username) : nlohmann::json(nullptr)},
          {"description", lobby.description.has_value() ? nlohmann::json(*lobby.description) : nlohmann::json(nullptr)},
          {"metadata", lobby.metadata},
          {"status", lobby.status},
      });
  if (result.status_code != 200) {
    throw std::runtime_error("lobby_runtime_bridge_edit_failed");
  }
}

void LobbyAdminService::sync_runtime_close(const std::string& lobby_id) const {
  if (!config_.runtime_bridge.enabled) {
    return;
  }
  const auto result = http_bridge_request(
      config_.runtime_bridge.host,
      config_.runtime_bridge.port,
      "POST",
      "/runtime/lobbies/" + lobby_id + "/close",
      config_.runtime_bridge.admin_token,
      std::nullopt);
  if (result.status_code != 200 && result.status_code != 404) {
    throw std::runtime_error("lobby_runtime_bridge_close_failed");
  }
}

nlohmann::json LobbyAdminService::fetch_runtime_info(const std::string& lobby_id) const {
  if (!config_.runtime_bridge.enabled) {
    return nullptr;
  }
  const auto result = http_bridge_request(
      config_.runtime_bridge.host,
      config_.runtime_bridge.port,
      "GET",
      "/runtime/lobbies/" + lobby_id,
      config_.runtime_bridge.admin_token,
      std::nullopt);
  if (result.status_code == 404) {
    return nullptr;
  }
  if (result.status_code != 200) {
    throw std::runtime_error("lobby_runtime_bridge_info_failed");
  }
  return result.body;
}

void LobbyAdminService::record_request() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_requests_;
  }
  write_state_file();
}

void LobbyAdminService::record_error() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_errors_;
  }
  write_state_file();
}

void LobbyAdminService::write_state_file() const {
  if (config_.state_path.empty()) return;
  std::lock_guard<std::mutex> lock(state_mutex_);
  std::filesystem::create_directories(config_.state_path.parent_path());
  nlohmann::json state = {
      {"ok", true},
      {"service", "sekailink_lobby_admin_service"},
      {"listen_host", config_.listen_host},
      {"http_port", config_.http_port},
      {"sqlite_path", config_.sqlite_path.string()},
      {"state_path", config_.state_path.string()},
      {"runtime_bridge_enabled", config_.runtime_bridge.enabled},
      {"runtime_bridge_host", config_.runtime_bridge.host},
      {"runtime_bridge_port", config_.runtime_bridge.port},
      {"total_requests", total_requests_},
      {"total_errors", total_errors_},
      {"updated_at", utc_timestamp_now()},
  };
  std::ofstream stream(config_.state_path);
  stream << state.dump(2) << "\n";
}

nlohmann::json LobbyAdminService::lobby_to_json(const LobbyAdminStore::LobbyRecord& lobby) {
  return {
      {"id", lobby.id},
      {"lobby_id", lobby.lobby_id},
      {"name", lobby.name},
      {"visibility", lobby.visibility},
      {"status", lobby.status},
      {"owner_username", lobby.owner_username.has_value() ? nlohmann::json(*lobby.owner_username) : nlohmann::json(nullptr)},
      {"description", lobby.description.has_value() ? nlohmann::json(*lobby.description) : nlohmann::json(nullptr)},
      {"rules", lobby.rules},
      {"metadata", lobby.metadata},
      {"created_at", lobby.created_at},
      {"updated_at", lobby.updated_at},
      {"closed_at", lobby.closed_at.has_value() ? nlohmann::json(*lobby.closed_at) : nlohmann::json(nullptr)},
  };
}

LobbyAdminHttpServer::LobbyAdminHttpServer(LobbyAdminServiceConfig config)
    : service_(config), config_(std::move(config)) {}

LobbyAdminHttpServer::~LobbyAdminHttpServer() {
  stop();
}

bool LobbyAdminHttpServer::start() {
#ifdef _WIN32
  throw std::runtime_error("lobby_admin_http_not_supported_on_windows_yet");
#else
  if (listen_fd_ >= 0) return false;
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;
  int reuse = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config_.http_port);
  if (::inet_pton(AF_INET, config_.listen_host.c_str(), &address.sin_addr) != 1) {
    stop();
    return false;
  }
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    stop();
    return false;
  }
  if (::listen(listen_fd_, 16) != 0) {
    stop();
    return false;
  }
  socket_len_t len = sizeof(address);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &len) == 0) {
    bound_port_ = ntohs(address.sin_port);
  }
  return true;
#endif
}

void LobbyAdminHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
    bound_port_ = 0;
  }
#endif
}

std::uint16_t LobbyAdminHttpServer::port() const {
  return bound_port_;
}

void LobbyAdminHttpServer::serve_one() const {
#ifdef _WIN32
  throw std::runtime_error("lobby_admin_http_not_supported_on_windows_yet");
#else
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(listen_fd_, &read_fds);
  timeval timeout{};
  timeout.tv_sec = 1;
  const auto ready = ::select(listen_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
  if (ready <= 0) return;
  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (client_fd < 0) throw std::runtime_error("lobby_admin_accept_failed");
  std::string request;
  char buffer[4096];
  std::size_t content_length = 0;
  while (request.find("\r\n\r\n") == std::string::npos) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      ::close(client_fd);
      throw std::runtime_error("lobby_admin_recv_failed");
    }
    request.append(buffer, static_cast<std::size_t>(received));
    const auto content_length_pos = request.find("Content-Length:");
    if (content_length_pos != std::string::npos) {
      const auto line_end = request.find("\r\n", content_length_pos);
      const auto value = trim(request.substr(content_length_pos + 15, line_end - (content_length_pos + 15)));
      if (!value.empty()) content_length = static_cast<std::size_t>(std::stoul(value));
    }
  }
  const auto headers_end = request.find("\r\n\r\n");
  const auto body_start = headers_end + 4;
  while (request.size() < body_start + content_length) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) break;
    request.append(buffer, static_cast<std::size_t>(received));
  }
  std::istringstream request_stream(request.substr(0, headers_end));
  std::string method;
  std::string path;
  std::string version;
  request_stream >> method >> path >> version;
  std::optional<std::string> bearer_token;
  LobbyAdminRequestContext context;
  std::string header_line;
  std::getline(request_stream, header_line);
  while (std::getline(request_stream, header_line)) {
    if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
    if (header_line.rfind("Authorization: Bearer ", 0) == 0) {
      bearer_token = header_line.substr(22);
    } else if (header_line.rfind("User-Agent: ", 0) == 0) {
      context.user_agent = trim(header_line.substr(12));
    } else if (header_line.rfind("X-SekaiLink-Client: ", 0) == 0) {
      context.client_name = trim(header_line.substr(20));
    } else if (header_line.rfind("X-SekaiLink-Client-Version: ", 0) == 0) {
      context.client_version = trim(header_line.substr(28));
    } else if (header_line.rfind("X-SekaiLink-Device-Id: ", 0) == 0) {
      context.device_id = trim(header_line.substr(23));
    } else if (header_line.rfind("X-SekaiLink-Locale: ", 0) == 0) {
      context.requested_locale = trim(header_line.substr(20));
    }
  }
  std::optional<nlohmann::json> body;
  if (content_length > 0 && request.size() >= body_start + content_length) {
    body = nlohmann::json::parse(request.substr(body_start, content_length));
  }
  const auto response_json = service_.handle(method, path, bearer_token, body, context);
  const auto status_code = response_json.value("status", response_json.value("ok", false) ? 200 : 500);
  const auto response = json_http_response(status_code, response_json);
  ::send(client_fd, response.data(), response.size(), 0);
  ::close(client_fd);
#endif
}

}  // namespace sekailink_server
