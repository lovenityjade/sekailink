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
        {"status", "offline"},
        {"presence_status", "offline"},
        {"is_online", false},
        {"last_seen", ""},
    };
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT p.username, p.display_name, p.avatar_url, p.updated_at, COALESCE(s.presence_status, 'offline'), "
        "COALESCE(strftime('%s', 'now') - strftime('%s', p.updated_at), 999999) "
        "FROM social_profiles p LEFT JOIN social_settings s ON s.user_id = p.user_id WHERE p.user_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto username = sqlite_column_string(stmt, 0);
        const auto display_name = sqlite_column_string(stmt, 1);
        const auto last_seen = sqlite_column_string(stmt, 3);
        const auto preferred_status = lower(sqlite_column_string(stmt, 4));
        const auto age_seconds = sqlite3_column_int64(stmt, 5);
        const auto active_recently = age_seconds >= 0 && age_seconds <= 45;
        const auto visible_status = active_recently && preferred_status != "offline" && preferred_status != "appear-offline" ? preferred_status : "offline";
        out["username"] = username;
        out["display_name"] = display_name;
        out["name"] = display_name.empty() ? username : display_name;
        out["avatar_url"] = sqlite_column_string(stmt, 2);
        out["last_seen"] = last_seen;
        out["status"] = visible_status;
        out["presence_status"] = visible_status;
        out["is_online"] = visible_status != "offline";
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
  if (method == "GET" && parts.size() == 2 && parts[1] == "unread-by-user") {
    nlohmann::json unread = nlohmann::json::array();
    nlohmann::json unread_by_user = nlohmann::json::object();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT from_id, COUNT(*) FROM social_dm_messages "
        "WHERE to_id = ? AND read_at IS NULL GROUP BY from_id ORDER BY COUNT(*) DESC, from_id LIMIT 250;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto from_id = sqlite_column_string(stmt, 0);
        const auto count = sqlite3_column_int(stmt, 1);
        unread.push_back({{"user_id", from_id}, {"from_id", from_id}, {"count", count}, {"unread_count", count}});
        unread_by_user[from_id] = count;
      }
      sqlite3_finalize(stmt);
    }
    close_db();
    return {200, nlohmann::json{{"ok", true}, {"unread", unread}, {"unread_by_user", unread_by_user}}.dump()};
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
    const auto moderated_content = moderate_user_text(
        body.value_or(nlohmann::json::object()).value("content", std::string{}),
        1000,
        ModerationMode::Chat);
    if (!moderated_content.ok) {
      close_db();
      return {400, moderation_error_json(moderated_content).dump()};
    }
    const auto content = moderated_content.text;
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
          "SELECT p.user_id, p.username, p.display_name, p.avatar_url, p.updated_at, COALESCE(s.presence_status, 'offline'), "
          "COALESCE(strftime('%s', 'now') - strftime('%s', p.updated_at), 999999) "
          "FROM social_profiles p LEFT JOIN social_settings s ON s.user_id = p.user_id "
          "WHERE p.user_id != ? AND (lower(p.username) LIKE ? OR lower(p.display_name) LIKE ?) "
          "ORDER BY lower(p.display_name), lower(p.username) LIMIT 20;";
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        const auto pattern = "%" + q + "%";
        sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          const auto username = sqlite_column_string(stmt, 1);
          const auto display_name = sqlite_column_string(stmt, 2);
          const auto preferred_status = lower(sqlite_column_string(stmt, 5));
          const auto age_seconds = sqlite3_column_int64(stmt, 6);
          const auto active_recently = age_seconds >= 0 && age_seconds <= 45;
          const auto visible_status = active_recently && preferred_status != "offline" && preferred_status != "appear-offline" ? preferred_status : "offline";
          users.push_back({
              {"user_id", sqlite_column_string(stmt, 0)},
              {"username", username},
              {"display_name", display_name},
              {"name", display_name.empty() ? username : display_name},
              {"avatar_url", sqlite_column_string(stmt, 3)},
              {"last_seen", sqlite_column_string(stmt, 4)},
              {"status", visible_status},
              {"presence_status", visible_status},
              {"is_online", visible_status != "offline"},
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
