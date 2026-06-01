#include "sekailink_server/chat_api_service.hpp"

#include <sqlite3.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
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
#include <system_error>
#include <vector>

namespace sekailink_server {
namespace {

#include "chat_api_internal_helpers.inc"

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
  if (data.contains("seed_config_host")) config.seed_config_host = data.at("seed_config_host").get<std::string>();
  if (data.contains("seed_config_port")) config.seed_config_port = static_cast<std::uint16_t>(data.at("seed_config_port").get<int>());
  if (data.contains("seed_config_user_token")) config.seed_config_user_token = trim(data.at("seed_config_user_token").get<std::string>());
  if (data.contains("generation_handoff_root") && !data.at("generation_handoff_root").is_null()) {
    config.generation_handoff_root = data.at("generation_handoff_root").get<std::string>();
  }
  if (data.contains("generation_handoff_command") && data.at("generation_handoff_command").is_array()) {
    config.generation_handoff_command = data.at("generation_handoff_command").get<std::vector<std::string>>();
  }
  if (data.contains("sqlite_path")) config.sqlite_path = data.at("sqlite_path").get<std::string>();
  if (data.contains("client_release_manifest_path")) config.client_release_manifest_path = data.at("client_release_manifest_path").get<std::string>();
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
      {"seed_config_host", config.seed_config_host},
      {"seed_config_port", config.seed_config_port},
      {"seed_config_configured", !config.seed_config_user_token.empty()},
      {"generation_handoff_configured", !config.generation_handoff_command.empty()},
      {"generation_handoff_root_configured", !config.generation_handoff_root.empty()},
      {"sqlite_configured", !config.sqlite_path.empty()},
      {"client_release_manifest_configured", !config.client_release_manifest_path.empty()},
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

ChatApiHttpResponse ChatApiService::forward_to_seed_config(
    const std::string& method,
    const std::string& path,
    const std::optional<nlohmann::json>& body) const {
#ifdef _WIN32
  (void)method;
  (void)path;
  (void)body;
  return {502, R"({"ok":false,"error":"unsupported_platform"})"};
#else
  if (config_.seed_config_user_token.empty()) {
    return {502, R"({"ok":false,"error":"seed_config_token_missing"})"};
  }
  return http_request(
      config_.seed_config_host,
      config_.seed_config_port,
      method,
      path,
      {{"Authorization", "Bearer " + config_.seed_config_user_token}, {"X-SekaiLink-Client", "chat-api-live"}},
      body.has_value() ? body->dump() : "");
#endif
}

ChatApiHttpResponse ChatApiService::handle_legacy_me(const nlohmann::json& identity) const {
  return {200, legacy_user_from_identity(identity).dump()};
}

#include "chat_api_lobby_routes.inc"

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

#include "chat_api_social_routes.inc"

#include "chat_api_world_config_routes.inc"

#include "chat_api_client_compat_routes.inc"

#include "chat_api_misc_compat_routes.inc"

#include "chat_api_chat_routes.inc"

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
  if (!parts.empty() && (parts[0] == "yamls" || parts[0] == "player-options" || parts[0] == "seed-configs" || parts[0] == "pulse")) {
    return handle_world_config(method, path, parts, *identity, body);
  }
  if (!parts.empty() && (
      parts[0] == "account" ||
      parts[0] == "auth" ||
      parts[0] == "support" ||
      parts[0] == "room_status" ||
      parts[0] == "generation_artifacts" ||
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
    const auto content = first_string_limited(*body, {"content", "message", "text"}, "", 400);
    if (content.empty()) {
      nlohmann::json keys = nlohmann::json::array();
      for (const auto& [key, value] : body->items()) {
        (void)value;
        keys.push_back(key);
      }
      return {400, nlohmann::json{{"ok", false}, {"error", "empty_message"}, {"body_keys", keys}}.dump()};
    }
    const auto& user = identity->at("user");
    const auto author = first_string(user, {"display_name", "username", "email"}, "SekaiLink");
    auto user_id = user_id_string(user);
    const auto username = first_string(user, {"username", "email"}, author);
    auto avatar_url = chat_avatar_url_from_user(user);
    if (avatar_url.size() > 1024 || avatar_url.rfind("data:", 0) == 0) avatar_url.clear();
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
