#include "sekailink_server/chat_api_service.hpp"

#include <sqlite3.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
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
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace sekailink_server {
namespace {

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.pop_back();
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string path_without_query(std::string_view raw_path) {
  const auto q = raw_path.find('?');
  return std::string(raw_path.substr(0, q == std::string_view::npos ? raw_path.size() : q));
}

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < path.size()) {
    while (start < path.size() && path[start] == '/') ++start;
    if (start >= path.size()) break;
    auto end = path.find('/', start);
    if (end == std::string_view::npos) end = path.size();
    parts.emplace_back(path.substr(start, end - start));
    start = end;
  }
  return parts;
}

std::string url_decode(std::string_view value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '+') {
      decoded.push_back(' ');
      continue;
    }
    if (value[i] == '%' && i + 2 < value.size()) {
      const auto hex = std::string(value.substr(i + 1, 2));
      char* end = nullptr;
      const auto parsed = std::strtol(hex.c_str(), &end, 16);
      if (end && *end == '\0') {
        decoded.push_back(static_cast<char>(parsed));
        i += 2;
        continue;
      }
    }
    decoded.push_back(value[i]);
  }
  return decoded;
}

std::string query_value(std::string_view raw_path, std::string_view key) {
  const auto q = raw_path.find('?');
  if (q == std::string_view::npos) return "";
  std::string_view query = raw_path.substr(q + 1);
  while (!query.empty()) {
    const auto amp = query.find('&');
    const auto pair = query.substr(0, amp == std::string_view::npos ? query.size() : amp);
    const auto eq = pair.find('=');
    const auto name = eq == std::string_view::npos ? pair : pair.substr(0, eq);
    if (name == key) return url_decode(eq == std::string_view::npos ? std::string_view{} : pair.substr(eq + 1));
    if (amp == std::string_view::npos) break;
    query = query.substr(amp + 1);
  }
  return "";
}

std::string http_status_text(int status_code) {
  switch (status_code) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 502: return "Bad Gateway";
    case 501: return "Not Implemented";
    default: return "Internal Server Error";
  }
}

std::string json_response(int status_code, const nlohmann::json& body) {
  const auto payload = body.dump();
  std::ostringstream stream;
  stream << "HTTP/1.1 " << status_code << ' ' << http_status_text(status_code) << "\r\n";
  stream << "Content-Type: application/json\r\n";
  stream << "Content-Length: " << payload.size() << "\r\n";
  stream << "Connection: close\r\n\r\n";
  stream << payload;
  return stream.str();
}

std::string raw_response(int status_code, const std::string& body) {
  std::ostringstream stream;
  stream << "HTTP/1.1 " << status_code << ' ' << http_status_text(status_code) << "\r\n";
  stream << "Content-Type: application/json\r\n";
  stream << "Content-Length: " << body.size() << "\r\n";
  stream << "Connection: close\r\n\r\n";
  stream << body;
  return stream.str();
}

std::string sanitized_text(std::string value, std::size_t max_len) {
  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
    return c == '\r' || c == '\n' || c == '\0';
  }), value.end());
  value = trim(value);
  if (value.size() > max_len) value.resize(max_len);
  return value;
}

bool is_safe_channel_id(const std::string& value) {
  if (value.empty() || value.size() > 96) return false;
  return std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return std::isalnum(c) != 0 || c == ':' || c == '_' || c == '-';
  });
}

bool is_public_channel_allowed(const std::string& value) {
  return value == "global:fr" || value == "global:en" || value.rfind("lobby:", 0) == 0;
}

std::string utc_now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif
  std::ostringstream stream;
  stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::string first_string(const nlohmann::json& object, const std::vector<std::string>& keys, const std::string& fallback) {
  for (const auto& key : keys) {
    if (object.contains(key) && object.at(key).is_string()) {
      const auto value = sanitized_text(object.at(key).get<std::string>(), 80);
      if (!value.empty()) return value;
    }
  }
  return fallback;
}

std::string first_string_limited(
    const nlohmann::json& object,
    const std::vector<std::string>& keys,
    const std::string& fallback,
    std::size_t max_len) {
  for (const auto& key : keys) {
    if (object.contains(key) && object.at(key).is_string()) {
      const auto value = sanitized_text(object.at(key).get<std::string>(), max_len);
      if (!value.empty()) return value;
    }
  }
  return fallback;
}

std::string avatar_url_from_user(const nlohmann::json& user) {
  return first_string_limited(user, {"avatar_url"}, "", 256 * 1024);
}

std::string user_id_string(const nlohmann::json& user) {
  if (user.contains("user_id")) {
    if (user.at("user_id").is_number_integer()) return std::to_string(user.at("user_id").get<long long>());
    if (user.at("user_id").is_string()) return sanitized_text(user.at("user_id").get<std::string>(), 80);
  }
  if (user.contains("id")) {
    if (user.at("id").is_number_integer()) return std::to_string(user.at("id").get<long long>());
    if (user.at("id").is_string()) return sanitized_text(user.at("id").get<std::string>(), 80);
  }
  return "";
}

std::string legacy_display_name(const nlohmann::json& user) {
  return first_string(user, {"display_name", "global_name", "username", "email"}, "SekaiLink");
}

std::string legacy_username(const nlohmann::json& user) {
  return first_string(user, {"username", "email", "display_name"}, "SekaiLink");
}

std::string identity_user_id(const nlohmann::json& identity) {
  if (!identity.contains("user") || !identity.at("user").is_object()) return "";
  auto out = user_id_string(identity.at("user"));
  if (out.empty()) out = legacy_username(identity.at("user"));
  return sanitized_text(out, 80);
}

std::string identity_username(const nlohmann::json& identity) {
  if (!identity.contains("user") || !identity.at("user").is_object()) return "SekaiLink";
  return legacy_username(identity.at("user"));
}

std::string identity_display_name(const nlohmann::json& identity) {
  if (!identity.contains("user") || !identity.at("user").is_object()) return "SekaiLink";
  return legacy_display_name(identity.at("user"));
}

std::string identity_avatar_url(const nlohmann::json& identity) {
  if (!identity.contains("user") || !identity.at("user").is_object()) return "";
  return avatar_url_from_user(identity.at("user"));
}

nlohmann::json parse_body_json(const ChatApiHttpResponse& response) {
  try {
    if (response.body.empty()) return nlohmann::json::object();
    return nlohmann::json::parse(response.body);
  } catch (...) {
    return nlohmann::json::object();
  }
}

int response_status_from_body(const ChatApiHttpResponse& response, const nlohmann::json& parsed) {
  if (parsed.contains("status") && parsed.at("status").is_number_integer()) return parsed.at("status").get<int>();
  return response.status;
}

nlohmann::json legacy_user_from_identity(const nlohmann::json& identity) {
  const auto& user = identity.at("user");
  auto user_id = user_id_string(user);
  if (user_id.empty()) user_id = legacy_username(user);
  const auto username = legacy_username(user);
  const auto display_name = legacy_display_name(user);
  nlohmann::json result = {
      {"ok", true},
      {"id", user_id},
      {"user_id", user_id},
      {"discord_id", user_id},
      {"username", username},
      {"global_name", display_name},
      {"display_name", display_name},
      {"avatar_url", avatar_url_from_user(user)},
  };
  for (const auto& key : {"email", "role", "permissions", "patreon_tier", "game_keys"}) {
    if (user.contains(key)) result[key] = user.at(key);
  }
  if (identity.contains("session")) result["session"] = identity.at("session");
  return result;
}

std::string json_string_or(const nlohmann::json& object, const char* key, const std::string& fallback = "") {
  if (!object.contains(key) || !object.at(key).is_string()) return fallback;
  return object.at(key).get<std::string>();
}

int json_int_or(const nlohmann::json& object, const char* key, int fallback = 0) {
  if (!object.contains(key)) return fallback;
  if (object.at(key).is_number_integer()) return object.at(key).get<int>();
  if (object.at(key).is_number_unsigned()) return static_cast<int>(object.at(key).get<unsigned int>());
  return fallback;
}

nlohmann::json legacy_lobby_from_runtime(const nlohmann::json& lobby) {
  const auto metadata = lobby.contains("metadata") && lobby.at("metadata").is_object() ? lobby.at("metadata") : nlohmann::json::object();
  const auto lobby_id = json_string_or(lobby, "lobby_id", json_string_or(lobby, "id"));
  const auto visibility = json_string_or(lobby, "visibility", "private");
  const auto presence_count = json_int_or(lobby, "presence_count", 0);
  const auto owner = json_string_or(lobby, "owner_username");
  const auto updated_at = json_string_or(lobby, "updated_at");
  nlohmann::json result = {
      {"id", lobby_id},
      {"lobby_id", lobby_id},
      {"name", json_string_or(lobby, "name", lobby_id.empty() ? std::string("SekaiLink Room") : lobby_id)},
      {"description", json_string_or(lobby, "description")},
      {"visibility", visibility},
      {"is_private", visibility == "private"},
      {"owner", owner},
      {"owner_username", owner},
      {"member_count", presence_count},
      {"players", presence_count},
      {"max_players", metadata.value("max_players", 50)},
      {"status", json_string_or(lobby, "status", "open")},
      {"created_at", json_string_or(lobby, "created_at")},
      {"updated_at", updated_at},
      {"last_activity", json_string_or(lobby, "last_activity_at", updated_at)},
      {"metadata", metadata},
  };
  for (const auto& key : {"game", "mode", "release_mode", "language", "tags", "games"}) {
    if (metadata.contains(key)) result[key] = metadata.at(key);
  }
  return result;
}

nlohmann::json legacy_members_from_runtime_lobby(const nlohmann::json& lobby) {
  nlohmann::json members = nlohmann::json::array();
  const auto owner = lobby.value("owner_username", std::string{});
  if (!lobby.contains("presence") || !lobby.at("presence").is_array()) return members;
  for (const auto& item : lobby.at("presence")) {
    const auto username = item.value("username", std::string{});
    members.push_back({
        {"discord_id", username},
        {"user_id", username},
        {"username", username},
        {"global_name", username},
        {"display_name", username},
        {"ready", false},
        {"is_host", !owner.empty() && username == owner},
        {"joined_at", item.value("joined_at", std::string{})},
        {"updated_at", item.value("updated_at", std::string{})},
    });
  }
  return members;
}

std::string linkedworld_id_from_game(std::string game) {
  game = lower(trim(std::move(game)));
  std::string out;
  out.reserve(game.size());
  for (const auto c : game) {
    if (std::isalnum(static_cast<unsigned char>(c)) != 0) out.push_back(c);
    else if (c == ' ' || c == '-' || c == '_' || c == ':') out.push_back('-');
  }
  while (out.find("--") != std::string::npos) out.erase(out.find("--"), 1);
  while (!out.empty() && out.front() == '-') out.erase(out.begin());
  while (!out.empty() && out.back() == '-') out.pop_back();
  return out.empty() ? "unknown" : out;
}

nlohmann::json normalize_lobby_game_selections(const nlohmann::json& raw) {
  nlohmann::json grouped = nlohmann::json::array();
  std::map<std::string, std::size_t> by_game;
  const auto add_config = [&](nlohmann::json& target, const nlohmann::json& config) {
    const auto id = config.value("id", config.value("yaml_id", config.value("config_id", std::string{})));
    if (id.empty()) return;
    for (const auto& existing : target) {
      if (existing.value("id", std::string{}) == id) return;
    }
    target.push_back({
        {"id", id},
        {"config_id", id},
        {"yaml_id", id},
        {"title", config.value("title", id)},
        {"game", config.value("game", std::string("unknown"))},
        {"player_name", config.value("player_name", std::string{})},
        {"custom", config.value("custom", true)},
    });
  };

  const auto append_config = [&](const nlohmann::json& config) {
    const auto game = config.value("game", std::string("unknown"));
    const auto linkedworld_id = config.value("linkedworld_id", linkedworld_id_from_game(game));
    if (!by_game.contains(linkedworld_id)) {
      by_game[linkedworld_id] = grouped.size();
      grouped.push_back({
          {"linkedworld_id", linkedworld_id},
          {"game", game},
          {"configs", nlohmann::json::array()},
      });
    }
    add_config(grouped.at(by_game[linkedworld_id]).at("configs"), config);
  };

  if (raw.is_array()) {
    for (const auto& item : raw) {
      if (!item.is_object()) continue;
      if (item.contains("configs") && item.at("configs").is_array()) {
        for (const auto& config : item.at("configs")) {
          if (!config.is_object()) continue;
          auto copy = config;
          if (!copy.contains("game")) copy["game"] = item.value("game", std::string("unknown"));
          if (!copy.contains("linkedworld_id")) copy["linkedworld_id"] = item.value("linkedworld_id", linkedworld_id_from_game(copy.value("game", std::string("unknown"))));
          append_config(copy);
        }
      } else {
        append_config(item);
      }
    }
  }
  return grouped;
}

nlohmann::json flatten_lobby_game_selections(const nlohmann::json& selections) {
  nlohmann::json flat = nlohmann::json::array();
  if (!selections.is_array()) return flat;
  for (const auto& game : selections) {
    if (!game.is_object() || !game.contains("configs") || !game.at("configs").is_array()) continue;
    for (const auto& config : game.at("configs")) flat.push_back(config);
  }
  return flat;
}

std::string safe_lobby_id_from_name(const std::string& name, const std::string& user_id) {
  std::string base;
  for (const auto c : lower(name)) {
    if (std::isalnum(static_cast<unsigned char>(c)) != 0) base.push_back(c);
    else if (c == ' ' || c == '-' || c == '_') base.push_back('-');
  }
  while (base.find("--") != std::string::npos) base.erase(base.find("--"), 1);
  while (!base.empty() && base.front() == '-') base.erase(base.begin());
  while (!base.empty() && base.back() == '-') base.pop_back();
  if (base.empty()) base = "sekailink-room";
  if (base.size() > 32) base.resize(32);
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  std::string suffix = user_id;
  suffix.erase(std::remove_if(suffix.begin(), suffix.end(), [](unsigned char c) {
    return std::isalnum(c) == 0;
  }), suffix.end());
  if (suffix.size() > 8) suffix.resize(8);
  return base + "-" + std::to_string(now) + (suffix.empty() ? "" : "-" + suffix);
}

ChatApiHttpResponse legacy_not_implemented(const std::string& endpoint) {
  return {501, nlohmann::json{{"ok", false}, {"error", "not_implemented"}, {"endpoint", endpoint}}.dump()};
}

void sqlite_exec_checked(sqlite3* db, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    std::string message = error ? error : "sqlite_exec_failed";
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

std::string sqlite_column_string(sqlite3_stmt* stmt, int column) {
  const auto* text = sqlite3_column_text(stmt, column);
  return text ? reinterpret_cast<const char*>(text) : "";
}

#ifndef _WIN32
void close_fd(int fd) {
  if (fd >= 0) close(fd);
}

bool wait_for_read(int fd, int timeout_ms) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  return select(fd + 1, &set, nullptr, nullptr, &tv) > 0;
}

bool write_all(int fd, const std::string& data) {
  const char* ptr = data.data();
  std::size_t left = data.size();
  while (left > 0) {
    const auto sent = send(fd, ptr, left, MSG_NOSIGNAL);
    if (sent <= 0) return false;
    ptr += sent;
    left -= static_cast<std::size_t>(sent);
  }
  return true;
}

ChatApiHttpResponse http_request(
    const std::string& host,
    std::uint16_t port,
    const std::string& method,
    const std::string& path,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* result = nullptr;
  const auto port_text = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0 || result == nullptr) {
    return {502, R"({"ok":false,"error":"upstream_resolve_failed"})"};
  }
  int fd = -1;
  for (auto* ai = result; ai != nullptr; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
    close_fd(fd);
    fd = -1;
  }
  freeaddrinfo(result);
  if (fd < 0) return {502, R"({"ok":false,"error":"upstream_connect_failed"})"};

  std::ostringstream request;
  request << method << ' ' << path << " HTTP/1.1\r\n";
  request << "Host: " << host << ':' << port << "\r\n";
  request << "Connection: close\r\n";
  if (!body.empty()) {
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
  }
  for (const auto& header : headers) request << header.first << ": " << header.second << "\r\n";
  request << "\r\n";
  request << body;
  if (!write_all(fd, request.str())) {
    close_fd(fd);
    return {502, R"({"ok":false,"error":"upstream_write_failed"})"};
  }

  std::string response;
  char buffer[4096];
  while (wait_for_read(fd, 2000)) {
    const auto n = recv(fd, buffer, sizeof(buffer), 0);
    if (n <= 0) break;
    response.append(buffer, static_cast<std::size_t>(n));
    if (response.size() > 1024 * 1024) break;
  }
  close_fd(fd);
  if (response.empty()) return {502, R"({"ok":false,"error":"upstream_empty_response"})"};

  std::istringstream input(response);
  std::string version;
  int status = 502;
  input >> version >> status;
  const auto body_pos = response.find("\r\n\r\n");
  return {status, body_pos == std::string::npos ? std::string{} : response.substr(body_pos + 4)};
}
#endif

}  // namespace

ChatApiServiceConfig load_chat_api_service_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) throw std::runtime_error("chat_api_config_open_failed");
  nlohmann::json data;
  stream >> data;
  ChatApiServiceConfig config;
  if (data.contains("http_port")) config.http_port = static_cast<std::uint16_t>(data.at("http_port").get<int>());
  if (data.contains("listen_host")) config.listen_host = data.at("listen_host").get<std::string>();
  if (data.contains("identity_host")) config.identity_host = data.at("identity_host").get<std::string>();
  if (data.contains("identity_port")) config.identity_port = static_cast<std::uint16_t>(data.at("identity_port").get<int>());
  if (data.contains("chat_gateway_host")) config.chat_gateway_host = data.at("chat_gateway_host").get<std::string>();
  if (data.contains("chat_gateway_port")) config.chat_gateway_port = static_cast<std::uint16_t>(data.at("chat_gateway_port").get<int>());
  if (data.contains("chat_gateway_token")) config.chat_gateway_token = trim(data.at("chat_gateway_token").get<std::string>());
  if (data.contains("lobby_runtime_host")) config.lobby_runtime_host = data.at("lobby_runtime_host").get<std::string>();
  if (data.contains("lobby_runtime_port")) config.lobby_runtime_port = static_cast<std::uint16_t>(data.at("lobby_runtime_port").get<int>());
  if (data.contains("lobby_runtime_token")) config.lobby_runtime_token = trim(data.at("lobby_runtime_token").get<std::string>());
  if (data.contains("sqlite_path")) config.sqlite_path = data.at("sqlite_path").get<std::string>();
  return config;
}

nlohmann::json to_json(const ChatApiServiceConfig& config) {
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"identity_host", config.identity_host},
      {"identity_port", config.identity_port},
      {"chat_gateway_host", config.chat_gateway_host},
      {"chat_gateway_port", config.chat_gateway_port},
      {"gateway_token_configured", !config.chat_gateway_token.empty()},
      {"lobby_runtime_host", config.lobby_runtime_host},
      {"lobby_runtime_port", config.lobby_runtime_port},
      {"lobby_runtime_token_configured", !config.lobby_runtime_token.empty()},
      {"sqlite_configured", !config.sqlite_path.empty()},
  };
}

ChatApiService::ChatApiService(ChatApiServiceConfig config) : config_(std::move(config)) {
  initialize_store();
}

void ChatApiService::initialize_store() const {
  if (config_.sqlite_path.empty()) return;
  std::scoped_lock lock(store_mutex_);
  std::filesystem::create_directories(config_.sqlite_path.parent_path());
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    std::string error = db ? sqlite3_errmsg(db) : "sqlite_open_failed";
    if (db) sqlite3_close(db);
    throw std::runtime_error(error);
  }
  try {
    sqlite_exec_checked(db, "PRAGMA journal_mode=WAL;");
    sqlite_exec_checked(db, "PRAGMA synchronous=NORMAL;");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS chat_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "channel_id TEXT NOT NULL,"
        "user_id TEXT,"
        "username TEXT,"
        "display_name TEXT,"
        "avatar_url TEXT,"
        "content TEXT NOT NULL,"
        "created_at TEXT NOT NULL"
        ");");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_chat_messages_channel_id_id ON chat_messages(channel_id, id);");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS chat_presence ("
        "channel_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "username TEXT,"
        "display_name TEXT,"
        "avatar_url TEXT,"
        "last_seen TEXT NOT NULL,"
        "PRIMARY KEY(channel_id, user_id)"
        ");");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_chat_presence_channel_last_seen ON chat_presence(channel_id, last_seen);");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS social_profiles ("
        "user_id TEXT PRIMARY KEY,"
        "username TEXT,"
        "display_name TEXT,"
        "avatar_url TEXT,"
        "updated_at TEXT NOT NULL"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS social_settings ("
        "user_id TEXT PRIMARY KEY,"
        "presence_status TEXT NOT NULL DEFAULT 'online',"
        "dm_policy TEXT NOT NULL DEFAULT 'friends',"
        "updated_at TEXT NOT NULL"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS social_friends ("
        "user_id TEXT NOT NULL,"
        "friend_id TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "PRIMARY KEY(user_id, friend_id)"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS social_friend_requests ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "from_id TEXT NOT NULL,"
        "to_id TEXT NOT NULL,"
        "status TEXT NOT NULL DEFAULT 'pending',"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ");");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_social_friend_requests_to_status ON social_friend_requests(to_id, status);");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_social_friend_requests_from_status ON social_friend_requests(from_id, status);");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS social_blocks ("
        "user_id TEXT NOT NULL,"
        "blocked_id TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "PRIMARY KEY(user_id, blocked_id)"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS social_dm_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "from_id TEXT NOT NULL,"
        "to_id TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "read_at TEXT"
        ");");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_social_dm_pair ON social_dm_messages(from_id, to_id, id);");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_game_selections ("
        "lobby_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "selection_json TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "PRIMARY KEY(lobby_id, user_id)"
        ");");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_lobby_game_selections_lobby ON lobby_game_selections(lobby_id, updated_at);");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_player_state ("
        "lobby_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "ready INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL,"
        "PRIMARY KEY(lobby_id, user_id)"
        ");");
    sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_lobby_player_state_lobby_username ON lobby_player_state(lobby_id, username);");
  } catch (...) {
    sqlite3_close(db);
    throw;
  }
  sqlite3_close(db);
}

std::optional<nlohmann::json> ChatApiService::validate_session(
    const std::string& session_token,
    const std::optional<std::string>& device_id) const {
#ifdef _WIN32
  (void)session_token;
  (void)device_id;
  return std::nullopt;
#else
  const auto cache_key = session_token + "\n" + device_id.value_or("");
  const auto now = std::chrono::steady_clock::now();
  {
    std::scoped_lock lock(session_cache_mutex_);
    auto it = session_cache_.find(cache_key);
    if (it != session_cache_.end()) {
      if (it->second.expires_at > now) return it->second.identity;
      session_cache_.erase(it);
    }
  }
  std::vector<std::pair<std::string, std::string>> headers = {
      {"Authorization", "Bearer " + session_token},
      {"X-SekaiLink-Client", "chat-api"},
  };
  if (device_id.has_value() && !device_id->empty()) headers.push_back({"X-SekaiLink-Device-Id", *device_id});
  const auto validation_started_at = std::chrono::steady_clock::now();
  const auto response = http_request(
      config_.identity_host,
      config_.identity_port,
      "GET",
      "/me",
      headers,
      "");
  const auto validation_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - validation_started_at).count();
  if (validation_ms > 500) {
    std::cerr << "chat_api_identity_validation_slow status=" << response.status
              << " ms=" << validation_ms << '\n';
  }
  if (response.status != 200) return std::nullopt;
  try {
    auto data = nlohmann::json::parse(response.body);
    if (!data.value("ok", false) || !data.contains("user") || !data.at("user").is_object()) return std::nullopt;
    {
      std::scoped_lock lock(session_cache_mutex_);
      if (session_cache_.size() > 512) session_cache_.clear();
      session_cache_[cache_key] = SessionCacheEntry{
          data,
          std::chrono::steady_clock::now() + std::chrono::seconds(30),
      };
    }
    return data;
  } catch (...) {
    return std::nullopt;
  }
#endif
}

ChatApiHttpResponse ChatApiService::forward_to_gateway(
    const std::string& method,
    const std::string& path,
    const std::optional<nlohmann::json>& body) const {
#ifdef _WIN32
  (void)method;
  (void)path;
  (void)body;
  return {502, R"({"ok":false,"error":"unsupported_platform"})"};
#else
  return http_request(
      config_.chat_gateway_host,
      config_.chat_gateway_port,
      method,
      path,
      {{"Authorization", "Bearer " + config_.chat_gateway_token}},
      body.has_value() ? body->dump() : "");
#endif
}

ChatApiHttpResponse ChatApiService::forward_to_lobby_runtime(
    const std::string& method,
    const std::string& path,
    const std::optional<nlohmann::json>& body) const {
#ifdef _WIN32
  (void)method;
  (void)path;
  (void)body;
  return {502, R"({"ok":false,"error":"unsupported_platform"})"};
#else
  if (config_.lobby_runtime_token.empty()) {
    return {502, R"({"ok":false,"error":"lobby_runtime_token_missing"})"};
  }
  return http_request(
      config_.lobby_runtime_host,
      config_.lobby_runtime_port,
      method,
      path,
      {{"Authorization", "Bearer " + config_.lobby_runtime_token}, {"X-SekaiLink-Client", "chat-api-compat"}},
      body.has_value() ? body->dump() : "");
#endif
}

ChatApiHttpResponse ChatApiService::handle_legacy_me(const nlohmann::json& identity) const {
  return {200, legacy_user_from_identity(identity).dump()};
}

ChatApiHttpResponse ChatApiService::handle_legacy_lobbies(
    const std::string& method,
    const std::vector<std::string>& parts,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  const auto& user = identity.at("user");
  const auto username = legacy_username(user);
  const auto display_name = legacy_display_name(user);
  auto user_id = user_id_string(user);
  if (user_id.empty()) user_id = username;

  if (parts.size() == 1 && method == "GET") {
    const auto upstream = forward_to_lobby_runtime("GET", "/runtime/lobbies/open?limit=100", std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false)) return {status, upstream.body};
    nlohmann::json lobbies = nlohmann::json::array();
    if (parsed.contains("lobbies") && parsed.at("lobbies").is_array()) {
      for (const auto& lobby : parsed.at("lobbies")) lobbies.push_back(legacy_lobby_from_runtime(lobby));
    }
    return {200, nlohmann::json{{"ok", true}, {"lobbies", lobbies}}.dump()};
  }

  if (parts.size() == 1 && method == "POST") {
    const auto request = body.value_or(nlohmann::json::object());
    const auto name = sanitized_text(request.value("name", std::string("SekaiLink Room")), 80);
    const auto description = sanitized_text(request.value("description", std::string{}), 240);
    const auto password = sanitized_text(request.value("password", std::string{}), 120);
    const auto visibility = password.empty() ? std::string("public") : std::string("private");
    const auto lobby_id = safe_lobby_id_from_name(name, user_id);
    nlohmann::json metadata = request;
    metadata["legacy_created_by"] = "chat-api-compat";
    if (!metadata.contains("max_players")) metadata["max_players"] = request.value("max_players", 50);
    const nlohmann::json create_body = {
        {"lobby_id", lobby_id},
        {"name", name.empty() ? std::string("SekaiLink Room") : name},
        {"visibility", visibility},
        {"owner_username", display_name},
        {"description", description},
        {"metadata", metadata},
    };
    const auto created = forward_to_lobby_runtime("POST", "/admin/runtime/lobbies/open", create_body);
    const auto created_json = parse_body_json(created);
    auto status = response_status_from_body(created, created_json);
    if ((status < 200 || status >= 300) || !created_json.value("ok", false)) return {status, created.body};
    const nlohmann::json join_body = {{"username", display_name}};
    (void)forward_to_lobby_runtime("POST", "/runtime/lobbies/" + lobby_id + "/presence/join", join_body);
    const auto lobby = created_json.contains("lobby") ? legacy_lobby_from_runtime(created_json.at("lobby")) : nlohmann::json::object();
    return {200, nlohmann::json{{"ok", true}, {"lobby", lobby}, {"id", lobby_id}, {"url", "/lobby/" + lobby_id}}.dump()};
  }

  if (parts.size() < 2) return {404, R"({"ok":false,"error":"not_found"})"};
  const auto lobby_id = url_decode(parts[1]);
  if (lobby_id.empty() || lobby_id.size() > 160) return {400, R"({"ok":false,"error":"invalid_lobby_id"})"};
  const auto channel_id = "lobby:" + lobby_id;

  if (parts.size() == 2 && method == "GET") {
    const auto upstream = forward_to_lobby_runtime("GET", "/runtime/lobbies/" + lobby_id, std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false)) return {status, upstream.body};
    return {200, nlohmann::json{{"ok", true}, {"lobby", legacy_lobby_from_runtime(parsed.at("lobby"))}}.dump()};
  }

  if (parts.size() == 3 && parts[2] == "join" && method == "POST") {
    const nlohmann::json join_body = {{"username", display_name}};
    const auto upstream = forward_to_lobby_runtime("POST", "/runtime/lobbies/" + lobby_id + "/presence/join", join_body);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false)) return {status, upstream.body};
    return {200, nlohmann::json{{"ok", true}, {"lobby", legacy_lobby_from_runtime(parsed.at("lobby"))}}.dump()};
  }

  if (parts.size() == 3 && parts[2] == "members" && method == "GET") {
    const auto upstream = forward_to_lobby_runtime("GET", "/runtime/lobbies/" + lobby_id, std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false)) return {status, upstream.body};
    const auto& lobby = parsed.at("lobby");
    auto members = legacy_members_from_runtime_lobby(lobby);
    if (!config_.sqlite_path.empty()) {
      std::scoped_lock lock(store_mutex_);
      sqlite3* db = nullptr;
      if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) == SQLITE_OK && db != nullptr) {
        (void)sqlite3_exec(
            db,
            "CREATE TABLE IF NOT EXISTS lobby_player_state ("
            "lobby_id TEXT NOT NULL,"
            "user_id TEXT NOT NULL,"
            "username TEXT NOT NULL,"
            "ready INTEGER NOT NULL DEFAULT 0,"
            "updated_at TEXT NOT NULL,"
            "PRIMARY KEY(lobby_id, user_id)"
            ");",
            nullptr,
            nullptr,
            nullptr);
        (void)sqlite3_exec(
            db,
            "CREATE TABLE IF NOT EXISTS lobby_game_selections ("
            "lobby_id TEXT NOT NULL,"
            "user_id TEXT NOT NULL,"
            "selection_json TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "PRIMARY KEY(lobby_id, user_id)"
            ");",
            nullptr,
            nullptr,
            nullptr);
        for (auto& member : members) {
          const auto member_name = member.value("display_name", member.value("username", std::string{}));
          if (member_name.empty()) continue;
          sqlite3_stmt* state_stmt = nullptr;
          if (sqlite3_prepare_v2(
                  db,
                  "SELECT user_id, ready FROM lobby_player_state WHERE lobby_id = ? AND username = ? ORDER BY updated_at DESC LIMIT 1;",
                  -1,
                  &state_stmt,
                  nullptr) == SQLITE_OK) {
            sqlite3_bind_text(state_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(state_stmt, 2, member_name.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(state_stmt) == SQLITE_ROW) {
              const auto selected_user_id = sqlite_column_string(state_stmt, 0);
              member["ready"] = sqlite3_column_int(state_stmt, 1) != 0;
              sqlite3_stmt* selection_stmt = nullptr;
              if (sqlite3_prepare_v2(
                      db,
                      "SELECT selection_json FROM lobby_game_selections WHERE lobby_id = ? AND user_id = ?;",
                      -1,
                      &selection_stmt,
                      nullptr) == SQLITE_OK) {
                sqlite3_bind_text(selection_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(selection_stmt, 2, selected_user_id.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(selection_stmt) == SQLITE_ROW) {
                  try {
                    const auto selections = normalize_lobby_game_selections(nlohmann::json::parse(sqlite_column_string(selection_stmt, 0)));
                    const auto flat = flatten_lobby_game_selections(selections);
                    member["selections"] = selections;
                    const auto game_count = selections.is_array() ? selections.size() : std::size_t{0};
                    member["selection_summary"] = flat.empty() ? std::string("No game selected") : std::to_string(game_count) + " game" + (game_count == 1 ? "" : "s") + " · " + std::to_string(flat.size()) + " config" + (flat.size() == 1 ? "" : "s");
                    member["active_yamls"] = flat;
                    if (!flat.empty()) {
                      member["active_yaml_id"] = flat.at(0).value("id", std::string{});
                      member["active_yaml_title"] = flat.at(0).value("title", std::string{});
                      member["active_yaml_game"] = flat.at(0).value("game", std::string{});
                      member["active_yaml_player"] = flat.at(0).value("player_name", member_name);
                    }
                  } catch (...) {
                    member["selections"] = nlohmann::json::array();
                    member["selection_summary"] = std::string("No game selected");
                    member["active_yamls"] = nlohmann::json::array();
                  }
                }
                sqlite3_finalize(selection_stmt);
              }
            }
            sqlite3_finalize(state_stmt);
          }
        }
        sqlite3_close(db);
      } else if (db) {
        sqlite3_close(db);
      }
    }
    return {200, nlohmann::json{{"ok", true}, {"members", members}}.dump()};
  }

  if (parts.size() == 3 && parts[2] == "messages" && method == "GET") {
    return handle_list_messages(channel_id);
  }
  if (parts.size() == 3 && parts[2] == "messages" && method == "POST") {
    if (!body.has_value() || !body->is_object()) return {400, R"({"ok":false,"error":"invalid_body"})"};
    const auto content = sanitized_text(body->value("content", ""), 400);
    if (content.empty()) return {400, R"({"ok":false,"error":"empty_message"})"};
    const auto relay = forward_to_gateway(
        "POST",
        "/channels/" + channel_id + "/messages",
        nlohmann::json{
            {"author", display_name},
            {"user_id", user_id},
            {"username", username},
            {"display_name", display_name},
            {"avatar_url", avatar_url_from_user(user)},
            {"role", "player"},
            {"ready", false},
            {"content", content},
        });
    if (relay.status < 200 || relay.status >= 300) return relay;
    return remember_message(channel_id, identity, content);
  }

  if (parts.size() == 3 && parts[2] == "generation" && method == "GET") {
    if (config_.sqlite_path.empty()) {
      return {200, nlohmann::json{{"ok", true}, {"status", "idle"}, {"completed", false}, {"running", false}, {"error", ""}}.dump()};
    }
    std::scoped_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      return {500, R"({"ok":false,"error":"generation_store_open_failed"})"};
    }
    auto close_db = [&]() { sqlite3_close(db); };
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_generation_state ("
        "lobby_id TEXT PRIMARY KEY,"
        "generation_id TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "room_url TEXT NOT NULL DEFAULT '',"
        "error TEXT NOT NULL DEFAULT '',"
        "players_json TEXT NOT NULL DEFAULT '[]',"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ");");
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT generation_id, status, room_url, error, players_json, created_at, updated_at "
            "FROM lobby_generation_state WHERE lobby_id = ? LIMIT 1;",
            -1,
            &stmt,
            nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_state_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      close_db();
      return {200, nlohmann::json{{"ok", true}, {"status", "idle"}, {"completed", false}, {"running", false}, {"error", ""}}.dump()};
    }
    const auto generation_id = sqlite_column_string(stmt, 0);
    const auto status = sqlite_column_string(stmt, 1);
    const auto room_url = sqlite_column_string(stmt, 2);
    const auto error = sqlite_column_string(stmt, 3);
    const auto players_json = sqlite_column_string(stmt, 4);
    const auto created_at = sqlite_column_string(stmt, 5);
    const auto updated_at = sqlite_column_string(stmt, 6);
    sqlite3_finalize(stmt);
    close_db();
    nlohmann::json players = nlohmann::json::array();
    try {
      players = nlohmann::json::parse(players_json);
      if (!players.is_array()) players = nlohmann::json::array();
    } catch (...) {
      players = nlohmann::json::array();
    }
    const auto normalized_status = status.empty() ? std::string("idle") : status;
    const auto completed =
        normalized_status == "completed" || normalized_status == "success" || normalized_status == "failed" || normalized_status == "error";
    const auto running = normalized_status == "queued" || normalized_status == "in_progress";
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"generation_id", generation_id},
            {"status", normalized_status},
            {"running", running},
            {"completed", completed},
            {"room_url", room_url},
            {"error", error},
            {"players", players},
            {"created_at", created_at},
            {"updated_at", updated_at},
        }.dump()};
  }
  if (parts.size() == 3 && parts[2] == "host-status" && method == "GET") {
    return {200, nlohmann::json{{"ok", true}, {"host_absent", false}, {"host_absent_seconds", 0}, {"candidate_name", ""}, {"vote_count", 0}, {"vote_needed", 0}}.dump()};
  }
  if (parts.size() == 3 && parts[2] == "hint-catalog" && method == "GET") {
    return {200, nlohmann::json{{"ok", true}, {"hints", nlohmann::json::array()}, {"players", nlohmann::json::array()}}.dump()};
  }
  if (parts.size() == 3 && parts[2] == "ready" && method == "POST") {
    const auto request = body.value_or(nlohmann::json::object());
    const auto next_ready = request.contains("ready") ? request.value("ready", true) : true;
    if (config_.sqlite_path.empty()) {
      return {200, nlohmann::json{{"ok", true}, {"ready", next_ready}}.dump()};
    }
    std::scoped_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      return {500, R"({"ok":false,"error":"ready_store_open_failed"})"};
    }
    auto close_db = [&]() { sqlite3_close(db); };
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_game_selections ("
        "lobby_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "selection_json TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "PRIMARY KEY(lobby_id, user_id)"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_player_state ("
        "lobby_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "ready INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL,"
        "PRIMARY KEY(lobby_id, user_id)"
        ");");
    if (next_ready) {
      bool has_selection = false;
      sqlite3_stmt* selection_stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT selection_json FROM lobby_game_selections WHERE lobby_id = ? AND user_id = ?;",
              -1,
              &selection_stmt,
              nullptr) == SQLITE_OK) {
        sqlite3_bind_text(selection_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(selection_stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(selection_stmt) == SQLITE_ROW) {
          try {
            has_selection = !flatten_lobby_game_selections(
                                 normalize_lobby_game_selections(nlohmann::json::parse(sqlite_column_string(selection_stmt, 0))))
                                 .empty();
          } catch (...) {
            has_selection = false;
          }
        }
        sqlite3_finalize(selection_stmt);
      }
      if (!has_selection) {
        close_db();
        return {400, R"({"ok":false,"error":"missing_game_selection"})"};
      }
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO lobby_player_state (lobby_id, user_id, username, ready, updated_at) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(lobby_id, user_id) DO UPDATE SET username=excluded.username, ready=excluded.ready, updated_at=excluded.updated_at;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"ready_store_prepare_failed"})"};
    }
    const auto now = utc_now_iso8601();
    sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, display_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, next_ready ? 1 : 0);
    sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    close_db();
    if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"ready_store_failed"})"};
    return {200, nlohmann::json{{"ok", true}, {"ready", next_ready}}.dump()};
  }
  if (parts.size() == 3 && (parts[2] == "active-yaml" || parts[2] == "game-selections") && (method == "GET" || method == "POST")) {
    if (config_.sqlite_path.empty()) {
      return {
          200,
          nlohmann::json{
              {"ok", true},
              {"lobby_id", lobby_id},
              {"user_id", user_id},
              {"locked", false},
              {"selections", nlohmann::json::array()},
              {"active_yamls", nlohmann::json::array()},
          }.dump()};
    }

    std::scoped_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      return {500, R"({"ok":false,"error":"selection_store_open_failed"})"};
    }
    const auto close_db = [&]() { sqlite3_close(db); };
    try {
      sqlite_exec_checked(
          db,
          "CREATE TABLE IF NOT EXISTS world_configs ("
          "id TEXT PRIMARY KEY,"
          "user_id TEXT NOT NULL,"
          "title TEXT NOT NULL,"
          "game TEXT NOT NULL,"
          "content TEXT NOT NULL,"
          "created_at TEXT NOT NULL,"
          "updated_at TEXT NOT NULL"
          ");");
      sqlite_exec_checked(
          db,
          "CREATE TABLE IF NOT EXISTS lobby_game_selections ("
          "lobby_id TEXT NOT NULL,"
          "user_id TEXT NOT NULL,"
          "selection_json TEXT NOT NULL,"
          "updated_at TEXT NOT NULL,"
          "PRIMARY KEY(lobby_id, user_id)"
          ");");
      sqlite_exec_checked(
          db,
          "CREATE TABLE IF NOT EXISTS lobby_player_state ("
          "lobby_id TEXT NOT NULL,"
          "user_id TEXT NOT NULL,"
          "username TEXT NOT NULL,"
          "ready INTEGER NOT NULL DEFAULT 0,"
          "updated_at TEXT NOT NULL,"
          "PRIMARY KEY(lobby_id, user_id)"
          ");");
      sqlite_exec_checked(
          db,
          "CREATE TABLE IF NOT EXISTS lobby_generation_state ("
          "lobby_id TEXT PRIMARY KEY,"
          "generation_id TEXT NOT NULL,"
          "status TEXT NOT NULL,"
          "room_url TEXT NOT NULL DEFAULT '',"
          "error TEXT NOT NULL DEFAULT '',"
          "players_json TEXT NOT NULL DEFAULT '[]',"
          "created_at TEXT NOT NULL,"
          "updated_at TEXT NOT NULL"
          ");");
    } catch (...) {
      close_db();
      return {500, R"({"ok":false,"error":"selection_store_init_failed"})"};
    }

    auto read_locked = [&]() {
      bool locked = false;
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT ready FROM lobby_player_state WHERE lobby_id = ? AND user_id = ? LIMIT 1;",
              -1,
              &stmt,
              nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) locked = sqlite3_column_int(stmt, 0) != 0;
        sqlite3_finalize(stmt);
      }
      return locked;
    };

    auto read_generation_locked = [&]() {
      bool locked = false;
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT status, room_url FROM lobby_generation_state WHERE lobby_id = ? LIMIT 1;",
              -1,
              &stmt,
              nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          const auto status = sqlite_column_string(stmt, 0);
          const auto room_url = sqlite_column_string(stmt, 1);
          locked = status == "queued" || status == "in_progress" || status == "completed" || status == "success" || !room_url.empty();
        }
        sqlite3_finalize(stmt);
      }
      return locked;
    };

    auto read_selection = [&]() {
      nlohmann::json selections = nlohmann::json::array();
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT selection_json FROM lobby_game_selections WHERE lobby_id = ? AND user_id = ?;",
              -1,
              &stmt,
              nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          try {
            selections = normalize_lobby_game_selections(nlohmann::json::parse(sqlite_column_string(stmt, 0)));
          } catch (...) {
            selections = nlohmann::json::array();
          }
        }
        sqlite3_finalize(stmt);
      }
      return selections;
    };

    auto write_selection = [&](const nlohmann::json& selections) {
      sqlite3_stmt* stmt = nullptr;
      const char* sql =
          "INSERT INTO lobby_game_selections (lobby_id, user_id, selection_json, updated_at) VALUES (?, ?, ?, ?) "
          "ON CONFLICT(lobby_id, user_id) DO UPDATE SET selection_json=excluded.selection_json, updated_at=excluded.updated_at;";
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
      const auto payload = normalize_lobby_game_selections(selections).dump();
      const auto now = utc_now_iso8601();
      sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
      const auto step = sqlite3_step(stmt);
      sqlite3_finalize(stmt);
      return step == SQLITE_DONE;
    };

    auto load_config = [&](const std::string& yaml_id) -> std::optional<nlohmann::json> {
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT id, title, game FROM world_configs WHERE user_id = ? AND id = ?;",
              -1,
              &stmt,
              nullptr) != SQLITE_OK) {
        return std::nullopt;
      }
      sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, yaml_id.c_str(), -1, SQLITE_TRANSIENT);
      std::optional<nlohmann::json> result;
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto game = sqlite_column_string(stmt, 2);
        result = nlohmann::json{
            {"id", sqlite_column_string(stmt, 0)},
            {"config_id", sqlite_column_string(stmt, 0)},
            {"yaml_id", sqlite_column_string(stmt, 0)},
            {"title", sqlite_column_string(stmt, 1)},
            {"game", game},
            {"linkedworld_id", linkedworld_id_from_game(game)},
            {"player_name", display_name},
            {"custom", true},
        };
      }
      sqlite3_finalize(stmt);
      return result;
    };

    nlohmann::json selections = read_selection();
    const auto locked = read_locked() || read_generation_locked();
    if (method == "POST") {
      if (locked) {
        close_db();
        return {
            409,
            nlohmann::json{
                {"ok", false},
                {"error", "selection_locked"},
                {"locked", true},
                {"lobby_id", lobby_id},
                {"user_id", user_id},
            }.dump()};
      }
      const auto request = body.value_or(nlohmann::json::object());
      if (request.contains("selections") && request.at("selections").is_array()) {
        selections = normalize_lobby_game_selections(request.at("selections"));
      } else {
        const auto action = sanitized_text(request.value("action", std::string("add")), 32);
        const auto yaml_id = sanitized_text(request.value("yaml_id", request.value("config_id", std::string{})), 160);
        if (yaml_id.empty()) {
          close_db();
          return {400, R"({"ok":false,"error":"missing_config_id"})"};
        }
        auto flat = flatten_lobby_game_selections(selections);
        if (action == "remove") {
          nlohmann::json next = nlohmann::json::array();
          for (const auto& item : flat) {
            if (item.value("id", std::string{}) != yaml_id && item.value("yaml_id", std::string{}) != yaml_id) next.push_back(item);
          }
          selections = normalize_lobby_game_selections(next);
        } else {
          const auto config = load_config(yaml_id);
          if (!config.has_value()) {
            close_db();
            return {404, R"({"ok":false,"error":"config_not_found"})"};
          }
          flat.push_back(*config);
          selections = normalize_lobby_game_selections(flat);
        }
      }
      if (!write_selection(selections)) {
        close_db();
        return {500, R"({"ok":false,"error":"selection_save_failed"})"};
      }
      sqlite3_stmt* state_stmt = nullptr;
      const char* state_sql =
          "INSERT INTO lobby_player_state (lobby_id, user_id, username, ready, updated_at) VALUES (?, ?, ?, 0, ?) "
          "ON CONFLICT(lobby_id, user_id) DO UPDATE SET username=excluded.username, updated_at=excluded.updated_at;";
      if (sqlite3_prepare_v2(db, state_sql, -1, &state_stmt, nullptr) == SQLITE_OK) {
        const auto now = utc_now_iso8601();
        sqlite3_bind_text(state_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(state_stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(state_stmt, 3, display_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(state_stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(state_stmt);
        sqlite3_finalize(state_stmt);
      }
    }

    const auto active_yamls = flatten_lobby_game_selections(selections);
    close_db();
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"lobby_id", lobby_id},
            {"user_id", user_id},
            {"locked", locked},
            {"selections", selections},
            {"active_yamls", active_yamls},
        }.dump()};
  }
  if (parts.size() == 3 && parts[2] == "close" && method == "POST") {
    const auto upstream = forward_to_lobby_runtime("POST", "/runtime/lobbies/" + lobby_id + "/close", std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    return {status, upstream.body};
  }
  if (parts.size() == 3 && parts[2] == "settings" && method == "POST") {
    const auto request = body.value_or(nlohmann::json::object());
    const auto current = forward_to_lobby_runtime("GET", "/runtime/lobbies/" + lobby_id, std::nullopt);
    const auto current_json = parse_body_json(current);
    nlohmann::json metadata = nlohmann::json::object();
    if (current_json.value("ok", false) && current_json.contains("lobby") && current_json.at("lobby").contains("metadata")) {
      metadata = current_json.at("lobby").at("metadata");
    }
    for (auto it = request.begin(); it != request.end(); ++it) metadata[it.key()] = it.value();
    const nlohmann::json patch_body = {{"metadata", metadata}};
    const auto upstream = forward_to_lobby_runtime("PATCH", "/admin/runtime/lobbies/" + lobby_id, patch_body);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status < 200 || status >= 300 || !parsed.value("ok", false)) return {status, upstream.body};
    return {200, nlohmann::json{{"ok", true}, {"lobby", legacy_lobby_from_runtime(parsed.at("lobby"))}}.dump()};
  }
  if (parts.size() == 3 && parts[2] == "release" && method == "POST") {
    return {200, nlohmann::json{{"ok", true}, {"released", true}, {"compat", true}}.dump()};
  }
  if (parts.size() == 3 && parts[2] == "kick" && method == "POST") {
    const auto request = body.value_or(nlohmann::json::object());
    const auto target = sanitized_text(request.value("name", request.value("user_id", std::string{})), 80);
    if (!target.empty()) {
      (void)forward_to_lobby_runtime("POST", "/runtime/lobbies/" + lobby_id + "/presence/leave", nlohmann::json{{"username", target}});
    }
    return {200, nlohmann::json{{"ok", true}, {"removed", !target.empty()}, {"target", target}}.dump()};
  }
  if (parts.size() == 3 && parts[2] == "vote-host" && method == "POST") {
    return {200, nlohmann::json{{"ok", true}, {"vote_count", 1}, {"vote_needed", 1}, {"host_absent", false}}.dump()};
  }
  if (parts.size() == 3 && parts[2] == "generate" && method == "POST") {
    const auto upstream = forward_to_lobby_runtime("GET", "/runtime/lobbies/" + lobby_id, std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto runtime_status = response_status_from_body(upstream, parsed);
    if (runtime_status != 200 || !parsed.value("ok", false) || !parsed.contains("lobby")) return {runtime_status, upstream.body};
    const auto members = legacy_members_from_runtime_lobby(parsed.at("lobby"));
    if (members.empty()) return {400, R"({"ok":false,"error":"empty_lobby"})"};
    if (config_.sqlite_path.empty()) return {503, R"({"ok":false,"error":"selection_store_missing"})"};

    std::scoped_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      return {500, R"({"ok":false,"error":"generation_store_open_failed"})"};
    }
    auto close_db = [&]() { sqlite3_close(db); };
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_player_state ("
        "lobby_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "ready INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL,"
        "PRIMARY KEY(lobby_id, user_id)"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_game_selections ("
        "lobby_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "selection_json TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "PRIMARY KEY(lobby_id, user_id)"
        ");");
    sqlite_exec_checked(
        db,
        "CREATE TABLE IF NOT EXISTS lobby_generation_state ("
        "lobby_id TEXT PRIMARY KEY,"
        "generation_id TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "room_url TEXT NOT NULL DEFAULT '',"
        "error TEXT NOT NULL DEFAULT '',"
        "players_json TEXT NOT NULL DEFAULT '[]',"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ");");

    nlohmann::json players = nlohmann::json::array();
    for (const auto& member : members) {
      const auto member_name = member.value("display_name", member.value("username", std::string{}));
      if (member_name.empty()) continue;
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT user_id, ready FROM lobby_player_state WHERE lobby_id = ? AND username = ? ORDER BY updated_at DESC LIMIT 1;",
              -1,
              &stmt,
              nullptr) != SQLITE_OK) {
        close_db();
        return {500, R"({"ok":false,"error":"generation_state_prepare_failed"})"};
      }
      sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, member_name.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        close_db();
        return {409, nlohmann::json{{"ok", false}, {"error", "player_not_ready"}, {"player", member_name}}.dump()};
      }
      const auto member_user_id = sqlite_column_string(stmt, 0);
      const auto member_ready = sqlite3_column_int(stmt, 1) != 0;
      sqlite3_finalize(stmt);
      if (!member_ready) {
        close_db();
        return {409, nlohmann::json{{"ok", false}, {"error", "player_not_ready"}, {"player", member_name}}.dump()};
      }

      sqlite3_stmt* selection_stmt = nullptr;
      if (sqlite3_prepare_v2(
              db,
              "SELECT selection_json FROM lobby_game_selections WHERE lobby_id = ? AND user_id = ?;",
              -1,
              &selection_stmt,
              nullptr) != SQLITE_OK) {
        close_db();
        return {500, R"({"ok":false,"error":"generation_selection_prepare_failed"})"};
      }
      sqlite3_bind_text(selection_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(selection_stmt, 2, member_user_id.c_str(), -1, SQLITE_TRANSIENT);
      nlohmann::json selections = nlohmann::json::array();
      if (sqlite3_step(selection_stmt) == SQLITE_ROW) {
        try {
          selections = normalize_lobby_game_selections(nlohmann::json::parse(sqlite_column_string(selection_stmt, 0)));
        } catch (...) {
          selections = nlohmann::json::array();
        }
      }
      sqlite3_finalize(selection_stmt);
      if (flatten_lobby_game_selections(selections).empty()) {
        close_db();
        return {409, nlohmann::json{{"ok", false}, {"error", "player_missing_game_selection"}, {"player", member_name}}.dump()};
      }
      players.push_back({{"user_id", member_user_id}, {"username", member_name}, {"selections", selections}});
    }

    const auto job_id = lobby_id + "-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    const auto now = utc_now_iso8601();
    sqlite3_stmt* generation_stmt = nullptr;
    const char* generation_sql =
        "INSERT INTO lobby_generation_state (lobby_id, generation_id, status, room_url, error, players_json, created_at, updated_at) "
        "VALUES (?, ?, 'queued', '', '', ?, ?, ?) "
        "ON CONFLICT(lobby_id) DO UPDATE SET generation_id=excluded.generation_id, status=excluded.status, room_url=excluded.room_url, "
        "error=excluded.error, players_json=excluded.players_json, updated_at=excluded.updated_at;";
    if (sqlite3_prepare_v2(db, generation_sql, -1, &generation_stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_state_prepare_failed"})"};
    }
    const auto players_dump = players.dump();
    sqlite3_bind_text(generation_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 2, job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 3, players_dump.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto generation_step = sqlite3_step(generation_stmt);
    sqlite3_finalize(generation_stmt);
    if (generation_step != SQLITE_DONE) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_state_save_failed"})"};
    }
    close_db();
    return {
        202,
        nlohmann::json{
            {"ok", true},
            {"lobby_id", lobby_id},
            {"generation_id", job_id},
            {"status", "queued"},
            {"running", false},
            {"completed", false},
            {"locked", true},
            {"players", players},
            {"created_at", now},
            {"updated_at", now},
            {"message", "Generation service handoff accepted by compatibility layer; full Evolution integration is pending."},
        }.dump()};
  }

  return {404, R"({"ok":false,"error":"not_found"})"};
}

void ChatApiService::upsert_social_profile(const nlohmann::json& identity) const {
  if (config_.sqlite_path.empty()) return;
  const auto user_id = identity_user_id(identity);
  if (user_id.empty()) return;
  const auto username = identity_username(identity);
  const auto display_name = identity_display_name(identity);
  const auto avatar_url = identity_avatar_url(identity);
  const auto now = utc_now_iso8601();
  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return;
  }
  const char* sql =
      "INSERT INTO social_profiles (user_id, username, display_name, avatar_url, updated_at) "
      "VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT(user_id) DO UPDATE SET "
      "username=excluded.username, display_name=excluded.display_name, avatar_url=excluded.avatar_url, updated_at=excluded.updated_at;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, display_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, avatar_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);
}

ChatApiHttpResponse ChatApiService::handle_social(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  const auto me = identity_user_id(identity);
  if (me.empty()) return {401, R"({"ok":false,"error":"unauthorized"})"};
  upsert_social_profile(identity);
  if (config_.sqlite_path.empty()) {
    return {200, nlohmann::json{{"ok", true}, {"friends", nlohmann::json::array()}, {"incoming", nlohmann::json::array()}, {"outgoing", nlohmann::json::array()}, {"blocks", nlohmann::json::array()}}.dump()};
  }

  const auto now = utc_now_iso8601();
  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return {500, R"({"ok":false,"error":"social_store_open_failed"})"};
  }
  auto close_db = [&]() { sqlite3_close(db); };
  auto profile_json = [&](const std::string& user_id) {
    nlohmann::json out = {
        {"user_id", user_id},
        {"username", user_id},
        {"display_name", user_id},
        {"name", user_id},
        {"avatar_url", ""},
        {"status", "online"},
    };
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT username, display_name, avatar_url FROM social_profiles WHERE user_id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto username = sqlite_column_string(stmt, 0);
        const auto display_name = sqlite_column_string(stmt, 1);
        out["username"] = username;
        out["display_name"] = display_name;
        out["name"] = display_name.empty() ? username : display_name;
        out["avatar_url"] = sqlite_column_string(stmt, 2);
      }
      sqlite3_finalize(stmt);
    }
    return out;
  };
  auto body_user_id = [&]() {
    if (!body.has_value() || !body->is_object()) return std::string{};
    return sanitized_text(body->value("user_id", std::string{}), 80);
  };

  if (method == "GET" && parts.size() == 2 && parts[1] == "settings") {
    nlohmann::json settings = {{"presence_status", "online"}, {"dm_policy", "friends"}};
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT presence_status, dm_policy FROM social_settings WHERE user_id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings["presence_status"] = sqlite_column_string(stmt, 0);
        settings["dm_policy"] = sqlite_column_string(stmt, 1);
      }
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"presence_status", settings["presence_status"]}, {"dm_policy", settings["dm_policy"]}, {"settings", settings}}.dump()};
  }
  if (method == "POST" && parts.size() == 2 && parts[1] == "settings") {
    const auto presence = sanitized_text(body.value_or(nlohmann::json::object()).value("presence_status", std::string("online")), 32);
    const auto dm_policy = sanitized_text(body.value_or(nlohmann::json::object()).value("dm_policy", std::string("friends")), 32);
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO social_settings (user_id, presence_status, dm_policy, updated_at) VALUES (?, ?, ?, ?) "
        "ON CONFLICT(user_id) DO UPDATE SET presence_status=excluded.presence_status, dm_policy=excluded.dm_policy, updated_at=excluded.updated_at;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"social_store_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, presence.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, dm_policy.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    close_db();
    if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"social_settings_update_failed"})"};
    return {200, nlohmann::json{{"ok", true}, {"presence_status", presence}, {"dm_policy", dm_policy}}.dump()};
  }

  if (method == "GET" && parts.size() == 2 && parts[1] == "friends") {
    nlohmann::json friends = nlohmann::json::array();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT friend_id FROM social_friends WHERE user_id = ? ORDER BY friend_id LIMIT 250;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) friends.push_back(profile_json(sqlite_column_string(stmt, 0)));
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"friends", friends}}.dump()};
  }
  if (method == "POST" && parts.size() == 3 && parts[1] == "friends" && parts[2] == "request") {
    const auto target = body_user_id();
    if (target.empty() || target == me) {
      close_db();
      return {400, R"({"ok":false,"error":"invalid_user_id"})"};
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO social_friend_requests (from_id, to_id, status, created_at, updated_at) "
        "VALUES (?, ?, 'pending', ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"social_store_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto step = sqlite3_step(stmt);
    const auto id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    close_db();
    if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"friend_request_failed"})"};
    return {200, nlohmann::json{{"ok", true}, {"request", {{"id", id}, {"from_id", me}, {"to_id", target}, {"status", "pending"}}}}.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "requests") {
    nlohmann::json incoming = nlohmann::json::array();
    nlohmann::json outgoing = nlohmann::json::array();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT id, from_id, to_id, created_at FROM social_friend_requests WHERE to_id = ? AND status = 'pending' ORDER BY id DESC LIMIT 100;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto from_id = sqlite_column_string(stmt, 1);
        auto item = profile_json(from_id);
        item["id"] = sqlite3_column_int64(stmt, 0);
        item["from_id"] = from_id;
        item["from_name"] = item.value("name", from_id);
        item["created_at"] = sqlite_column_string(stmt, 3);
        incoming.push_back(item);
      }
      sqlite3_finalize(stmt);
    }
    if (sqlite3_prepare_v2(db, "SELECT id, from_id, to_id, created_at FROM social_friend_requests WHERE from_id = ? AND status = 'pending' ORDER BY id DESC LIMIT 100;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto to_id = sqlite_column_string(stmt, 2);
        auto item = profile_json(to_id);
        item["id"] = sqlite3_column_int64(stmt, 0);
        item["to_id"] = to_id;
        item["to_name"] = item.value("name", to_id);
        item["created_at"] = sqlite_column_string(stmt, 3);
        outgoing.push_back(item);
      }
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"incoming", incoming}, {"outgoing", outgoing}}.dump()};
  }
  if (method == "POST" && parts.size() == 3 && parts[1] == "friends" && parts[2] == "respond") {
    const auto request_id = body.value_or(nlohmann::json::object()).value("request_id", 0);
    const auto action = sanitized_text(body.value_or(nlohmann::json::object()).value("action", std::string{}), 24);
    if (request_id <= 0 || (action != "accept" && action != "decline" && action != "reject")) {
      close_db();
      return {400, R"({"ok":false,"error":"invalid_request"})"};
    }
    sqlite3_stmt* stmt = nullptr;
    std::string from_id;
    if (sqlite3_prepare_v2(db, "SELECT from_id FROM social_friend_requests WHERE id = ? AND to_id = ? AND status = 'pending';", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_int64(stmt, 1, request_id);
      sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) from_id = sqlite_column_string(stmt, 0);
      sqlite3_finalize(stmt);
    }
    if (from_id.empty()) {
      close_db();
      return {404, R"({"ok":false,"error":"request_not_found"})"};
    }
    const auto next_status = action == "accept" ? std::string("accepted") : std::string("declined");
    if (sqlite3_prepare_v2(db, "UPDATE social_friend_requests SET status = ?, updated_at = ? WHERE id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, next_status.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(stmt, 3, request_id);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
    if (action == "accept") {
      sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
      const char* sql = "INSERT OR IGNORE INTO social_friends (user_id, friend_id, created_at) VALUES (?, ?, ?);";
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, from_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, from_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
      }
      sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"status", next_status}}.dump()};
  }
  if (method == "POST" && parts.size() == 3 && parts[1] == "friends" && parts[2] == "remove") {
    const auto target = body_user_id();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM social_friends WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?);", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, R"({"ok":true})"};
  }

  if (method == "GET" && parts.size() == 2 && parts[1] == "blocks") {
    nlohmann::json blocks = nlohmann::json::array();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT blocked_id, created_at FROM social_blocks WHERE user_id = ? ORDER BY created_at DESC LIMIT 250;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto item = profile_json(sqlite_column_string(stmt, 0));
        item["blocked_id"] = sqlite_column_string(stmt, 0);
        item["created_at"] = sqlite_column_string(stmt, 1);
        blocks.push_back(item);
      }
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"blocks", blocks}}.dump()};
  }
  if (method == "POST" && parts.size() == 2 && parts[1] == "blocks") {
    const auto target = body_user_id();
    if (target.empty() || target == me) {
      close_db();
      return {400, R"({"ok":false,"error":"invalid_user_id"})"};
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO social_blocks (user_id, blocked_id, created_at) VALUES (?, ?, ?);", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, R"({"ok":true})"};
  }
  if (method == "POST" && parts.size() == 3 && parts[1] == "blocks" && parts[2] == "remove") {
    const auto target = body_user_id();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM social_blocks WHERE user_id = ? AND blocked_id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, R"({"ok":true})"};
  }

  if (method == "GET" && parts.size() == 2 && parts[1] == "unread-count") {
    sqlite3_stmt* stmt = nullptr;
    int unread = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM social_dm_messages WHERE to_id = ? AND read_at IS NULL;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) unread = sqlite3_column_int(stmt, 0);
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"count", unread}, {"unread_count", unread}}.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "messages") {
    const auto target = sanitized_text(query_value(raw_path, "user_id"), 80);
    nlohmann::json messages = nlohmann::json::array();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, from_id, to_id, content, created_at, read_at FROM social_dm_messages "
        "WHERE (from_id = ? AND to_id = ?) OR (from_id = ? AND to_id = ?) ORDER BY id DESC LIMIT 100;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, me.c_str(), -1, SQLITE_TRANSIENT);
      std::vector<nlohmann::json> reversed;
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        reversed.push_back({
            {"id", sqlite3_column_int64(stmt, 0)},
            {"from_id", sqlite_column_string(stmt, 1)},
            {"to_id", sqlite_column_string(stmt, 2)},
            {"content", sqlite_column_string(stmt, 3)},
            {"created_at", sqlite_column_string(stmt, 4)},
            {"read_at", sqlite_column_string(stmt, 5)},
        });
      }
      sqlite3_finalize(stmt);
      for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) messages.push_back(*it);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"messages", messages}}.dump()};
  }
  if (method == "POST" && parts.size() == 2 && parts[1] == "messages") {
    const auto target = body_user_id();
    const auto content = sanitized_text(body.value_or(nlohmann::json::object()).value("content", std::string{}), 1000);
    if (target.empty() || content.empty()) {
      close_db();
      return {400, R"({"ok":false,"error":"invalid_message"})"};
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO social_dm_messages (from_id, to_id, content, created_at) VALUES (?, ?, ?, ?);", -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"social_store_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto step = sqlite3_step(stmt);
    const auto id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    close_db();
    if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"dm_send_failed"})"};
    return {200, nlohmann::json{{"ok", true}, {"message", {{"id", id}, {"from_id", me}, {"to_id", target}, {"content", content}, {"created_at", now}}}}.dump()};
  }
  if (method == "POST" && parts.size() == 3 && parts[1] == "messages" && parts[2] == "read") {
    const auto target = body_user_id();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE social_dm_messages SET read_at = ? WHERE from_id = ? AND to_id = ? AND read_at IS NULL;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, R"({"ok":true})"};
  }
  if (method == "GET" && parts.size() == 3 && parts[1] == "users" && parts[2] == "search") {
    const auto q = lower(sanitized_text(query_value(raw_path, "q"), 80));
    nlohmann::json users = nlohmann::json::array();
    if (!q.empty()) {
      sqlite3_stmt* stmt = nullptr;
      const char* sql =
          "SELECT user_id, username, display_name, avatar_url FROM social_profiles "
          "WHERE user_id != ? AND (lower(username) LIKE ? OR lower(display_name) LIKE ?) "
          "ORDER BY lower(display_name), lower(username) LIMIT 20;";
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        const auto pattern = "%" + q + "%";
        sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          const auto username = sqlite_column_string(stmt, 1);
          const auto display_name = sqlite_column_string(stmt, 2);
          users.push_back({
              {"user_id", sqlite_column_string(stmt, 0)},
              {"username", username},
              {"display_name", display_name},
              {"name", display_name.empty() ? username : display_name},
              {"avatar_url", sqlite_column_string(stmt, 3)},
          });
        }
        sqlite3_finalize(stmt);
      }
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"users", users}}.dump()};
  }

  close_db();
  return {404, R"({"ok":false,"error":"not_found"})"};
}

ChatApiHttpResponse ChatApiService::handle_world_config(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  (void)raw_path;
  const auto me = identity_user_id(identity);
  if (me.empty()) return {401, R"({"ok":false,"error":"unauthorized"})"};
  if (config_.sqlite_path.empty()) {
    return {200, nlohmann::json{{"ok", true}, {"yamls", nlohmann::json::array()}, {"games", nlohmann::json::array()}}.dump()};
  }

  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return {500, R"({"ok":false,"error":"world_config_store_open_failed"})"};
  }
  sqlite_exec_checked(
      db,
      "CREATE TABLE IF NOT EXISTS world_configs ("
      "id TEXT PRIMARY KEY,"
      "user_id TEXT NOT NULL,"
      "title TEXT NOT NULL,"
      "game TEXT NOT NULL,"
      "content TEXT NOT NULL,"
      "created_at TEXT NOT NULL,"
      "updated_at TEXT NOT NULL"
      ");");
  sqlite_exec_checked(db, "CREATE INDEX IF NOT EXISTS idx_world_configs_user_game ON world_configs(user_id, game);");
  auto close_db = [&]() { sqlite3_close(db); };
  const auto now = utc_now_iso8601();

  if (method == "GET" && parts.size() == 1 && parts[0] == "yamls") {
    nlohmann::json yamls = nlohmann::json::array();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT id, title, game, updated_at FROM world_configs WHERE user_id = ? ORDER BY updated_at DESC LIMIT 500;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        yamls.push_back({
            {"id", sqlite_column_string(stmt, 0)},
            {"title", sqlite_column_string(stmt, 1)},
            {"game", sqlite_column_string(stmt, 2)},
            {"player_name", identity_display_name(identity)},
            {"custom", true},
            {"updated_at", sqlite_column_string(stmt, 3)},
        });
      }
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"yamls", yamls}}.dump()};
  }
  if (method == "GET" && parts.size() == 3 && parts[0] == "yamls" && parts[2] == "raw") {
    const auto id = url_decode(parts[1]);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT title, game, content FROM world_configs WHERE user_id = ? AND id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"world_config_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      close_db();
      return {404, R"({"ok":false,"error":"yaml_not_found"})"};
    }
    const auto title = sqlite_column_string(stmt, 0);
    const auto game = sqlite_column_string(stmt, 1);
    const auto content = sqlite_column_string(stmt, 2);
    sqlite3_finalize(stmt);
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"id", id}, {"yaml_title", title}, {"title", title}, {"game", game}, {"content", content}}.dump()};
  }
  if (method == "POST" && parts.size() == 2 && parts[0] == "yamls" && parts[1] == "import") {
    const auto request = body.value_or(nlohmann::json::object());
    const auto content = request.contains("content") && request.at("content").is_string()
        ? request.at("content").get<std::string>()
        : std::string{};
    if (content.empty()) {
      close_db();
      return {400, R"({"ok":false,"error":"empty_yaml"})"};
    }
    auto title = sanitized_text(request.value("title", std::string("Imported WorldConfig")), 120);
    auto game = sanitized_text(request.value("game", std::string("unknown")), 80);
    const auto id = safe_lobby_id_from_name(title + "-import", me + "-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()));
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO world_configs (id, user_id, title, game, content, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"world_config_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, game.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    close_db();
    if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"yaml_import_failed"})"};
    return {200, nlohmann::json{{"ok", true}, {"import_id", id}, {"id", id}, {"yaml", {{"id", id}, {"title", title}, {"game", game}, {"custom", true}}}}.dump()};
  }
  if (method == "GET" && parts.size() == 3 && parts[0] == "yamls" && parts[1] == "import") {
    const auto id = url_decode(parts[2]);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT title, game, content FROM world_configs WHERE user_id = ? AND id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"world_config_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      close_db();
      return {404, R"({"ok":false,"error":"yaml_not_found"})"};
    }
    const auto title = sqlite_column_string(stmt, 0);
    const auto game = sqlite_column_string(stmt, 1);
    const auto content = sqlite_column_string(stmt, 2);
    sqlite3_finalize(stmt);
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"import_id", id}, {"id", id}, {"yaml_title", title}, {"title", title}, {"game", game}, {"content", content}}.dump()};
  }
  if (method == "POST" && ((parts.size() == 1 && parts[0] == "yamls") || (parts.size() == 2 && parts[0] == "yamls" && parts[1] == "new"))) {
    const auto request = body.value_or(nlohmann::json::object());
    const auto title = sanitized_text(request.value("title", std::string("WorldConfig")), 120);
    const auto game = sanitized_text(request.value("game", std::string("unknown")), 80);
    const auto content = request.contains("content") && request.at("content").is_string()
        ? request.at("content").get<std::string>()
        : request.dump(2);
    const auto id = safe_lobby_id_from_name(title + "-" + game, me);
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO world_configs (id, user_id, title, game, content, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"world_config_prepare_failed"})"};
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, game.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    close_db();
    if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"yaml_save_failed"})"};
    return {200, nlohmann::json{{"ok", true}, {"id", id}, {"yaml", {{"id", id}, {"title", title}, {"game", game}, {"custom", true}}}}.dump()};
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "yamls" && parts[2] == "delete") {
    const auto id = url_decode(parts[1]);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM world_configs WHERE user_id = ? AND id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, R"({"ok":true})"};
  }
  if (method == "GET" && parts.size() >= 2 && parts[0] == "player-options") {
    const auto game = url_decode(parts[1]);
    close_db();
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"game", game},
            {"schema", nlohmann::json::object()},
            {"options", nlohmann::json::array()},
            {"message", "WorldConfig/playerOptions schema endpoint is online; game-specific schemas are pending LinkedWorld manifests."},
        }.dump()};
  }
  if (method == "POST" && parts.size() >= 3 && parts[0] == "player-options" && parts[2] == "dashboard-save") {
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"saved", false}, {"message", "WorldConfig save compatibility acknowledged; full schema persistence is pending."}}.dump()};
  }

  close_db();
  return {404, R"({"ok":false,"error":"not_found"})"};
}

ChatApiHttpResponse ChatApiService::handle_client_compat(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const std::optional<nlohmann::json>& body) const {
  (void)raw_path;
  (void)body;
  if (parts.empty() || parts[0] != "client") return {404, R"({"ok":false,"error":"not_found"})"};
  if (method == "GET" && parts.size() == 2 && parts[1] == "version") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"latest", "0.3.1"},
            {"min_supported", "0.1.0"},
            {"channel", "dev"},
            {"notes_url", "https://sekailink.com/progress.html"},
            {"incremental_manifest_url", ""},
        }.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "release-latest") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"available", false},
            {"version", "0.3.1"},
            {"channel", query_value(raw_path, "channel").empty() ? std::string("dev") : query_value(raw_path, "channel")},
            {"download_url", ""},
            {"notes_url", "https://sekailink.com/progress.html"},
        }.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "runtime-versions") {
    return {200, nlohmann::json{{"ok", true}, {"bizhawk_version", ""}, {"poptracker_version", ""}, {"sekailink_core_version", "0.3.1"}}.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "games") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"games", nlohmann::json::array({
                {{"game_id", "earthbound"}, {"title", "EarthBound"}, {"platform", "SNES"}, {"status", "dev"}},
                {{"game_id", "soh-sekailink"}, {"title", "Ocarina of Time SekaiLink Edition"}, {"platform", "PC"}, {"status", "pre-beta"}},
                {{"game_id", "wind-waker"}, {"title", "The Wind Waker"}, {"platform", "GameCube"}, {"status", "mvp"}},
            })},
        }.dump()};
  }
  if (method == "GET" && parts.size() == 2 && (parts[1] == "gamebox" || parts[1] == "boxart")) {
    return {200, nlohmann::json{{"ok", true}, {"boxarts", nlohmann::json::object()}, {"images", nlohmann::json::object()}, {"items", nlohmann::json::array()}}.dump()};
  }
  if (method == "POST" && parts.size() == 2 && (parts[1] == "debug-log" || parts[1] == "bug-report" || parts[1] == "crash-report")) {
    return {200, nlohmann::json{{"ok", true}, {"accepted", true}}.dump()};
  }
  return {404, R"({"ok":false,"error":"not_found"})"};
}

ChatApiHttpResponse ChatApiService::handle_misc_compat(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  (void)raw_path;
  const auto user = legacy_user_from_identity(identity);
  if (parts.size() == 2 && parts[0] == "account" && parts[1] == "profile" && method == "POST") {
    return {200, nlohmann::json{{"ok", true}, {"profile", user}, {"user", user}, {"saved", true}, {"compat", true}}.dump()};
  }
  if (parts.size() == 2 && parts[0] == "auth" && parts[1] == "terms" && (method == "GET" || method == "POST")) {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"accepted", true},
            {"terms_accepted", true},
            {"terms_version", "v1"},
            {"terms_accepted_at", utc_now_iso8601()},
        }.dump()};
  }
  if (parts.size() == 2 && parts[0] == "support" && parts[1] == "tickets" && method == "POST") {
    const auto request = body.value_or(nlohmann::json::object());
    const auto ticket_id = "ticket-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"accepted", true},
            {"ticket_id", ticket_id},
            {"category", sanitized_text(request.value("category", std::string("general")), 40)},
            {"subject", sanitized_text(request.value("subject", std::string("SekaiLink report")), 160)},
        }.dump()};
  }
  if (parts.size() == 2 && parts[0] == "room_status" && method == "GET") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"room_id", url_decode(parts[1])},
            {"status", "pending"},
            {"tracker", ""},
            {"players_total", 0},
            {"checks_done", 0},
            {"total_locations", 0},
            {"completed_players", 0},
        }.dump()};
  }
  if (parts.size() == 2 && parts[0] == "static_tracker" && method == "GET") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"tracker_id", url_decode(parts[1])},
            {"datapackage", nlohmann::json::object()},
            {"player_game", nlohmann::json::array()},
        }.dump()};
  }
  if (parts.size() == 2 && parts[0] == "tracker_view" && method == "GET") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"tracker_id", url_decode(parts[1])},
            {"room_players", nlohmann::json::object()},
            {"player_names", nlohmann::json::array()},
            {"player_games", nlohmann::json::array()},
            {"player_status", nlohmann::json::array()},
            {"player_checks_done", nlohmann::json::array()},
            {"player_locations_total", nlohmann::json::array()},
            {"total_team_locations", nlohmann::json::object()},
            {"total_team_locations_complete", nlohmann::json::object()},
            {"completed_worlds", nlohmann::json::object()},
            {"activity_timers", nlohmann::json::array()},
            {"hints", nlohmann::json::array()},
            {"enabled_trackers", nlohmann::json::array()},
            {"videos", nlohmann::json::object()},
        }.dump()};
  }
  if (parts.size() == 4 && parts[0] == "tracker_player" && method == "GET") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"tracker_id", url_decode(parts[1])},
            {"player_name", "Player " + url_decode(parts[3])},
            {"game", "Generic"},
            {"inventory", nlohmann::json::object()},
            {"locations", nlohmann::json::array()},
            {"checked_locations", nlohmann::json::array()},
            {"received_items", nlohmann::json::object()},
            {"hints", nlohmann::json::array()},
            {"player_names", nlohmann::json::array()},
        }.dump()};
  }
  if (parts.size() == 2 && parts[0] == "sphere_tracker" && method == "GET") {
    return {200, nlohmann::json::array().dump()};
  }
  if (parts.size() == 2 && parts[0] == "datapackage" && method == "GET") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"checksum", url_decode(parts[1])},
            {"item_name_to_id", nlohmann::json::object()},
            {"location_name_to_id", nlohmann::json::object()},
        }.dump()};
  }
  return {404, R"({"ok":false,"error":"not_found"})"};
}

ChatApiHttpResponse ChatApiService::handle_list_messages(const std::string& channel_id) const {
  if (!is_safe_channel_id(channel_id)) return {400, R"({"ok":false,"error":"invalid_channel"})"};
  if (!is_public_channel_allowed(channel_id)) return {403, R"({"ok":false,"error":"channel_forbidden"})"};
  if (config_.sqlite_path.empty()) return forward_to_gateway("GET", "/channels/" + channel_id + "/messages", std::nullopt);

  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_open_failed"})"};
  }
  const char* sql =
      "SELECT id, channel_id, user_id, username, display_name, avatar_url, content, created_at "
      "FROM chat_messages WHERE channel_id = ? ORDER BY id DESC LIMIT 100;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_prepare_failed"})"};
  }
  sqlite3_bind_text(stmt, 1, channel_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<nlohmann::json> reversed;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto id = sqlite3_column_int64(stmt, 0);
    const auto display_name = sqlite_column_string(stmt, 4);
    const auto username = sqlite_column_string(stmt, 3);
    reversed.push_back({
        {"id", id},
        {"channel", sqlite_column_string(stmt, 1)},
        {"user_id", sqlite_column_string(stmt, 2)},
        {"username", username},
        {"display_name", display_name},
        {"author", display_name.empty() ? username : display_name},
        {"avatar_url", sqlite_column_string(stmt, 5)},
        {"content", sqlite_column_string(stmt, 6)},
        {"created_at", sqlite_column_string(stmt, 7)},
        {"kind", "user"},
      });
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  nlohmann::json messages = nlohmann::json::array();
  for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) messages.push_back(*it);
  return {200, nlohmann::json{{"ok", true}, {"channel", channel_id}, {"messages", messages}}.dump()};
}

ChatApiHttpResponse ChatApiService::handle_list_presence(const std::string& channel_id) const {
  if (!is_safe_channel_id(channel_id)) return {400, R"({"ok":false,"error":"invalid_channel"})"};
  if (!is_public_channel_allowed(channel_id)) return {403, R"({"ok":false,"error":"channel_forbidden"})"};
  const auto gateway_presence = forward_to_gateway("GET", "/channels/" + channel_id + "/presence", std::nullopt);
  const auto gateway_json = parse_body_json(gateway_presence);
  if (gateway_presence.status == 200 && gateway_json.value("ok", false) &&
      gateway_json.contains("users") && gateway_json.at("users").is_array() &&
      !gateway_json.at("users").empty()) {
    return {200, gateway_presence.body};
  }
  if (config_.sqlite_path.empty()) {
    return {200, nlohmann::json{{"ok", true}, {"channel", channel_id}, {"users", nlohmann::json::array()}}.dump()};
  }

  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_open_failed"})"};
  }
  const char* sql =
      "SELECT user_id, username, display_name, avatar_url, last_seen "
      "FROM chat_presence "
      "WHERE channel_id = ? AND last_seen >= datetime('now', '-90 seconds') "
      "ORDER BY lower(display_name), lower(username) LIMIT 250;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_prepare_failed"})"};
  }
  sqlite3_bind_text(stmt, 1, channel_id.c_str(), -1, SQLITE_TRANSIENT);
  nlohmann::json users = nlohmann::json::array();
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto username = sqlite_column_string(stmt, 1);
    const auto display_name = sqlite_column_string(stmt, 2);
    users.push_back({
        {"user_id", sqlite_column_string(stmt, 0)},
        {"username", username},
        {"display_name", display_name},
        {"name", display_name.empty() ? username : display_name},
        {"avatar_url", sqlite_column_string(stmt, 3)},
        {"last_seen", sqlite_column_string(stmt, 4)},
        {"status", "online"},
    });
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return {200, nlohmann::json{{"ok", true}, {"channel", channel_id}, {"users", users}}.dump()};
}

ChatApiHttpResponse ChatApiService::touch_presence(
    const std::string& channel_id,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  if (!is_safe_channel_id(channel_id)) return {400, R"({"ok":false,"error":"invalid_channel"})"};
  if (!is_public_channel_allowed(channel_id)) return {403, R"({"ok":false,"error":"channel_forbidden"})"};
  if (config_.sqlite_path.empty()) return {200, nlohmann::json{{"ok", true}, {"channel", channel_id}}.dump()};

  const auto& user = identity.at("user");
  auto user_id = user_id_string(user);
  const auto username = first_string(user, {"username", "email"}, "SekaiLink");
  const auto display_name = first_string(user, {"display_name", "username", "email"}, username);
  const auto avatar_url = avatar_url_from_user(user);
  if (user_id.empty()) user_id = username;
  const auto last_seen = utc_now_iso8601();
  const auto request = body.value_or(nlohmann::json::object());
  auto role = sanitized_text(request.value("role", std::string("player")), 32);
  if (role != "host") role = "player";
  const auto ready = request.value("ready", false);
  const nlohmann::json gateway_body = {
      {"user_id", user_id},
      {"username", username},
      {"display_name", display_name},
      {"avatar_url", avatar_url},
      {"role", role},
      {"ready", ready},
  };
  (void)forward_to_gateway("POST", "/channels/" + channel_id + "/presence", gateway_body);

  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_open_failed"})"};
  }
  const char* sql =
      "INSERT INTO chat_presence (channel_id, user_id, username, display_name, avatar_url, last_seen) "
      "VALUES (?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(channel_id, user_id) DO UPDATE SET "
      "username=excluded.username,"
      "display_name=excluded.display_name,"
      "avatar_url=excluded.avatar_url,"
      "last_seen=excluded.last_seen;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_prepare_failed"})"};
  }
  sqlite3_bind_text(stmt, 1, channel_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, display_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, avatar_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, last_seen.c_str(), -1, SQLITE_TRANSIENT);
  const auto step = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"chat_store_presence_failed"})"};
  return {200, nlohmann::json{{"ok", true}, {"channel", channel_id}, {"last_seen", last_seen}}.dump()};
}

ChatApiHttpResponse ChatApiService::remember_message(
    const std::string& channel_id,
    const nlohmann::json& identity,
    const std::string& content) const {
  if (config_.sqlite_path.empty()) {
    return {200, nlohmann::json{{"ok", true}, {"channel", channel_id}}.dump()};
  }
  const auto& user = identity.at("user");
  const auto username = first_string(user, {"username", "email"}, "SekaiLink");
  const auto display_name = first_string(user, {"display_name", "username", "email"}, username);
  const auto avatar_url = avatar_url_from_user(user);
  const auto user_id = user_id_string(user);
  const auto created_at = utc_now_iso8601();

  std::scoped_lock lock(store_mutex_);
  sqlite3* db = nullptr;
  if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_open_failed"})"};
  }
  const char* sql =
      "INSERT INTO chat_messages (channel_id, user_id, username, display_name, avatar_url, content, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return {500, R"({"ok":false,"error":"chat_store_prepare_failed"})"};
  }
  sqlite3_bind_text(stmt, 1, channel_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, display_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, avatar_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, created_at.c_str(), -1, SQLITE_TRANSIENT);
  const auto step = sqlite3_step(stmt);
  const auto row_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if (step != SQLITE_DONE) return {500, R"({"ok":false,"error":"chat_store_insert_failed"})"};
  return {
      200,
      nlohmann::json{
          {"ok", true},
          {"message", {
              {"id", row_id},
              {"channel", channel_id},
              {"user_id", user_id},
              {"username", username},
              {"display_name", display_name},
              {"author", display_name},
              {"avatar_url", avatar_url},
              {"content", content},
              {"created_at", created_at},
              {"kind", "user"},
          }},
      }.dump()};
}

ChatApiHttpResponse ChatApiService::handle(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token,
    const std::optional<std::string>& device_id,
    const std::optional<nlohmann::json>& body) const {
  const auto clean_path = path_without_query(path);
  if (method == "GET" && clean_path == "/health") {
    return {200, nlohmann::json{{"ok", true}, {"service", "sekailink-chat-api"}}.dump()};
  }
  if (method == "OPTIONS") return {204, ""};
  const auto parts = split_path(clean_path);
  if (!parts.empty() && parts[0] == "client") {
    return handle_client_compat(method, path, parts, body);
  }
  if (!bearer_token.has_value() || bearer_token->empty()) {
    return {401, R"({"ok":false,"error":"unauthorized"})"};
  }
  const auto identity = validate_session(*bearer_token, device_id);
  if (!identity.has_value()) {
    return {401, R"({"ok":false,"error":"unauthorized"})"};
  }
  if (method == "GET" && clean_path == "/me") {
    return handle_legacy_me(*identity);
  }
  if (method == "GET" && clean_path == "/me/profile") {
    const auto legacy_user = legacy_user_from_identity(*identity);
    return {200, nlohmann::json{{"ok", true}, {"profile", legacy_user}, {"user", legacy_user}}.dump()};
  }
  if (method == "GET" && clean_path == "/me/linked-accounts/patreon") {
    const auto user = identity->contains("user") && identity->at("user").is_object()
                          ? identity->at("user")
                          : nlohmann::json::object();
    const auto patreon = user.contains("patreon") && user.at("patreon").is_object()
                             ? user.at("patreon")
                             : nlohmann::json{{"linked", false}, {"tier", nullptr}, {"status", nullptr}, {"is_supporter", false}};
    return {200, nlohmann::json{{"ok", true}, {"patreon", patreon}}.dump()};
  }
  if (method == "POST" && (clean_path == "/me/linked-accounts/patreon/begin" ||
                           clean_path == "/me/linked-accounts/patreon/unlink")) {
#ifdef _WIN32
    return {502, R"({"ok":false,"error":"unsupported_platform"})"};
#else
    const auto upstream = http_request(
        config_.identity_host,
        config_.identity_port,
        "POST",
        clean_path,
        {{"Authorization", "Bearer " + *bearer_token}, {"X-SekaiLink-Client", "chat-api-compat"}},
        body.has_value() ? body->dump() : "{}");
    return upstream;
#endif
  }
  if (method == "POST" && clean_path == "/me/profile") {
#ifdef _WIN32
    return {502, R"({"ok":false,"error":"unsupported_platform"})"};
#else
    const auto upstream = http_request(
        config_.identity_host,
        config_.identity_port,
        "POST",
        "/me/profile",
        {{"Authorization", "Bearer " + *bearer_token}, {"X-SekaiLink-Client", "chat-api-compat"}},
        body.has_value() ? body->dump() : "{}");
    if (upstream.status == 404) return legacy_not_implemented("/me/profile");
    return upstream;
#endif
  }
  if (method == "POST" && clean_path == "/auth/logout") {
    return {200, R"({"ok":true,"legacy":true})"};
  }
  if (parts.size() == 2 && parts[0] == "room" && parts[1] == "rooms" && method == "GET") {
    return handle_legacy_lobbies("GET", std::vector<std::string>{"lobbies"}, *identity, std::nullopt);
  }
  if (!parts.empty() && parts[0] == "lobbies") {
    return handle_legacy_lobbies(method, parts, *identity, body);
  }
  if (!parts.empty() && parts[0] == "social") {
    return handle_social(method, path, parts, *identity, body);
  }
  if (!parts.empty() && (parts[0] == "yamls" || parts[0] == "player-options")) {
    return handle_world_config(method, path, parts, *identity, body);
  }
  if (!parts.empty() && (
      parts[0] == "account" ||
      parts[0] == "auth" ||
      parts[0] == "support" ||
      parts[0] == "room_status" ||
      parts[0] == "static_tracker" ||
      parts[0] == "tracker_view" ||
      parts[0] == "tracker_player" ||
      parts[0] == "sphere_tracker" ||
      parts[0] == "datapackage")) {
    return handle_misc_compat(method, path, parts, *identity, body);
  }
  if (method == "GET" && clean_path == "/channels") {
    return forward_to_gateway(method, clean_path, std::nullopt);
  }
  if (method == "GET" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "messages") {
    return handle_list_messages(url_decode(parts[1]));
  }
  if (method == "GET" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "presence") {
    return handle_list_presence(url_decode(parts[1]));
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "presence") {
    return touch_presence(url_decode(parts[1]), *identity, body);
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "messages") {
    if (!body.has_value() || !body->is_object()) return {400, R"({"ok":false,"error":"invalid_body"})"};
    const auto channel_id = url_decode(parts[1]);
    if (!is_safe_channel_id(channel_id)) return {400, R"({"ok":false,"error":"invalid_channel"})"};
    if (!is_public_channel_allowed(channel_id)) return {403, R"({"ok":false,"error":"channel_forbidden"})"};
    const auto content = sanitized_text(body->value("content", ""), 400);
    if (content.empty()) return {400, R"({"ok":false,"error":"empty_message"})"};
    const auto& user = identity->at("user");
    const auto author = first_string(user, {"display_name", "username", "email"}, "SekaiLink");
    auto user_id = user_id_string(user);
    const auto username = first_string(user, {"username", "email"}, author);
    const auto avatar_url = avatar_url_from_user(user);
    if (user_id.empty()) user_id = username;
    const nlohmann::json gateway_body = {
        {"author", author},
        {"user_id", user_id},
        {"username", username},
        {"display_name", author},
        {"avatar_url", avatar_url},
        {"role", "player"},
        {"ready", false},
        {"content", content},
    };
    const auto relay = forward_to_gateway(method, clean_path, gateway_body);
    if (relay.status < 200 || relay.status >= 300) return relay;
    return remember_message(channel_id, *identity, content);
  }
  return {404, R"({"ok":false,"error":"not_found"})"};
}

ChatApiHttpServer::ChatApiHttpServer(ChatApiServiceConfig config)
    : service_(config), config_(std::move(config)) {}

ChatApiHttpServer::~ChatApiHttpServer() {
  stop();
}

bool ChatApiHttpServer::start() {
#ifdef _WIN32
  return false;
#else
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;
  int reuse = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.http_port);
  if (inet_pton(AF_INET, config_.listen_host.c_str(), &addr.sin_addr) != 1) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (listen(listen_fd_, 64) != 0) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  socket_len_t len = sizeof(addr);
  bound_port_ = getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0 ? ntohs(addr.sin_port) : config_.http_port;
  return true;
#endif
}

void ChatApiHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
  }
#endif
}

std::uint16_t ChatApiHttpServer::port() const {
  return bound_port_;
}

void ChatApiHttpServer::serve_one() const {
#ifndef _WIN32
  if (listen_fd_ < 0) return;
  if (!wait_for_read(listen_fd_, 250)) return;
  sockaddr_in client{};
  socket_len_t len = sizeof(client);
  const int fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client), &len);
  if (fd < 0) return;
  std::string request;
  char buffer[4096];
  const auto n = recv(fd, buffer, sizeof(buffer), 0);
  if (n > 0) request.assign(buffer, static_cast<std::size_t>(n));

  std::istringstream input(request);
  std::string method;
  std::string path;
  std::string version;
  input >> method >> path >> version;
  std::string line;
  std::optional<std::string> bearer;
  std::optional<std::string> device_id;
  std::size_t content_length = 0;
  std::getline(input, line);
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) break;
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    const auto key = lower(trim(line.substr(0, colon)));
    const auto value = trim(line.substr(colon + 1));
    if (key == "authorization") {
      const std::string prefix = "Bearer ";
      if (value.rfind(prefix, 0) == 0) bearer = value.substr(prefix.size());
    } else if (key == "x-sekailink-device-id") {
      device_id = sanitized_text(value, 128);
    } else if (key == "content-length") {
      try {
        content_length = static_cast<std::size_t>(std::stoul(value));
      } catch (...) {
        content_length = 0;
      }
    }
  }
  std::string body_text;
  if (content_length > 0) {
    const auto body_pos = request.find("\r\n\r\n");
    if (body_pos != std::string::npos) body_text = request.substr(body_pos + 4);
    while (body_text.size() < content_length && body_text.size() < 65536) {
      const auto more = recv(fd, buffer, sizeof(buffer), 0);
      if (more <= 0) break;
      body_text.append(buffer, static_cast<std::size_t>(more));
    }
  }
  std::optional<nlohmann::json> body;
  if (!body_text.empty()) {
    try {
      body = nlohmann::json::parse(body_text);
    } catch (...) {
      body = nlohmann::json::object();
    }
  }
  const auto result = service_.handle(method, path, bearer, device_id, body);
  const auto response = result.body.empty() ? raw_response(result.status, "") : raw_response(result.status, result.body);
  write_all(fd, response);
  close_fd(fd);
#endif
}

}  // namespace sekailink_server
