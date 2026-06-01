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
  const auto avatar_url = chat_avatar_url_from_user(user);
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
  const auto avatar_url = chat_avatar_url_from_user(user);
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
