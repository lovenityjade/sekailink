ChatApiHttpResponse ChatApiService::handle_misc_compat(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  (void)raw_path;
  const auto user = legacy_user_from_identity(identity);
  const auto parse_object_or_empty = [](const std::string& text) {
    try {
      auto parsed = nlohmann::json::parse(text.empty() ? "{}" : text);
      return parsed.is_object() ? parsed : nlohmann::json::object();
    } catch (...) {
      return nlohmann::json::object();
    }
  };
  const auto parse_array_or_empty = [](const std::string& text) {
    try {
      auto parsed = nlohmann::json::parse(text.empty() ? "[]" : text);
      return parsed.is_array() ? parsed : nlohmann::json::array();
    } catch (...) {
      return nlohmann::json::array();
    }
  };
  const auto json_string = [](const nlohmann::json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) return std::string();
    return object.at(key).get<std::string>();
  };
  const auto json_int = [](const nlohmann::json& object, const std::vector<const char*>& keys) {
    if (!object.is_object()) return 0;
    for (const auto* key : keys) {
      if (!object.contains(key)) continue;
      const auto& value = object.at(key);
      if (value.is_number_integer()) return value.get<int>();
      if (value.is_string()) {
        try {
          return std::stoi(value.get<std::string>());
        } catch (...) {
        }
      }
    }
    return 0;
  };
  const auto json_array = [](const nlohmann::json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_array()) return nlohmann::json::array();
    return object.at(key);
  };
  const auto load_generation_state = [&](const std::string& lookup_id) {
    nlohmann::json state = {
        {"ok", false},
        {"error", "generation_state_not_found"},
    };
    if (config_.sqlite_path.empty() || lookup_id.empty()) return state;
    std::unique_lock lock(store_mutex_);
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
      if (db) sqlite3_close(db);
      state["error"] = "generation_store_open_failed";
      return state;
    }
    ensure_lobby_generation_state_table(db);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT lobby_id, generation_id, status, room_url, error, players_json, response_json, created_at, updated_at "
            "FROM lobby_generation_state WHERE lobby_id = ? OR generation_id = ? LIMIT 1;",
            -1,
            &stmt,
            nullptr) != SQLITE_OK) {
      sqlite3_close(db);
      state["error"] = "generation_state_prepare_failed";
      return state;
    }
    sqlite3_bind_text(stmt, 1, lookup_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, lookup_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return state;
    }
    state = {
        {"ok", true},
        {"lobby_id", sqlite_column_string(stmt, 0)},
        {"generation_id", sqlite_column_string(stmt, 1)},
        {"status", sqlite_column_string(stmt, 2)},
        {"room_url", sqlite_column_string(stmt, 3)},
        {"error", sqlite_column_string(stmt, 4)},
        {"players", parse_array_or_empty(sqlite_column_string(stmt, 5))},
        {"response", parse_object_or_empty(sqlite_column_string(stmt, 6))},
        {"created_at", sqlite_column_string(stmt, 7)},
        {"updated_at", sqlite_column_string(stmt, 8)},
    };
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return state;
  };
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
  if (parts.size() == 2 && parts[0] == "room_admin_secrets" && method == "POST") {
    if (config_.room_admin_tool_token.empty()) {
      return {503, R"({"ok":false,"error":"room_admin_tool_token_not_configured"})"};
    }
    const auto request = body.value_or(nlohmann::json::object());
    const auto presented = request.is_object() ? trim(request.value("admin_tool_token", std::string{})) : std::string{};
    if (presented.empty() || presented != config_.room_admin_tool_token) {
      return {401, R"({"ok":false,"error":"unauthorized"})"};
    }
    if (config_.generation_handoff_command.empty()) {
      return {503, R"({"ok":false,"error":"generation_handoff_not_configured"})"};
    }
    const auto room_id = url_decode(parts[1]);
    const auto generation_state = load_generation_state(room_id);
    if (!generation_state.value("ok", false)) {
      return {404, R"({"ok":false,"error":"generation_state_not_found"})"};
    }
    const auto generation_id = generation_state.value("generation_id", std::string{});
    const auto handoff = run_generation_handoff(
        config_,
        generation_id,
        nlohmann::json{
            {"schema_version", "sekailink-live-generation-request-v1"},
            {"action", "generation_admin_secrets"},
            {"generation_id", generation_id},
            {"lobby_id", generation_state.value("lobby_id", std::string{})},
            {"sync_id", generation_state.value("lobby_id", std::string{})},
            {"source", "link-chat-api-room-admin-secrets"},
        });
    if (!handoff.ok || !handoff.response.is_object()) {
      return {502, nlohmann::json{{"ok", false}, {"error", handoff.error.empty() ? "generation_handoff_failed" : handoff.error}}.dump()};
    }
    if (!handoff.response.value("ok", false)) {
      return {404, handoff.response.dump()};
    }
    return {200, handoff.response.dump()};
  }
  if (parts.size() == 2 && parts[0] == "room_status" && method == "GET") {
    const auto room_id = url_decode(parts[1]);
    const auto generation_state = load_generation_state(room_id);
    if (generation_state.value("ok", false)) {
      auto generation_response = generation_state.at("response");
      if (!config_.generation_handoff_command.empty() &&
          generation_response.value("sync_package_ready", false)) {
        const auto generation_id = generation_state.value("generation_id", std::string{});
        const auto handoff = run_generation_handoff(
            config_,
            generation_id,
            nlohmann::json{
                {"schema_version", "sekailink-live-generation-request-v1"},
                {"action", "generation_status"},
                {"generation_id", generation_id},
                {"lobby_id", generation_state.value("lobby_id", std::string{})},
                {"sync_id", generation_state.value("lobby_id", std::string{})},
                {"source", "link-chat-api-room-status"},
            });
        if (handoff.ok && handoff.response.is_object() && !handoff.response.empty()) {
          generation_response = handoff.response;
          if (!config_.sqlite_path.empty()) {
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
                const auto next_status = handoff.status.empty() ? generation_state.value("status", std::string{}) : handoff.status;
                const auto next_room_url = handoff.room_url.empty() ? generation_state.value("room_url", std::string{}) : handoff.room_url;
                const auto next_error = handoff.error.empty() ? generation_state.value("error", std::string{}) : handoff.error;
                const auto response_dump = generation_response.dump();
                const auto updated_at = utc_now_iso8601();
                sqlite3_bind_text(update_stmt, 1, next_status.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 2, next_room_url.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 3, next_error.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 4, response_dump.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 5, updated_at.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 6, generation_state.value("lobby_id", std::string{}).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 7, generation_id.c_str(), -1, SQLITE_TRANSIENT);
                (void)sqlite3_step(update_stmt);
                sqlite3_finalize(update_stmt);
              }
              sqlite3_close(update_db);
            } else if (update_db != nullptr) {
              sqlite3_close(update_db);
            }
          }
        }
      }
      const auto launch_entries = json_array(generation_response, "launch_entries");
      nlohmann::json players = nlohmann::json::array();
      nlohmann::json downloads = nlohmann::json::array();
      std::map<int, bool> seen_slots;
      for (const auto& entry : launch_entries) {
        if (!entry.is_object()) continue;
        const auto slot = json_int(entry, {"slot"});
        if (slot <= 0) continue;
        const auto slot_name = json_string(entry, "slot_name");
        const auto username = json_string(entry, "username");
        const auto game = json_string(entry, "game");
        const auto download = json_string(entry, "download");
        if (!seen_slots[slot]) {
          players.push_back({
              {"slot", slot},
              {"name", !slot_name.empty() ? slot_name : username},
              {"game", game},
          });
          seen_slots[slot] = true;
        }
        if (!download.empty()) {
          downloads.push_back({
              {"slot", slot},
              {"download", download},
          });
        }
      }
      const auto last_port = json_int(generation_response, {"last_port", "room_port", "port"});
      auto status = json_string(generation_response, "room_status");
      const auto generation_status = generation_state.value("status", std::string("pending"));
      if (status.empty()) status = last_port > 0 ? std::string("ready") : (generation_response.value("sync_package_ready", false) ? std::string("package_ready") : generation_status);
      return {
          200,
          nlohmann::json{
              {"ok", true},
              {"room_id", room_id},
              {"lobby_id", generation_state.value("lobby_id", std::string{})},
              {"generation_id", generation_state.value("generation_id", std::string{})},
              {"status", status},
              {"tracker", json_string(generation_response, "tracker")},
              {"last_port", last_port},
              {"room_port", json_int(generation_response, {"room_port"})},
              {"room_host", json_string(generation_response, "room_host")},
              {"room_runtime_error", json_string(generation_response, "room_runtime_error")},
              {"room_runtime_log", json_string(generation_response, "room_runtime_log")},
              {"players", players},
              {"downloads", downloads},
              {"launch_entries", launch_entries},
              {"sync_package_path", json_string(generation_response, "sync_package_path")},
              {"sync_package_ready", generation_response.value("sync_package_ready", false)},
              {"players_total", players.size()},
              {"checks_done", 0},
              {"total_locations", 0},
              {"completed_players", 0},
          }.dump()};
    }
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"room_id", room_id},
            {"status", "pending"},
            {"tracker", ""},
            {"players_total", 0},
            {"checks_done", 0},
            {"total_locations", 0},
            {"completed_players", 0},
        }.dump()};
  }
  if (parts.size() == 4 && parts[0] == "generation_artifacts" && method == "GET") {
    const auto generation_id = url_decode(parts[1]);
    const auto entry_id = url_decode(parts[2]);
    const auto filename = url_decode(parts[3]);
    const auto generation_state = load_generation_state(generation_id);
    if (!generation_state.value("ok", false)) return {404, R"({"ok":false,"error":"generation_state_not_found"})"};
    const auto& generation_response = generation_state.at("response");
    const auto launch_entries = json_array(generation_response, "launch_entries");
    std::filesystem::path artifact_path;
    for (const auto& entry : launch_entries) {
      if (!entry.is_object()) continue;
      if (json_string(entry, "entry_id") != entry_id) continue;
      if (json_string(entry, "artifact_filename") != filename) continue;
      artifact_path = json_string(entry, "artifact_path");
      break;
    }
    if (artifact_path.empty() || !std::filesystem::exists(artifact_path)) {
      return {404, R"({"ok":false,"error":"generation_artifact_not_found"})"};
    }
    const auto output_dir = json_string(generation_response, "output_dir");
    if (output_dir.empty()) return {403, R"({"ok":false,"error":"generation_artifact_scope_missing"})"};
    std::error_code root_error;
    std::error_code file_error;
    const auto root = std::filesystem::weakly_canonical(std::filesystem::path(output_dir), root_error);
    const auto file = std::filesystem::weakly_canonical(artifact_path, file_error);
    if (root_error || file_error) return {403, R"({"ok":false,"error":"generation_artifact_scope_invalid"})"};
    const auto root_text = root.string();
    const auto file_text = file.string();
    const auto scoped = file_text == root_text ||
                        (file_text.size() > root_text.size() &&
                         file_text.rfind(root_text, 0) == 0 &&
                         (root_text.empty() || root_text.back() == std::filesystem::path::preferred_separator ||
                          file_text[root_text.size()] == std::filesystem::path::preferred_separator));
    if (!scoped) return {403, R"({"ok":false,"error":"generation_artifact_out_of_scope"})"};
    std::ifstream stream(file, std::ios::binary);
    if (!stream) return {404, R"({"ok":false,"error":"generation_artifact_open_failed"})"};
    std::ostringstream contents;
    contents << stream.rdbuf();
    return {200, contents.str()};
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
