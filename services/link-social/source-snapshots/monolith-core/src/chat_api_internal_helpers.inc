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

nlohmann::json read_json_file_object(const std::filesystem::path& path) {
  if (path.empty()) return nlohmann::json::object();
  std::ifstream stream(path);
  if (!stream) return nlohmann::json::object();
  try {
    auto parsed = nlohmann::json::parse(stream);
    return parsed.is_object() ? parsed : nlohmann::json::object();
  } catch (...) {
    return nlohmann::json::object();
  }
}

nlohmann::json select_release_manifest_entry(
    const nlohmann::json& manifest,
    const std::string& channel,
    const std::string& platform) {
  const auto matches_request = [&](const nlohmann::json& entry) {
    const auto entry_channel = first_string_limited(entry, {"channel"}, "", 80);
    const auto entry_platform = first_string_limited(entry, {"platform"}, "", 80);
    if (!channel.empty() && !entry_channel.empty() && entry_channel != channel) return false;
    if (!platform.empty() && !entry_platform.empty() && entry_platform != platform) return false;
    return true;
  };
  if (manifest.contains("releases") && manifest.at("releases").is_array()) {
    for (const auto& entry : manifest.at("releases")) {
      if (!entry.is_object()) continue;
      if (matches_request(entry)) return entry;
    }
    return nlohmann::json::object();
  }
  if (!manifest.is_object()) return nlohmann::json::object();
  return matches_request(manifest) ? manifest : nlohmann::json::object();
}

std::string avatar_url_from_user(const nlohmann::json& user) {
  return first_string_limited(user, {"avatar_url"}, "", 256 * 1024);
}

std::string chat_avatar_url_from_user(const nlohmann::json& user) {
  auto avatar_url = avatar_url_from_user(user);
  if (avatar_url.size() > 1024 || avatar_url.rfind("data:", 0) == 0) return {};
  return avatar_url;
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
  const auto& value = object.at(key);
  if (value.is_number_integer()) return value.get<int>();
  if (value.is_number_unsigned()) return static_cast<int>(value.get<unsigned int>());
  if (value.is_string()) {
    try {
      std::size_t parsed = 0;
      const auto out = std::stoi(value.get<std::string>(), &parsed);
      if (parsed == value.get<std::string>().size()) return out;
    } catch (...) {
    }
  }
  return fallback;
}

std::string json_id_or(const nlohmann::json& object, const char* key, const std::string& fallback = "") {
  if (!object.contains(key) || object.at(key).is_null()) return fallback;
  const auto& value = object.at(key);
  if (value.is_string()) return sanitized_text(value.get<std::string>(), 160);
  if (value.is_number_integer()) return std::to_string(value.get<long long>());
  if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
  return fallback;
}

bool is_decimal_id_string(const std::string& value) {
  return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
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
      {"max_players", json_int_or(metadata, "max_players", 50)},
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
    const auto config_id = json_id_or(config, "config_id", json_id_or(config, "yaml_id", json_id_or(config, "id")));
    const auto id = json_id_or(config, "id", config_id);
    if (id.empty()) return;
    const auto yaml_id = json_id_or(config, "yaml_id", config_id);
    const auto version_id = json_id_or(config, "version_id");
    const auto game_key = first_string_limited(config, {"game_key"}, "", 120);
    auto game = first_string_limited(config, {"game_display_name", "game"}, "unknown", 120);
    auto game_display_name = first_string_limited(config, {"game_display_name", "game"}, game, 120);
    if (game_display_name.empty()) game_display_name = game_key.empty() ? std::string("unknown") : game_key;
    if (game.empty()) game = game_display_name;
    const auto linkedworld_id = first_string_limited(
        config,
        {"linkedworld_id"},
        game_key.empty() ? linkedworld_id_from_game(game_display_name) : linkedworld_id_from_game(game_key),
        160);
    nlohmann::json item = {
        {"id", id},
        {"config_id", config_id.empty() ? id : config_id},
        {"yaml_id", yaml_id.empty() ? id : yaml_id},
        {"title", first_string_limited(config, {"title", "name"}, id, 120)},
        {"name", first_string_limited(config, {"name", "title"}, id, 120)},
        {"game", game},
        {"game_key", game_key},
        {"game_display_name", game_display_name},
        {"linkedworld_id", linkedworld_id},
        {"player_name", first_string_limited(config, {"player_name"}, "", 120)},
        {"custom", config.value("custom", true)},
    };
    if (!version_id.empty()) item["version_id"] = version_id;
    if (config.contains("values_hash") && config.at("values_hash").is_string()) item["values_hash"] = config.at("values_hash");
    if (config.contains("source") && config.at("source").is_string()) item["source"] = config.at("source");
    target.push_back(item);
  };

  const auto append_config = [&](const nlohmann::json& config) {
    const auto game_key = first_string_limited(config, {"game_key"}, "", 120);
    const auto game = first_string_limited(config, {"game_display_name", "game"}, "unknown", 120);
    const auto linkedworld_id = first_string_limited(
        config,
        {"linkedworld_id"},
        game_key.empty() ? linkedworld_id_from_game(game) : linkedworld_id_from_game(game_key),
        160);
    if (!by_game.contains(linkedworld_id)) {
      by_game[linkedworld_id] = grouped.size();
      grouped.push_back({
          {"linkedworld_id", linkedworld_id},
          {"game", game},
          {"game_key", game_key},
          {"game_display_name", first_string_limited(config, {"game_display_name", "game"}, game, 120)},
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
          if (!copy.contains("game_key") && item.contains("game_key")) copy["game_key"] = item.at("game_key");
          if (!copy.contains("game_display_name") && item.contains("game_display_name")) copy["game_display_name"] = item.at("game_display_name");
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

std::string safe_path_component(std::string value, std::string fallback) {
  std::string out;
  out.reserve(value.size());
  for (const auto c : value) {
    if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_') out.push_back(c);
    else if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == '.') out.push_back('-');
  }
  while (out.find("--") != std::string::npos) out.erase(out.find("--"), 1);
  while (!out.empty() && (out.front() == '-' || out.front() == '_')) out.erase(out.begin());
  while (!out.empty() && (out.back() == '-' || out.back() == '_')) out.pop_back();
  if (out.empty()) out = std::move(fallback);
  if (out.size() > 96) out.resize(96);
  return out;
}

std::string replace_all(std::string value, const std::string& needle, const std::string& replacement) {
  std::size_t pos = 0;
  while ((pos = value.find(needle, pos)) != std::string::npos) {
    value.replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
  return value;
}

void write_json_file_atomic(const std::filesystem::path& path, const nlohmann::json& value) {
  std::filesystem::create_directories(path.parent_path());
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("generation_handoff_write_failed");
    out << value.dump(2) << '\n';
  }
  std::filesystem::rename(tmp, path);
}

nlohmann::json read_json_file_or_empty(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) return nlohmann::json::object();
  nlohmann::json value;
  in >> value;
  return value;
}

#ifndef _WIN32
int run_process_wait(const std::vector<std::string>& args) {
  if (args.empty()) return 127;
  const auto pid = fork();
  if (pid < 0) return 127;
  if (pid == 0) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return 127;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return 127;
}
#endif

struct GenerationHandoffResult {
  bool ok = true;
  std::string status;
  std::string room_url;
  std::string error;
  nlohmann::json response = nlohmann::json::object();
};

GenerationHandoffResult run_generation_handoff(
    const ChatApiServiceConfig& config,
    const std::string& generation_id,
    nlohmann::json request) {
  GenerationHandoffResult result;
  if (config.generation_handoff_command.empty()) return result;
  if (config.generation_handoff_root.empty()) {
    return {.ok = false, .error = "generation_handoff_root_missing"};
  }
#ifdef _WIN32
  (void)generation_id;
  (void)request;
  return {.ok = false, .error = "generation_handoff_unsupported_on_windows"};
#else
  const auto safe_id = safe_path_component(generation_id, "generation");
  const auto job_dir = config.generation_handoff_root / safe_id;
  const auto request_path = job_dir / "request.json";
  const auto response_path = job_dir / "response.json";
  request["handoff"] = {
      {"request_path", request_path.string()},
      {"response_path", response_path.string()},
  };
  write_json_file_atomic(request_path, request);

  std::vector<std::string> args;
  args.reserve(config.generation_handoff_command.size());
  for (auto arg : config.generation_handoff_command) {
    arg = replace_all(std::move(arg), "{request_path}", request_path.string());
    arg = replace_all(std::move(arg), "{response_path}", response_path.string());
    arg = replace_all(std::move(arg), "{generation_id}", generation_id);
    args.push_back(std::move(arg));
  }
  const auto exit_code = run_process_wait(args);
  try {
    result.response = read_json_file_or_empty(response_path);
  } catch (...) {
    if (exit_code != 0) {
      return {
          .ok = false,
          .error = "generation_handoff_command_failed:" + std::to_string(exit_code),
      };
    }
    return {.ok = false, .error = "generation_handoff_invalid_response"};
  }
  if (exit_code != 0 && (!result.response.is_object() || result.response.empty())) {
    return {
        .ok = false,
        .error = "generation_handoff_command_failed:" + std::to_string(exit_code),
    };
  }
  if (!result.response.is_object() || result.response.empty()) return result;
  result.ok = result.response.value("ok", true);
  result.status = result.response.value("status", result.status);
  result.room_url = result.response.value("room_url", std::string{});
  result.error = result.response.value("error", std::string{});
  if (exit_code != 0 && result.ok) {
    result.ok = false;
    result.error = "generation_handoff_command_failed:" + std::to_string(exit_code);
  }
  if (!result.ok && result.error.empty()) result.error = "generation_handoff_rejected";
  return result;
#endif
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

void ensure_lobby_generation_state_table(sqlite3* db) {
  sqlite_exec_checked(
      db,
      "CREATE TABLE IF NOT EXISTS lobby_generation_state ("
      "lobby_id TEXT PRIMARY KEY,"
      "generation_id TEXT NOT NULL,"
      "status TEXT NOT NULL,"
      "room_url TEXT NOT NULL DEFAULT '',"
      "error TEXT NOT NULL DEFAULT '',"
      "players_json TEXT NOT NULL DEFAULT '[]',"
      "response_json TEXT NOT NULL DEFAULT '{}',"
      "created_at TEXT NOT NULL,"
      "updated_at TEXT NOT NULL"
      ");");

  bool has_response_json = false;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "PRAGMA table_info(lobby_generation_state);", -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      if (sqlite_column_string(stmt, 1) == "response_json") {
        has_response_json = true;
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  if (!has_response_json) {
    sqlite_exec_checked(db, "ALTER TABLE lobby_generation_state ADD COLUMN response_json TEXT NOT NULL DEFAULT '{}';");
  }
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
