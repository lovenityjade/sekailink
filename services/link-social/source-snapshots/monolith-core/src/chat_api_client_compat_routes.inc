ChatApiHttpResponse ChatApiService::handle_client_compat(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const std::optional<nlohmann::json>& body) const {
  (void)raw_path;
  (void)body;
  if (parts.empty() || parts[0] != "client") return {404, R"({"ok":false,"error":"not_found"})"};
  const auto manifest = read_json_file_object(config_.client_release_manifest_path);
  const auto channel_query = query_value(raw_path, "channel");
  const auto platform_query = query_value(raw_path, "platform");
  const auto release = select_release_manifest_entry(manifest, channel_query, platform_query);
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
            {"platform", first_string_limited(release, {"platform"}, platform_query, 80)},
            {"download_url", download_url},
            {"sha256", first_string_limited(release, {"sha256"}, "", 128)},
            {"signature", first_string_limited(release, {"signature"}, "", 2048)},
            {"public_key", first_string_limited(release, {"public_key"}, "", 2048)},
            {"notes_url", first_string_limited(release, {"notes_url"}, "https://sekailink.com/progress.html", 2048)},
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
