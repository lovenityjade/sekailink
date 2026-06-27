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
    metadata["max_players"] = json_int_or(request, "max_players", 50);
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

  if (parts.size() == 3 && parts[2] == "leave" && method == "POST") {
    (void)forward_to_lobby_runtime(
        "POST",
        "/runtime/lobbies/" + lobby_id + "/presence/leave",
        nlohmann::json{{"username", display_name}, {"user_id", user_id}});
    if (!config_.sqlite_path.empty()) {
      std::scoped_lock lock(store_mutex_);
      sqlite3* db = nullptr;
      if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) == SQLITE_OK && db != nullptr) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM lobby_player_state WHERE lobby_id = ? AND user_id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
          sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_step(stmt);
          sqlite3_finalize(stmt);
        }
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM lobby_game_selections WHERE lobby_id = ? AND user_id = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
          sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_step(stmt);
          sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
      }
    }
    return {200, R"({"ok":true})"};
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
    const auto content = first_string_limited(*body, {"content", "message", "text"}, "", 400);
    if (content.empty()) {
      nlohmann::json keys = nlohmann::json::array();
      for (const auto& [key, value] : body->items()) {
        (void)value;
        keys.push_back(key);
      }
      return {400, nlohmann::json{{"ok", false}, {"error", "empty_message"}, {"body_keys", keys}}.dump()};
    }
    auto avatar_url = chat_avatar_url_from_user(user);
    if (avatar_url.size() > 1024 || avatar_url.rfind("data:", 0) == 0) avatar_url.clear();
    const auto relay = forward_to_gateway(
        "POST",
        "/channels/" + channel_id + "/messages",
        nlohmann::json{
            {"author", display_name},
            {"user_id", user_id},
            {"username", username},
            {"display_name", display_name},
            {"avatar_url", avatar_url},
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
    std::unique_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      return {500, R"({"ok":false,"error":"generation_store_open_failed"})"};
    }
    auto close_db = [&]() {
      if (db != nullptr) {
        sqlite3_close(db);
        db = nullptr;
      }
    };
    ensure_lobby_generation_state_table(db);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT generation_id, status, room_url, error, players_json, response_json, created_at, updated_at "
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
    auto status = sqlite_column_string(stmt, 1);
    auto room_url = sqlite_column_string(stmt, 2);
    auto error = sqlite_column_string(stmt, 3);
    const auto players_json = sqlite_column_string(stmt, 4);
    auto response_json = sqlite_column_string(stmt, 5);
    const auto created_at = sqlite_column_string(stmt, 6);
    auto updated_at = sqlite_column_string(stmt, 7);
    sqlite3_finalize(stmt);
    close_db();
    lock.unlock();
    nlohmann::json players = nlohmann::json::array();
    nlohmann::json generation_response = nlohmann::json::object();
    try {
      players = nlohmann::json::parse(players_json);
      if (!players.is_array()) players = nlohmann::json::array();
    } catch (...) {
      players = nlohmann::json::array();
    }
    try {
      generation_response = nlohmann::json::parse(response_json);
      if (!generation_response.is_object()) generation_response = nlohmann::json::object();
    } catch (...) {
      generation_response = nlohmann::json::object();
    }
    const auto status_running = [](const std::string& value) {
      return value == "queued" || value == "starting" || value == "in_progress" || value == "running";
    };
    const auto status_completed = [](const std::string& value) {
      return value == "completed" || value == "success" || value == "succeeded" ||
             value == "failed" || value == "error";
    };
    const auto response_string = [](const nlohmann::json& data, const char* key) {
      if (!data.contains(key) || !data.at(key).is_string()) return std::string();
      return data.at(key).get<std::string>();
    };
    auto normalized_status = status.empty() ? std::string("idle") : status;
    if (status_running(normalized_status) && !config_.generation_handoff_command.empty()) {
      const auto handoff = run_generation_handoff(
          config_,
          generation_id,
          nlohmann::json{
              {"schema_version", "sekailink-live-generation-request-v1"},
              {"action", "generation_status"},
              {"generation_id", generation_id},
              {"lobby_id", lobby_id},
              {"source", "link-chat-api"},
          });
      if (handoff.ok) {
        bool changed = false;
        if (!handoff.response.empty() && handoff.response != generation_response) {
          generation_response = handoff.response;
          response_json = generation_response.dump();
          changed = true;
        }
        if (!handoff.status.empty() && handoff.status != normalized_status) {
          normalized_status = handoff.status;
          status = handoff.status;
          changed = true;
        }
        if (!handoff.room_url.empty() && handoff.room_url != room_url) {
          room_url = handoff.room_url;
          changed = true;
        }
        if (!handoff.error.empty() && handoff.error != error) {
          error = handoff.error;
          changed = true;
        }
        if (changed && !config_.sqlite_path.empty()) {
          const auto refresh_time = utc_now_iso8601();
          std::unique_lock update_lock(store_mutex_);
          sqlite3* update_db = nullptr;
          if (sqlite3_open(config_.sqlite_path.string().c_str(), &update_db) == SQLITE_OK && update_db != nullptr) {
            sqlite3_stmt* update_stmt = nullptr;
            if (sqlite3_prepare_v2(
                    update_db,
                    "UPDATE lobby_generation_state SET status = ?, room_url = ?, error = ?, response_json = ?, updated_at = ? "
                    "WHERE lobby_id = ? AND generation_id = ?;",
                    -1,
                    &update_stmt,
                    nullptr) == SQLITE_OK) {
              sqlite3_bind_text(update_stmt, 1, normalized_status.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 2, room_url.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 3, error.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 4, response_json.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 5, refresh_time.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 6, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 7, generation_id.c_str(), -1, SQLITE_TRANSIENT);
              (void)sqlite3_step(update_stmt);
              sqlite3_finalize(update_stmt);
              updated_at = refresh_time;
            }
            sqlite3_close(update_db);
          } else if (update_db != nullptr) {
            sqlite3_close(update_db);
          }
        }
      }
    }
    const auto completed = status_completed(normalized_status);
    const auto running = status_running(normalized_status);
    const auto response_array = [](const nlohmann::json& data, const char* key) {
      if (!data.contains(key) || !data.at(key).is_array()) return nlohmann::json::array();
      return data.at(key);
    };
    const auto artifact_path = response_string(generation_response, "artifact_path");
    const auto output_dir = response_string(generation_response, "output_dir");
    auto sync_package_path = response_string(generation_response, "sync_package_path");
    if (sync_package_path.empty()) sync_package_path = artifact_path;
    const auto sync_entries = response_array(generation_response, "sync_entries");
    const auto launch_entries = response_array(generation_response, "launch_entries");
    if (room_url.empty() && !sync_package_path.empty()) room_url = "/rooms/" + lobby_id;
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
            {"artifact_path", artifact_path},
            {"artifact_ready", !artifact_path.empty()},
            {"sync_package_path", sync_package_path},
            {"sync_package_ready", !sync_package_path.empty()},
            {"sync_entries", sync_entries},
            {"launch_entries", launch_entries},
            {"room_status", response_string(generation_response, "room_status")},
            {"room_host", response_string(generation_response, "room_host")},
            {"room_runtime_log", response_string(generation_response, "room_runtime_log")},
            {"room_runtime_error", response_string(generation_response, "room_runtime_error")},
            {"room_runtime_alive", generation_response.value("room_runtime_alive", false)},
            {"room_runtime_pid", generation_response.value("room_runtime_pid", 0)},
            {"output_dir", output_dir},
            {"players", players},
            {"response", generation_response},
            {"created_at", created_at},
            {"updated_at", updated_at},
        }.dump()};
  }
  if (parts.size() == 4 && parts[2] == "generation" && parts[3] == "reset" && method == "POST") {
    if (config_.sqlite_path.empty()) {
      return {200, nlohmann::json{{"ok", true}, {"status", "idle"}, {"reset", false}}.dump()};
    }
    std::unique_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      return {500, R"({"ok":false,"error":"generation_store_open_failed"})"};
    }
    auto close_db = [&]() {
      if (db != nullptr) {
        sqlite3_close(db);
        db = nullptr;
      }
    };
    ensure_lobby_generation_state_table(db);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT generation_id, status, room_url, error, response_json FROM lobby_generation_state WHERE lobby_id = ? LIMIT 1;",
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
      return {200, nlohmann::json{{"ok", true}, {"status", "idle"}, {"reset", false}}.dump()};
    }
    const auto generation_id = sqlite_column_string(stmt, 0);
    const auto status = sqlite_column_string(stmt, 1);
    const auto room_url = sqlite_column_string(stmt, 2);
    const auto error = sqlite_column_string(stmt, 3);
    const auto response_json = sqlite_column_string(stmt, 4);
    sqlite3_finalize(stmt);

    nlohmann::json generation_response = nlohmann::json::object();
    try {
      generation_response = nlohmann::json::parse(response_json);
      if (!generation_response.is_object()) generation_response = nlohmann::json::object();
    } catch (...) {
      generation_response = nlohmann::json::object();
    }
    const auto response_string = [](const nlohmann::json& data, const char* key) {
      if (!data.contains(key) || !data.at(key).is_string()) return std::string();
      return data.at(key).get<std::string>();
    };
    const auto artifact_path = response_string(generation_response, "artifact_path");
    auto sync_package_path = response_string(generation_response, "sync_package_path");
    if (sync_package_path.empty()) sync_package_path = artifact_path;
    const auto running = status == "queued" || status == "starting" || status == "in_progress" || status == "running";
    const auto successful = status == "completed" || status == "success" || status == "succeeded" ||
                            !room_url.empty() || !sync_package_path.empty();
    const auto failed = status == "failed" || status == "error" || status == "cancelled" || !error.empty() ||
                        generation_response.value("ok", true) == false;
    if (running || successful || !failed) {
      close_db();
      return {
          409,
          nlohmann::json{
              {"ok", false},
              {"error", "generation_reset_refused"},
              {"status", status},
              {"generation_id", generation_id},
              {"message", "Only failed generation states can be reset."},
          }.dump()};
    }

    sqlite3_stmt* delete_stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM lobby_generation_state WHERE lobby_id = ?;", -1, &delete_stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_reset_prepare_failed"})"};
    }
    sqlite3_bind_text(delete_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
    const auto delete_step = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    if (delete_step != SQLITE_DONE) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_reset_failed"})"};
    }
    close_db();
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"reset", true},
            {"status", "idle"},
            {"lobby_id", lobby_id},
            {"generation_id", generation_id},
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
    std::unique_lock lock(store_mutex_);
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

    std::unique_lock lock(store_mutex_);
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
      ensure_lobby_generation_state_table(db);
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
          locked = status == "queued" || status == "in_progress" || status == "running" ||
                   status == "generating" || status == "processing" || status == "done" ||
                   status == "completed" || status == "success" || status == "succeeded" ||
                   !room_url.empty();
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

    auto seed_game_display_name = [&](const std::string& game_key, const std::string& fallback) {
      if (config_.seed_config_user_token.empty() || game_key.empty()) return fallback;
      const auto upstream = forward_to_seed_config("GET", "/seed-configs/games", std::nullopt);
      const auto parsed = parse_body_json(upstream);
      const auto status = response_status_from_body(upstream, parsed);
      if (status != 200 || !parsed.value("ok", false) || !parsed.contains("games") || !parsed.at("games").is_array()) {
        return fallback;
      }
      for (const auto& game : parsed.at("games")) {
        if (!game.is_object()) continue;
        if (first_string_limited(game, {"game_key"}, "", 120) != game_key) continue;
        return first_string_limited(game, {"display_name", "name", "game"}, fallback, 120);
      }
      return fallback;
    };

    auto load_config = [&](const std::string& yaml_id) -> std::optional<nlohmann::json> {
      if (!config_.seed_config_user_token.empty() && is_decimal_id_string(user_id)) {
        const auto upstream = forward_to_seed_config("GET", "/users/" + user_id + "/seed-configs", std::nullopt);
        const auto parsed = parse_body_json(upstream);
        const auto status = response_status_from_body(upstream, parsed);
        if (status == 200 && parsed.value("ok", false) && parsed.contains("configs") && parsed.at("configs").is_array()) {
          for (const auto& config : parsed.at("configs")) {
            if (!config.is_object()) continue;
            const auto config_id = json_id_or(config, "config_id", json_id_or(config, "id"));
            if (config_id != yaml_id) continue;
            const auto version_id = json_id_or(config, "version_id");
            const auto game_key = first_string_limited(config, {"game_key"}, "", 120);
            auto game_display_name = first_string_limited(config, {"game_display_name", "game"}, game_key.empty() ? std::string("unknown") : game_key, 120);
            game_display_name = seed_game_display_name(game_key, game_display_name);
            if (game_display_name.empty()) game_display_name = game_key.empty() ? std::string("unknown") : game_key;
            nlohmann::json result = {
                {"id", config_id},
                {"config_id", config_id},
                {"yaml_id", config_id},
                {"title", first_string_limited(config, {"name", "title"}, config_id, 120)},
                {"name", first_string_limited(config, {"name", "title"}, config_id, 120)},
                {"game", game_display_name},
                {"game_key", game_key},
                {"game_display_name", game_display_name},
                {"linkedworld_id", game_key.empty() ? linkedworld_id_from_game(game_display_name) : linkedworld_id_from_game(game_key)},
                {"player_name", display_name},
                {"custom", true},
                {"source", "nexus.seed_config_api"},
            };
            if (!version_id.empty()) result["version_id"] = version_id;
            if (config.contains("values_hash") && config.at("values_hash").is_string()) result["values_hash"] = config.at("values_hash");
            return result;
          }
        }
      }
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
    const auto close_status = response_status_from_body(upstream, parsed);
    if (close_status >= 200 && close_status < 300 &&
        !config_.generation_handoff_command.empty() && !config_.sqlite_path.empty()) {
      std::string generation_id;
      {
        std::unique_lock lock(store_mutex_);
        sqlite3* db = nullptr;
        if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) == SQLITE_OK && db != nullptr) {
          ensure_lobby_generation_state_table(db);
          sqlite3_stmt* stmt = nullptr;
          if (sqlite3_prepare_v2(
                  db,
                  "SELECT generation_id FROM lobby_generation_state WHERE lobby_id = ? LIMIT 1;",
                  -1,
                  &stmt,
                  nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) generation_id = sqlite_column_string(stmt, 0);
            sqlite3_finalize(stmt);
          }
          sqlite3_close(db);
        }
      }
      if (!generation_id.empty()) {
        const auto handoff = run_generation_handoff(
            config_,
            generation_id,
            nlohmann::json{
                {"schema_version", "sekailink-live-generation-request-v1"},
                {"action", "stop_generation"},
                {"generation_id", generation_id},
                {"lobby_id", lobby_id},
                {"source", "link-chat-api"},
            });
        if (handoff.ok && handoff.response.is_object() && !handoff.response.empty()) {
          std::unique_lock lock(store_mutex_);
          sqlite3* db = nullptr;
          if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) == SQLITE_OK && db != nullptr) {
            sqlite3_stmt* update_stmt = nullptr;
            if (sqlite3_prepare_v2(
                    db,
                    "UPDATE lobby_generation_state SET room_url = '', response_json = ?, updated_at = ? "
                    "WHERE lobby_id = ? AND generation_id = ?;",
                    -1,
                    &update_stmt,
                    nullptr) == SQLITE_OK) {
              const auto now = utc_now_iso8601();
              const auto response_dump = handoff.response.dump();
              sqlite3_bind_text(update_stmt, 1, response_dump.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 3, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(update_stmt, 4, generation_id.c_str(), -1, SQLITE_TRANSIENT);
              (void)sqlite3_step(update_stmt);
              sqlite3_finalize(update_stmt);
            }
            sqlite3_close(db);
          }
        }
      }
    }
    return {close_status, upstream.body};
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

    std::unique_lock lock(store_mutex_);
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
        "CREATE TABLE IF NOT EXISTS world_configs ("
        "id TEXT PRIMARY KEY,"
        "user_id TEXT NOT NULL,"
        "title TEXT NOT NULL,"
        "game TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ");");
    ensure_lobby_generation_state_table(db);

    sqlite3_stmt* existing_generation_stmt = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT generation_id, status, room_url, error, players_json, response_json, created_at, updated_at "
            "FROM lobby_generation_state WHERE lobby_id = ? LIMIT 1;",
            -1,
            &existing_generation_stmt,
            nullptr) == SQLITE_OK) {
      sqlite3_bind_text(existing_generation_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(existing_generation_stmt) == SQLITE_ROW) {
        const auto existing_generation_id = sqlite_column_string(existing_generation_stmt, 0);
        const auto existing_status = sqlite_column_string(existing_generation_stmt, 1);
        const auto existing_room_url = sqlite_column_string(existing_generation_stmt, 2);
        const auto existing_error = sqlite_column_string(existing_generation_stmt, 3);
        const auto existing_players_json = sqlite_column_string(existing_generation_stmt, 4);
        const auto existing_response_json = sqlite_column_string(existing_generation_stmt, 5);
        const auto existing_created_at = sqlite_column_string(existing_generation_stmt, 6);
        const auto existing_updated_at = sqlite_column_string(existing_generation_stmt, 7);
        sqlite3_finalize(existing_generation_stmt);
        nlohmann::json existing_response = nlohmann::json::object();
        nlohmann::json existing_players = nlohmann::json::array();
        try {
          existing_response = nlohmann::json::parse(existing_response_json);
          if (!existing_response.is_object()) existing_response = nlohmann::json::object();
        } catch (...) {
          existing_response = nlohmann::json::object();
        }
        try {
          existing_players = nlohmann::json::parse(existing_players_json);
          if (!existing_players.is_array()) existing_players = nlohmann::json::array();
        } catch (...) {
          existing_players = nlohmann::json::array();
        }
        const auto existing_response_string = [](const nlohmann::json& data, const char* key) {
          if (!data.contains(key) || !data.at(key).is_string()) return std::string();
          return data.at(key).get<std::string>();
        };
        const auto existing_response_array = [](const nlohmann::json& data, const char* key) {
          if (!data.contains(key) || !data.at(key).is_array()) return nlohmann::json::array();
          return data.at(key);
        };
        const auto existing_artifact_path = existing_response_string(existing_response, "artifact_path");
        auto existing_sync_package_path = existing_response_string(existing_response, "sync_package_path");
        if (existing_sync_package_path.empty()) existing_sync_package_path = existing_artifact_path;
        const auto existing_running =
            existing_status == "queued" || existing_status == "starting" ||
            existing_status == "in_progress" || existing_status == "running";
        const auto existing_completed =
            existing_status == "completed" || existing_status == "success" || existing_status == "succeeded";
        if (existing_running || existing_completed || !existing_room_url.empty() || !existing_sync_package_path.empty()) {
          close_db();
          return {
              202,
              nlohmann::json{
                  {"ok", true},
                  {"lobby_id", lobby_id},
                  {"generation_id", existing_generation_id},
                  {"status", existing_status},
                  {"running", existing_running},
                  {"completed", existing_completed},
                  {"room_url", existing_room_url.empty() && !existing_sync_package_path.empty() ? "/rooms/" + lobby_id : existing_room_url},
                  {"error", existing_error},
                  {"artifact_path", existing_artifact_path},
                  {"artifact_ready", !existing_artifact_path.empty()},
                  {"sync_package_path", existing_sync_package_path},
                  {"sync_package_ready", !existing_sync_package_path.empty()},
                  {"sync_entries", existing_response_array(existing_response, "sync_entries")},
                  {"launch_entries", existing_response_array(existing_response, "launch_entries")},
                  {"room_status", existing_response_string(existing_response, "room_status")},
                  {"room_host", existing_response_string(existing_response, "room_host")},
                  {"room_runtime_log", existing_response_string(existing_response, "room_runtime_log")},
                  {"room_runtime_error", existing_response_string(existing_response, "room_runtime_error")},
                  {"room_runtime_alive", existing_response.value("room_runtime_alive", false)},
                  {"room_runtime_pid", existing_response.value("room_runtime_pid", 0)},
                  {"output_dir", existing_response_string(existing_response, "output_dir")},
                  {"locked", true},
                  {"existing_generation", true},
                  {"players", existing_players},
                  {"response", existing_response},
                  {"created_at", existing_created_at},
                  {"updated_at", existing_updated_at},
                  {"message", "Generation is already active for this lobby."},
              }.dump()};
        }
      } else {
        sqlite3_finalize(existing_generation_stmt);
      }
    }

    nlohmann::json players = nlohmann::json::array();
    nlohmann::json generation_players = nlohmann::json::array();
    auto seed_game_display_name = [&](const std::string& game_key, const std::string& fallback) {
      if (config_.seed_config_user_token.empty() || game_key.empty()) return fallback;
      const auto upstream = forward_to_seed_config("GET", "/seed-configs/games", std::nullopt);
      const auto parsed = parse_body_json(upstream);
      const auto status = response_status_from_body(upstream, parsed);
      if (status != 200 || !parsed.value("ok", false) || !parsed.contains("games") || !parsed.at("games").is_array()) {
        return fallback;
      }
      for (const auto& game : parsed.at("games")) {
        if (!game.is_object()) continue;
        if (first_string_limited(game, {"game_key"}, "", 120) != game_key) continue;
        return first_string_limited(game, {"display_name", "name", "game"}, fallback, 120);
      }
      return fallback;
    };
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
      const auto flat_selections = flatten_lobby_game_selections(selections);
      if (flat_selections.empty()) {
        close_db();
        return {409, nlohmann::json{{"ok", false}, {"error", "player_missing_game_selection"}, {"player", member_name}}.dump()};
      }
      nlohmann::json generation_configs = nlohmann::json::array();
      for (const auto& selected : flat_selections) {
        const auto config_id = json_id_or(selected, "config_id", json_id_or(selected, "yaml_id", json_id_or(selected, "id")));
        if (config_id.empty()) {
          close_db();
          return {409, nlohmann::json{{"ok", false}, {"error", "generation_config_id_missing"}, {"player", member_name}}.dump()};
        }
        auto title = first_string_limited(selected, {"title", "name"}, config_id, 120);
        auto game_key = first_string_limited(selected, {"game_key"}, "", 120);
        auto game_display_name = first_string_limited(selected, {"game_display_name", "game"}, game_key.empty() ? std::string("unknown") : game_key, 120);
        game_display_name = seed_game_display_name(game_key, game_display_name);
        if (game_display_name.empty()) game_display_name = game_key.empty() ? std::string("unknown") : game_key;
        const auto version_id = json_id_or(selected, "version_id");
        std::string content;
        std::string source = "legacy.world_configs";
        if (!config_.seed_config_user_token.empty() && is_decimal_id_string(member_user_id)) {
          const auto exported = forward_to_seed_config(
              "POST",
              "/users/" + member_user_id + "/seed-configs/" + config_id + "/export-yaml",
              std::nullopt);
          const auto exported_json = parse_body_json(exported);
          const auto export_status = response_status_from_body(exported, exported_json);
          if (export_status == 200 && exported_json.value("ok", false) &&
              exported_json.contains("yaml") && exported_json.at("yaml").is_string()) {
            content = exported_json.at("yaml").get<std::string>();
            source = "nexus.seed_config_api";
            const auto exported_game_key = first_string_limited(exported_json, {"game_key"}, "", 120);
            if (game_key.empty()) game_key = exported_game_key;
            game_display_name = seed_game_display_name(game_key, game_display_name);
          }
        }
        if (content.empty()) {
          sqlite3_stmt* config_stmt = nullptr;
          if (sqlite3_prepare_v2(
                  db,
                  "SELECT title, game, content FROM world_configs WHERE user_id = ? AND id = ?;",
                  -1,
                  &config_stmt,
                  nullptr) != SQLITE_OK) {
            close_db();
            return {500, R"({"ok":false,"error":"generation_config_prepare_failed"})"};
          }
          sqlite3_bind_text(config_stmt, 1, member_user_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(config_stmt, 2, config_id.c_str(), -1, SQLITE_TRANSIENT);
          if (sqlite3_step(config_stmt) == SQLITE_ROW) {
            title = sqlite_column_string(config_stmt, 0);
            game_display_name = sqlite_column_string(config_stmt, 1);
            content = sqlite_column_string(config_stmt, 2);
          }
          sqlite3_finalize(config_stmt);
        }
        if (content.empty()) {
          close_db();
          return {409, nlohmann::json{{"ok", false}, {"error", "generation_config_content_missing"}, {"player", member_name}, {"config_id", config_id}}.dump()};
        }
        nlohmann::json generation_config = {
            {"config_id", config_id},
            {"title", title},
            {"game", game_display_name},
            {"game_key", game_key},
            {"game_display_name", game_display_name},
            {"content", content},
            {"source", source},
        };
        if (!version_id.empty()) generation_config["version_id"] = version_id;
        if (selected.contains("values_hash") && selected.at("values_hash").is_string()) {
          generation_config["values_hash"] = selected.at("values_hash");
        }
        generation_configs.push_back(generation_config);
      }
      players.push_back({{"user_id", member_user_id}, {"username", member_name}, {"selections", selections}});
      generation_players.push_back({
          {"user_id", member_user_id},
          {"username", member_name},
          {"selections", selections},
          {"configs", generation_configs},
      });
    }

    const auto job_id = lobby_id + "-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    const auto now = utc_now_iso8601();
    std::string generation_status = "queued";
    std::string room_url;
    std::string generation_error;
    nlohmann::json generation_response = nlohmann::json::object();
    if (!config_.generation_handoff_command.empty()) {
      const nlohmann::json handoff_request = {
          {"schema_version", "sekailink-live-generation-request-v1"},
          {"action", "submit_generation"},
          {"generation_id", job_id},
          {"lobby_id", lobby_id},
          {"sync_id", lobby_id},
          {"created_at", now},
          {"source", "link-chat-api"},
          {"players", generation_players},
      };
      close_db();
      lock.unlock();
      const auto handoff = run_generation_handoff(config_, job_id, handoff_request);
      if (!handoff.ok) {
        return {
            502,
            nlohmann::json{
                {"ok", false},
                {"error", "generation_handoff_failed"},
                {"detail", handoff.error},
            }.dump()};
      }
      if (!handoff.status.empty()) generation_status = handoff.status;
      room_url = handoff.room_url;
      generation_error = handoff.error;
      if (handoff.response.is_object()) generation_response = handoff.response;
      lock.lock();
      if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) sqlite3_close(db);
        return {500, R"({"ok":false,"error":"generation_store_open_failed"})"};
      }
    }
    sqlite3_stmt* generation_stmt = nullptr;
    const char* generation_sql =
        "INSERT INTO lobby_generation_state (lobby_id, generation_id, status, room_url, error, players_json, response_json, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(lobby_id) DO UPDATE SET generation_id=excluded.generation_id, status=excluded.status, room_url=excluded.room_url, "
        "error=excluded.error, players_json=excluded.players_json, response_json=excluded.response_json, updated_at=excluded.updated_at;";
    if (sqlite3_prepare_v2(db, generation_sql, -1, &generation_stmt, nullptr) != SQLITE_OK) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_state_prepare_failed"})"};
    }
    const auto players_dump = players.dump();
    const auto response_dump = generation_response.dump();
    sqlite3_bind_text(generation_stmt, 1, lobby_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 2, job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 3, generation_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 4, room_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 5, generation_error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 6, players_dump.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 7, response_dump.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(generation_stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto generation_step = sqlite3_step(generation_stmt);
    sqlite3_finalize(generation_stmt);
    if (generation_step != SQLITE_DONE) {
      close_db();
      return {500, R"({"ok":false,"error":"generation_state_save_failed"})"};
    }
    close_db();
    const auto response_string = [](const nlohmann::json& data, const char* key) {
      if (!data.contains(key) || !data.at(key).is_string()) return std::string();
      return data.at(key).get<std::string>();
    };
    const auto response_array = [](const nlohmann::json& data, const char* key) {
      if (!data.contains(key) || !data.at(key).is_array()) return nlohmann::json::array();
      return data.at(key);
    };
    const auto artifact_path = response_string(generation_response, "artifact_path");
    const auto output_dir = response_string(generation_response, "output_dir");
    auto sync_package_path = response_string(generation_response, "sync_package_path");
    if (sync_package_path.empty()) sync_package_path = artifact_path;
    const auto sync_entries = response_array(generation_response, "sync_entries");
    const auto launch_entries = response_array(generation_response, "launch_entries");
    if (room_url.empty() && !sync_package_path.empty()) room_url = "/rooms/" + lobby_id;
    return {
        202,
        nlohmann::json{
            {"ok", true},
            {"lobby_id", lobby_id},
            {"generation_id", job_id},
            {"status", generation_status},
            {"running", generation_status == "queued" || generation_status == "starting" || generation_status == "in_progress" || generation_status == "running"},
            {"completed", generation_status == "completed" || generation_status == "success" || generation_status == "succeeded"},
            {"room_url", room_url},
            {"error", generation_error},
            {"artifact_path", artifact_path},
            {"artifact_ready", !artifact_path.empty()},
            {"sync_package_path", sync_package_path},
            {"sync_package_ready", !sync_package_path.empty()},
            {"sync_entries", sync_entries},
            {"launch_entries", launch_entries},
            {"room_status", response_string(generation_response, "room_status")},
            {"room_host", response_string(generation_response, "room_host")},
            {"room_runtime_log", response_string(generation_response, "room_runtime_log")},
            {"room_runtime_error", response_string(generation_response, "room_runtime_error")},
            {"room_runtime_alive", generation_response.value("room_runtime_alive", false)},
            {"room_runtime_pid", generation_response.value("room_runtime_pid", 0)},
            {"output_dir", output_dir},
            {"locked", true},
            {"players", players},
            {"response", generation_response},
            {"created_at", now},
            {"updated_at", now},
            {"message", "Generation request queued for live Worlds orchestration."},
        }.dump()};
  }

  return {404, R"({"ok":false,"error":"not_found"})"};
}
