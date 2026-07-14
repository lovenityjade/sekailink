ChatApiHttpResponse ChatApiService::handle_client_compat(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const std::optional<nlohmann::json>& body) const {
  if (parts.empty() || parts[0] != "client") return {404, R"({"ok":false,"error":"not_found"})"};
  const auto manifest = read_json_file_object(config_.client_release_manifest_path);
  const auto channel_query = query_value(raw_path, "channel");
  const auto platform_query = query_value(raw_path, "platform");
  const auto build_query = query_value(raw_path, "build");
  const auto release = select_release_manifest_entry(manifest, channel_query, platform_query, build_query);
  if (method == "GET" && parts.size() == 2 && parts[1] == "version") {
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"latest", first_string_limited(release, {"latest", "version"}, "0.3.1", 80)},
            {"min_supported", first_string_limited(release, {"min_supported"}, "0.1.0", 80)},
            {"channel", first_string_limited(release, {"channel"}, channel_query.empty() ? std::string("dev") : channel_query, 80)},
            {"notes_url", first_string_limited(release, {"notes_url"}, "https://sekailink.com/progress.html", 2048)},
            {"incremental_manifest_url", first_string_limited(release, {"incremental_manifest_url"}, "", 2048)},
        }.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "release-latest") {
    const auto download_url = first_string_limited(release, {"download_url", "url"}, "", 2048);
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"available", release.value("available", !download_url.empty())},
            {"version", first_string_limited(release, {"version", "latest"}, "0.3.1", 80)},
            {"channel", first_string_limited(release, {"channel"}, channel_query.empty() ? std::string("dev") : channel_query, 80)},
            {"build", first_string_limited(release, {"build"}, "", 80)},
            {"platform", first_string_limited(release, {"platform"}, platform_query, 80)},
            {"download_url", download_url},
            {"sha256", first_string_limited(release, {"sha256"}, "", 128)},
            {"signature", first_string_limited(release, {"signature"}, "", 2048)},
            {"release_sequence", release.value("release_sequence", 0ull)},
            {"public_key", first_string_limited(release, {"public_key"}, "", 2048)},
            {"artifact_type", first_string_limited(release, {"artifact_type"}, "", 80)},
            {"requires_client_updater", first_string_limited(release, {"requires_client_updater"}, "", 80)},
            {"fallback_download_url", first_string_limited(release, {"fallback_download_url"}, "", 2048)},
            {"fallback_sha256", first_string_limited(release, {"fallback_sha256"}, "", 128)},
            {"fallback_signature", first_string_limited(release, {"fallback_signature"}, "", 2048)},
            {"fallback_public_key", first_string_limited(release, {"fallback_public_key"}, "", 2048)},
            {"fallback_artifact_type", first_string_limited(release, {"fallback_artifact_type"}, "", 80)},
            {"notes_url", first_string_limited(release, {"notes_url"}, "https://sekailink.com/progress.html", 2048)},
        }.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "bootstrapper-latest") {
    auto bootstrapper = select_bootstrapper_manifest_entry(manifest, channel_query, platform_query, build_query);
    if (bootstrapper.empty() && manifest.contains("bootstrapper") && manifest.at("bootstrapper").is_object()) {
      const auto& raw_bootstrapper = manifest.at("bootstrapper");
      if (raw_bootstrapper.contains("artifacts") && raw_bootstrapper.at("artifacts").is_array()) {
        bootstrapper = raw_bootstrapper;
      }
    }
    if (bootstrapper.empty()) {
      return {404, nlohmann::json{{"ok", false}, {"error", "bootstrapper_release_not_found"}}.dump()};
    }
    nlohmann::json artifacts = nlohmann::json::array();
    if (bootstrapper.contains("artifacts") && bootstrapper.at("artifacts").is_array()) {
      for (const auto& artifact : bootstrapper.at("artifacts")) {
        if (!artifact.is_object()) continue;
        const auto file_name = first_string_limited(artifact, {"fileName", "file_name", "name"}, "", 240);
        const auto url = first_string_limited(artifact, {"url", "download_url"}, "", 2048);
        const auto sha256 = first_string_limited(artifact, {"sha256"}, "", 128);
        if (url.empty() || sha256.empty()) continue;
        artifacts.push_back({
            {"fileName", file_name},
            {"file_name", file_name},
            {"url", url},
            {"download_url", url},
            {"sha256", sha256},
            {"size", artifact.value("size", 0ull)},
        });
      }
    } else {
      const auto url = first_string_limited(bootstrapper, {"download_url", "url"}, "", 2048);
      const auto sha256 = first_string_limited(bootstrapper, {"sha256"}, "", 128);
      if (!url.empty() && !sha256.empty()) {
        artifacts.push_back({
            {"fileName", first_string_limited(bootstrapper, {"fileName", "file_name", "name"}, "", 240)},
            {"file_name", first_string_limited(bootstrapper, {"fileName", "file_name", "name"}, "", 240)},
            {"url", url},
            {"download_url", url},
            {"sha256", sha256},
            {"size", bootstrapper.value("size", 0ull)},
        });
      }
    }
    if (artifacts.empty()) {
      return {404, nlohmann::json{{"ok", false}, {"error", "bootstrapper_artifact_not_found"}}.dump()};
    }
    const auto normalized_channel = normalized_release_channel(first_string_limited(bootstrapper, {"channel"}, channel_query, 80));
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"version", first_string_limited(bootstrapper, {"version", "latest"}, "0.0.0", 80)},
            {"channel", normalized_channel},
            {"build", first_string_limited(bootstrapper, {"build"}, build_query.empty() ? std::string("release") : build_query, 80)},
            {"platform", normalized_release_platform(platform_query)},
            {"implementation", first_string_limited(bootstrapper, {"implementation"}, "native-cpp-sdl2-miniz", 120)},
            {"artifacts", artifacts},
        }.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "runtime-versions") {
    return {200, nlohmann::json{{"ok", true}, {"bizhawk_version", ""}, {"poptracker_version", ""}, {"sekailink_core_version", "0.3.1"}}.dump()};
  }
  if (method == "GET" && parts.size() == 2 && parts[1] == "games") {
    if (!config_.seed_config_user_token.empty()) {
      const auto upstream = forward_to_seed_config("GET", "/seed-configs/games", std::nullopt);
      const auto parsed = parse_body_json(upstream);
      const auto status = response_status_from_body(upstream, parsed);
      if (status == 200 && parsed.value("ok", false) && parsed.contains("games") && parsed.at("games").is_array()) {
        nlohmann::json games = nlohmann::json::array();
        for (const auto& game : parsed.at("games")) {
          if (!game.is_object()) continue;
          const auto game_key = first_string_limited(game, {"game_key"}, "", 120);
          const auto display_name = first_string_limited(game, {"display_name", "name", "game"}, game_key, 160);
          if (game_key.empty() || display_name.empty()) continue;
          games.push_back({
              {"game_id", game_key},
              {"display_name", display_name},
              {"title", display_name},
              {"platform", first_string_limited(game, {"system_key", "platform"}, "", 80)},
              {"linkedworld_id", first_string_limited(game, {"linkedworld_id"}, linkedworld_id_from_game(game_key), 120)},
              {"schema_version", first_string_limited(game, {"schema_version"}, "", 160)},
              {"source", "nexus.seed_config_api"},
              {"status", "active"},
          });
        }
        return {200, nlohmann::json{{"ok", true}, {"games", games}, {"source", "nexus.seed_config_api"}}.dump()};
      }
    }
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
    if (parts[1] != "bug-report") {
      return {200, nlohmann::json{{"ok", true}, {"accepted", true}, {"forwarded", false}, {"reason", "compat_only"}}.dump()};
    }
    if (!body.has_value() || !body->is_object()) return {400, R"({"ok":false,"error":"invalid_body"})"};
    if (body->contains("bundle_base64") && body->at("bundle_base64").is_string() &&
        body->at("bundle_base64").get_ref<const std::string&>().size() > 64 * 1024 * 1024) {
      return {413, R"({"ok":false,"error":"bug_report_bundle_too_large"})"};
    }

    const auto raw_string = [](const nlohmann::json& object, const char* key, std::size_t max_len) {
      if (!object.contains(key) || !object.at(key).is_string()) return std::string{};
      auto value = object.at(key).get<std::string>();
      if (value.size() > max_len) value.resize(max_len);
      return value;
    };

    auto title = first_string_limited(*body, {"title", "summary", "message"}, "SekaiLink bug report", 100);
    if (title.size() < 3) title = "SekaiLink bug report";
    auto description = first_string_limited(*body, {"description", "detail", "error"}, "No description provided.", 200);
    if (description.empty()) description = "No description provided.";
    auto reporter = first_string_limited(*body, {"reporter_name", "reporter", "username", "display_name"}, "SekaiLink User", 80);
    if (reporter.empty()) reporter = "SekaiLink User";

    nlohmann::json app_info = body->contains("app_info") && body->at("app_info").is_object() ? body->at("app_info") : nlohmann::json::object();
    if (!app_info.contains("source")) app_info["source"] = "unknown";
    app_info["reported_via"] = "link-social.client-compat";
    app_info["report_type"] = parts[1];
    app_info["received_at"] = utc_now_iso8601();

    nlohmann::json system_info = body->contains("system_info") && body->at("system_info").is_object() ? body->at("system_info") : nlohmann::json::object();

    nlohmann::json report = {
        {"title", title},
        {"description", description},
        {"reporter_name", reporter},
        {"screenshot_base64", raw_string(*body, "screenshot_base64", 16 * 1024 * 1024)},
        {"logs_text", raw_string(*body, "logs_text", 1024 * 1024)},
        {"bundle_base64", raw_string(*body, "bundle_base64", 64 * 1024 * 1024)},
        {"bundle_manifest", body->contains("bundle_manifest") && body->at("bundle_manifest").is_object() ? body->at("bundle_manifest") : nlohmann::json::object()},
        {"system_info", system_info},
        {"app_info", app_info},
    };
    if (body->contains("channel_id") && !body->at("channel_id").is_null()) report["channel_id"] = body->at("channel_id");

    if (config_.social_bots_control_api_key.empty()) {
      return {
          200,
          nlohmann::json{
              {"ok", true},
              {"accepted", true},
              {"forwarded", false},
              {"warning", "social_bots_api_key_missing"},
          }.dump()};
    }

    const auto upstream = forward_to_social_bots("POST", "/discord/bug-report", report);
    const auto parsed = parse_body_json(upstream);
    const auto forwarded = upstream.status >= 200 && upstream.status < 300;
    if (!forwarded) {
      return {
          502,
          nlohmann::json{
              {"ok", false},
              {"accepted", true},
              {"forwarded", false},
              {"error", "bug_report_forward_failed"},
              {"upstream_status", upstream.status},
              {"upstream", parsed},
          }.dump()};
    }
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"accepted", true},
            {"forwarded", true},
            {"upstream_status", upstream.status},
            {"upstream", parsed},
        }.dump()};
  }
  return {404, R"({"ok":false,"error":"not_found"})"};
}
