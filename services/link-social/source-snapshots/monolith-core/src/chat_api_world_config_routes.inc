ChatApiHttpResponse ChatApiService::handle_world_config(
    const std::string& method,
    const std::string& raw_path,
    const std::vector<std::string>& parts,
    const nlohmann::json& identity,
    const std::optional<nlohmann::json>& body) const {
  (void)raw_path;
  (void)body;
  const auto me = identity_user_id(identity);
  if (me.empty()) return {401, R"({"ok":false,"error":"unauthorized"})"};
  auto canonical_seed_game_key = [](std::string key) {
    key = lower(trim(std::move(key)));
    if (key.empty()) return std::string{};
    std::string normalized;
    normalized.reserve(key.size());
    bool last_was_separator = false;
    for (const auto c : key) {
      if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
        normalized.push_back(c);
        last_was_separator = false;
      } else if (!last_was_separator) {
        normalized.push_back('_');
        last_was_separator = true;
      }
    }
    while (!normalized.empty() && normalized.front() == '_') normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '_') normalized.pop_back();
    key = std::move(normalized);
    if (key.rfind("multiworldgg_", 0) == 0) key = key.substr(std::string("multiworldgg_").size());
    if (key == "alttp" || key == "a-link-to-the-past" || key == "a_link_to_the_past") {
      return std::string("a_link_to_the_past");
    }
    static const std::unordered_map<std::string, std::string> aliases = {
        {"chrono_trigger_jets_of_time", "chrono_trigger"},
        {"final_fantasy_iv_free_enterprise", "final_fantasy_iv"},
        {"final_fantasy_v_career_day", "final_fantasy_v"},
        {"kirby_s_dream_land_3", "kirbys_dream_land_3"},
        {"links_awakening_dx_beta", "links_awakening_dx"},
        {"link_s_awakening_dx", "links_awakening_dx"},
        {"lufia_ii_ancient_cave", "lufia_ii"},
        {"luigi_s_mansion", "luigis_mansion"},
        {"mario_and_luigi_superstar_saga", "mario_luigi_superstar_saga"},
        {"mega_man_battle_network_3_blue", "mega_man_battle_network_3"},
        {"paper_mario_the_thousand_year_door", "thousand_year_door"},
        {"pokemon_firered_and_leafgreen", "pokemon_firered"},
        {"pokemon_fire_red_and_leaf_green", "pokemon_firered"},
        {"pokemon_red_and_blue", "pokemon_red_blue"},
        {"the_legend_of_zelda_a_link_to_the_past", "a_link_to_the_past"},
        {"the_legend_of_zelda_oracle_of_ages", "oracle_of_ages"},
        {"the_legend_of_zelda_oracle_of_seasons", "oracle_of_seasons"},
        {"the_legend_of_zelda_twilight_princess", "twilight_princess"},
        {"zelda_ii_the_adventure_of_link", "zelda_ii"},
    };
    if (const auto it = aliases.find(key); it != aliases.end()) return it->second;
    return key;
  };
  if (method == "GET" && !parts.empty() && parts[0] == "seed-configs") {
    const bool supported =
        (parts.size() == 2 && parts[1] == "games") ||
        (parts.size() == 4 && parts[1] == "games" && (parts[3] == "options" || parts[3] == "presets"));
    if (!supported) {
      return {404, R"({"ok":false,"error":"not_found"})"};
    }
    std::string upstream_path;
    for (std::size_t i = 0; i < parts.size(); ++i) {
      upstream_path.push_back('/');
      upstream_path.append(i == 2 ? canonical_seed_game_key(url_decode(parts[i])) : parts[i]);
    }
    return forward_to_seed_config(method, upstream_path, std::nullopt);
  }
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
  auto validate_saved_seed_config = [&](const std::string& config_id,
                                        const std::string& version_id,
                                        const std::string& game_key,
                                        const std::string& game_display_name,
                                        const std::string& title) -> std::optional<ChatApiHttpResponse> {
    if (config_.generation_handoff_command.empty()) {
      return std::nullopt;
    }
    const auto exported = forward_to_seed_config(
        "POST",
        "/users/" + me + "/seed-configs/" + config_id + "/export-yaml",
        std::nullopt);
    const auto exported_json = parse_body_json(exported);
    const auto export_status = response_status_from_body(exported, exported_json);
    if (export_status != 200 || !exported_json.value("ok", false) ||
        !exported_json.contains("yaml") || !exported_json.at("yaml").is_string()) {
      const auto cleanup = forward_to_seed_config("DELETE", "/users/" + me + "/seed-configs/" + config_id, std::nullopt);
      (void)cleanup;
      return ChatApiHttpResponse{
          export_status,
          nlohmann::json{
              {"ok", false},
              {"error", "seed_config_export_failed"},
              {"detail", exported_json.value("error", std::string("unable_to_export_validation_yaml"))},
          }.dump()};
    }
    const auto validation_id = "config-validation-" + config_id + "-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    const nlohmann::json validation_request = {
        {"schema_version", "sekailink-config-validation-request-v1"},
        {"action", "validate_seed_config"},
        {"generation_id", validation_id},
        {"user_id", me},
        {"player_name", identity_display_name(identity)},
        {"config_id", config_id},
        {"version_id", version_id},
        {"title", title},
        {"game_key", game_key},
        {"game_display_name", game_display_name},
        {"content", exported_json.at("yaml").get<std::string>()},
        {"created_at", utc_now_iso8601()},
    };
    const auto handoff = run_generation_handoff(config_, validation_id, validation_request);
    if (!handoff.ok || !handoff.response.value("ok", false)) {
      const auto cleanup = forward_to_seed_config("DELETE", "/users/" + me + "/seed-configs/" + config_id, std::nullopt);
      (void)cleanup;
      const auto detail = handoff.response.is_object()
          ? first_string_limited(handoff.response, {"detail", "message", "error"}, handoff.error, 4096)
          : handoff.error;
      return ChatApiHttpResponse{
          400,
          nlohmann::json{
              {"ok", false},
              {"error", "seed_config_apworld_validation_failed"},
              {"detail", detail.empty() ? std::string("Archipelago rejected this seed config.") : detail},
              {"generation_exit_code", handoff.response.is_object() && handoff.response.contains("generation_exit_code") ? handoff.response.at("generation_exit_code") : nlohmann::json(nullptr)},
              {"generation_log_path", handoff.response.is_object() ? handoff.response.value("generation_log_path", std::string{}) : std::string{}},
          }.dump()};
    }
    return std::nullopt;
  };
  if (method == "POST" && parts.size() == 1 && parts[0] == "seed-configs" &&
      !config_.seed_config_user_token.empty() && is_decimal_id_string(me)) {
    const auto request = body.value_or(nlohmann::json::object());
    auto game_key = canonical_seed_game_key(first_string_limited(request, {"game_key"}, "", 120));
    if (game_key.empty()) return {400, R"({"ok":false,"error":"missing_game_key"})"};
    const auto values = request.contains("values") && request.at("values").is_object()
        ? request.at("values")
        : nlohmann::json::object();
    nlohmann::json upstream_body = {
        {"game_key", game_key},
        {"name", first_string_limited(request, {"title", "name"}, "SekaiLink Seed", 120)},
        {"description", first_string_limited(request, {"description"}, "Created from SekaiLink Client Core.", 240)},
        {"values", values},
    };
    const auto upstream = forward_to_seed_config("POST", "/users/" + me + "/seed-configs", upstream_body);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false) || !parsed.contains("config") || !parsed.at("config").is_object()) {
      return {status, parsed.dump()};
    }
    const auto& config = parsed.at("config");
    const auto config_id = json_id_or(config, "config_id", json_id_or(config, "id"));
    const auto version_id = json_id_or(config, "version_id");
    const auto game_display_name = seed_game_display_name(game_key, first_string_limited(request, {"game"}, game_key, 120));
    const auto title = first_string_limited(config, {"name", "title"}, upstream_body.value("name", std::string("SekaiLink Seed")), 120);
    if (const auto validation_error = validate_saved_seed_config(config_id, version_id, game_key, game_display_name, title);
        validation_error.has_value()) {
      return *validation_error;
    }
    nlohmann::json item = {
        {"id", config_id},
        {"config_id", config_id},
        {"yaml_id", config_id},
        {"title", title},
        {"name", title},
        {"game", game_display_name},
        {"game_key", game_key},
        {"game_display_name", game_display_name},
        {"linkedworld_id", linkedworld_id_from_game(game_key)},
        {"player_name", identity_display_name(identity)},
        {"custom", true},
        {"source", "advanced.seed_config_api"},
        {"validation_status", config_.generation_handoff_command.empty() ? "skipped" : "validated"},
        {"values", config.contains("values") && config.at("values").is_object() ? config.at("values") : values},
    };
    if (!version_id.empty()) item["version_id"] = version_id;
    if (config.contains("values_hash") && config.at("values_hash").is_string()) item["values_hash"] = config.at("values_hash");
    return {200, nlohmann::json{{"ok", true}, {"seed", item}, {"yaml", item}, {"source", "nexus.seed_config_api"}}.dump()};
  }
  auto explicit_seed_config_values = [&](const std::string& game_key, const nlohmann::json& values) {
    if (!values.is_object()) return nlohmann::json::object();
    const auto upstream = forward_to_seed_config("GET", "/seed-configs/games/" + game_key + "/options", std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false) || !parsed.contains("game") || !parsed.at("game").is_object()) {
      return values;
    }
    const auto& game = parsed.at("game");
    if (!game.contains("options") || !game.at("options").is_array()) return values;

    nlohmann::json option_by_key = nlohmann::json::object();
    for (const auto& option : game.at("options")) {
      if (!option.is_object()) continue;
      const auto option_key = first_string_limited(option, {"option_key"}, "", 120);
      if (option_key.empty()) continue;
      option_by_key[option_key] = option;
      const auto yaml_key = first_string_limited(option, {"yaml_key"}, "", 120);
      if (!yaml_key.empty()) option_by_key[yaml_key] = option;
    }

    nlohmann::json explicit_values = nlohmann::json::object();
    for (const auto& [key, value] : values.items()) {
      if (!option_by_key.contains(key) || !option_by_key.at(key).is_object()) {
        explicit_values[key] = value;
        continue;
      }
      const auto& option = option_by_key.at(key);
      if (option.contains("default_value") && option.at("default_value") == value) {
        continue;
      }
      const auto option_key = first_string_limited(option, {"option_key"}, key, 120);
      explicit_values[option_key] = value;
    }
    return explicit_values;
  };
  auto pulse_error = [](int status, const std::string& error) {
    return ChatApiHttpResponse{status, nlohmann::json{{"ok", false}, {"error", error}}.dump()};
  };
  auto pulse_hex_encode = [](const std::string& raw) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto c : raw) {
      out << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
    }
    return out.str();
  };
  auto pulse_hex_decode = [](const std::string& encoded) -> std::optional<std::string> {
    if (encoded.empty() || encoded.size() > 8192 || encoded.size() % 2 != 0) return std::nullopt;
    auto hex_value = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return 10 + c - 'a';
      if (c >= 'A' && c <= 'F') return 10 + c - 'A';
      return -1;
    };
    std::string out;
    out.reserve(encoded.size() / 2);
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
      const auto high = hex_value(encoded[i]);
      const auto low = hex_value(encoded[i + 1]);
      if (high < 0 || low < 0) return std::nullopt;
      out.push_back(static_cast<char>((high << 4) | low));
    }
    return out;
  };
  auto pulse_questions = [] {
    return nlohmann::json::array({
        {
            {"id", "experience"},
            {"text", "What kind of run do you want?"},
            {"answers", nlohmann::json::array({
                {{"id", "casual"}, {"label", "Casual and guided"}, {"description", "Friendly defaults with logic safety and smoother progression."}},
                {{"id", "balanced"}, {"label", "Balanced showcase"}, {"description", "A standard showcase seed with familiar Archipelago defaults."}},
                {{"id", "challenge"}, {"label", "Extra challenge"}, {"description", "Less assistance, but still fair and no-glitches."}},
                {{"id", "expert"}, {"label", "Expert routing"}, {"description", "Low assistance for players who want to route aggressively."}},
                {{"id", "surprise"}, {"label", "Surprise me"}, {"description", "Let Pulse choose a clean but varied setup."}},
            })},
        },
        {
            {"id", "pace"},
            {"text", "How long should the seed feel?"},
            {"answers", nlohmann::json::array({
                {{"id", "short"}, {"label", "Short"}, {"description", "Lower completion pressure where the schema supports it."}},
                {{"id", "standard"}, {"label", "Standard"}, {"description", "Normal showcase pacing."}},
                {{"id", "long"}, {"label", "Long"}, {"description", "More checks before the finish."}},
                {{"id", "hunt"}, {"label", "Treasure hunt"}, {"description", "Prefer a collection-style goal when available."}},
                {{"id", "marathon"}, {"label", "Marathon"}, {"description", "A fuller route for longer sessions."}},
            })},
        },
        {
            {"id", "shuffle"},
            {"text", "How much item shuffle do you want?"},
            {"answers", nlohmann::json::array({
                {{"id", "simple"}, {"label", "Simple"}, {"description", "Dungeon items stay close to their normal homes when supported."}},
                {{"id", "dungeon_items"}, {"label", "Dungeon items"}, {"description", "Maps, compasses, and keys become part of the run."}},
                {{"id", "keysanity"}, {"label", "Keysanity"}, {"description", "Keys and dungeon info are shuffled for the full tracker showcase."}},
                {{"id", "mixed"}, {"label", "Mixed"}, {"description", "A middle ground with broader dungeon item routing."}},
                {{"id", "maximum"}, {"label", "Maximum"}, {"description", "Use the most complete shuffle this game schema allows."}},
            })},
        },
        {
            {"id", "goal"},
            {"text", "What should the win condition be?"},
            {"answers", nlohmann::json::array({
                {{"id", "final_boss"}, {"label", "Final boss"}, {"description", "A classic finish when the route is ready."}},
                {{"id", "crystals"}, {"label", "Crystal route"}, {"description", "Focus on the main dungeon progression."}},
                {{"id", "triforce_hunt"}, {"label", "Triforce hunt"}, {"description", "Collect pieces when the game supports that mode."}},
                {{"id", "bosses"}, {"label", "Boss route"}, {"description", "Prefer boss-focused goals if present in the schema."}},
                {{"id", "pedestal"}, {"label", "Pedestal"}, {"description", "Use the alternate objective if supported."}},
            })},
        },
        {
            {"id", "comfort"},
            {"text", "Pick one comfort option."},
            {"answers", nlohmann::json::array({
                {{"id", "open_start"}, {"label", "Open start"}, {"description", "Start in a flexible mode when supported."}},
                {{"id", "boots_start"}, {"label", "Early mobility"}, {"description", "Ask Pulse to prefer movement help when the schema allows it."}},
                {{"id", "helpful_hints"}, {"label", "Helpful hints"}, {"description", "Enable hints or hint-like help if this game exposes it."}},
                {{"id", "classic_start"}, {"label", "Classic start"}, {"description", "Keep the start closer to the original game."}},
                {{"id", "no_comfort"}, {"label", "No extra help"}, {"description", "Do not add comfort preferences."}},
            })},
        },
    });
  };
  auto pulse_session_id = [&](const nlohmann::json& session) {
    return pulse_hex_encode(session.dump());
  };
  auto pulse_session_from_id = [&](const std::string& session_id) -> std::optional<nlohmann::json> {
    const auto decoded = pulse_hex_decode(url_decode(session_id));
    if (!decoded.has_value()) return std::nullopt;
    try {
      auto parsed = nlohmann::json::parse(*decoded);
      if (!parsed.is_object() || parsed.value("v", 0) != 1) return std::nullopt;
      if (!parsed.contains("game_key") || !parsed.at("game_key").is_string()) return std::nullopt;
      if (!parsed.contains("answers") || !parsed.at("answers").is_array()) return std::nullopt;
      return parsed;
    } catch (...) {
      return std::nullopt;
    }
  };
  auto pulse_response_for_session = [&](nlohmann::json session) {
    const auto questions = pulse_questions();
    const auto answered = session.contains("answers") && session.at("answers").is_array()
                              ? session.at("answers").size()
                              : 0;
    session["session_id"] = pulse_session_id(session);
    if (answered >= questions.size()) {
      session["complete"] = true;
      if (!session.contains("summary") || !session.at("summary").is_string() || session.at("summary").get<std::string>().empty()) {
        session["summary"] = "Pulse has enough information to create this Sync config.";
      }
    } else {
      session["complete"] = false;
      session["question"] = questions.at(answered);
    }
    return ChatApiHttpResponse{200, nlohmann::json{{"ok", true}, {"session", session}}.dump()};
  };
  auto pulse_answer_ids = [](const nlohmann::json& session) {
    std::vector<std::string> ids;
    if (!session.contains("answers") || !session.at("answers").is_array()) return ids;
    for (const auto& answer : session.at("answers")) {
      if (!answer.is_object()) continue;
      const auto id = first_string_limited(answer, {"answer_id", "id"}, "", 80);
      if (!id.empty()) ids.push_back(id);
    }
    return ids;
  };
  auto pulse_has_answer = [](const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  };
  auto pulse_schema_options = [&](const std::string& game_key) {
    nlohmann::json options = nlohmann::json::array();
    const auto upstream = forward_to_seed_config("GET", "/seed-configs/games/" + game_key + "/options", std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status != 200 || !parsed.value("ok", false) || !parsed.contains("game") || !parsed.at("game").is_object()) {
      return options;
    }
    const auto& game = parsed.at("game");
    if (game.contains("options") && game.at("options").is_array()) return game.at("options");
    return options;
  };
  auto pulse_values_from_answers = [&](const std::string& game_key, const std::vector<std::string>& ids) {
    const auto options = pulse_schema_options(game_key);
    nlohmann::json values = nlohmann::json::object();
    auto option_for = [&](const std::string& key) -> const nlohmann::json* {
      for (const auto& option : options) {
        if (!option.is_object()) continue;
        if (first_string_limited(option, {"option_key", "yaml_key"}, "", 120) == key) return &option;
        if (first_string_limited(option, {"yaml_key"}, "", 120) == key) return &option;
      }
      return nullptr;
    };
    auto set_choice = [&](const std::string& key, const std::vector<std::string>& candidates) {
      const auto* option = option_for(key);
      if (option == nullptr) return false;
      if (!option->contains("choices") || !option->at("choices").is_array() || option->at("choices").empty()) {
        if (!candidates.empty()) {
          values[key] = candidates.front();
          return true;
        }
        return false;
      }
      for (const auto& candidate : candidates) {
        for (const auto& choice : option->at("choices")) {
          if (!choice.is_object()) continue;
          const auto choice_key = first_string_limited(choice, {"choice_key"}, "", 120);
          const auto yaml_value = first_string_limited(choice, {"yaml_value", "value"}, choice_key, 120);
          const auto label = lower(first_string_limited(choice, {"label"}, "", 120));
          if (choice_key == candidate || yaml_value == candidate || label == lower(candidate)) {
            values[key] = yaml_value.empty() ? choice_key : yaml_value;
            return true;
          }
        }
      }
      return false;
    };
    auto set_bool = [&](const std::string& key, bool value) {
      const auto* option = option_for(key);
      if (option == nullptr) return;
      const auto type = lower(first_string_limited(*option, {"type"}, "", 80));
      if (type == "boolean" || type == "bool" || type.empty()) values[key] = value;
    };
    auto set_number = [&](const std::string& key, int value) {
      const auto* option = option_for(key);
      if (option == nullptr) return;
      const auto type = lower(first_string_limited(*option, {"type"}, "", 80));
      if (type == "range" || type == "named_range" || type == "integer" || type == "number" || type.empty()) {
        values[key] = value;
      }
    };

    set_choice("glitches_required", {"no_glitches", "none"});
    set_choice("progressive", {"on", "progressive"});
    if (pulse_has_answer(ids, "casual")) {
      set_choice("accessibility", {"items", "locations"});
      set_number("progression_balancing", 75);
    } else if (pulse_has_answer(ids, "challenge")) {
      set_choice("accessibility", {"items", "minimal"});
      set_number("progression_balancing", 35);
    } else if (pulse_has_answer(ids, "expert")) {
      set_choice("accessibility", {"minimal", "items"});
      set_number("progression_balancing", 15);
    } else {
      set_choice("accessibility", {"items", "locations"});
      set_number("progression_balancing", 50);
    }

    if (pulse_has_answer(ids, "classic_start")) {
      set_choice("mode", {"standard", "open"});
    } else {
      set_choice("mode", {"open", "standard"});
    }

    if (pulse_has_answer(ids, "short")) {
      set_number("crystals_needed_for_gt", 5);
      set_number("crystals_needed_for_ganon", 5);
    } else if (pulse_has_answer(ids, "long") || pulse_has_answer(ids, "marathon")) {
      set_number("crystals_needed_for_gt", 7);
      set_number("crystals_needed_for_ganon", 7);
    }

    if (pulse_has_answer(ids, "triforce_hunt") || pulse_has_answer(ids, "hunt")) {
      set_choice("goal", {"triforce_hunt", "triforce-hunt"});
      set_number("triforce_pieces_required", pulse_has_answer(ids, "marathon") ? 30 : 20);
      set_number("triforce_pieces_available", pulse_has_answer(ids, "marathon") ? 40 : 30);
    } else if (pulse_has_answer(ids, "pedestal")) {
      set_choice("goal", {"pedestal", "master_sword_pedestal"});
    } else if (pulse_has_answer(ids, "bosses")) {
      set_choice("goal", {"bosses", "ganon"});
    } else if (pulse_has_answer(ids, "crystals")) {
      set_choice("goal", {"crystals", "ganon"});
    } else {
      set_choice("goal", {"ganon", "final_boss"});
    }

    const auto broad_shuffle = pulse_has_answer(ids, "keysanity") || pulse_has_answer(ids, "maximum");
    const auto mixed_shuffle = pulse_has_answer(ids, "dungeon_items") || pulse_has_answer(ids, "mixed");
    const auto shuffle_value = broad_shuffle ? std::vector<std::string>{"own_world", "any_world", "keysanity"}
                                             : mixed_shuffle ? std::vector<std::string>{"own_world", "original_dungeon"}
                                                             : std::vector<std::string>{"original_dungeon", "vanilla"};
    set_choice("big_key_shuffle", shuffle_value);
    set_choice("small_key_shuffle", shuffle_value);
    set_choice("map_shuffle", shuffle_value);
    set_choice("compass_shuffle", shuffle_value);
    if (broad_shuffle || mixed_shuffle) set_bool("key_drop_shuffle", true);
    if (pulse_has_answer(ids, "helpful_hints")) {
      if (!set_choice("hints", {"on", "enabled", "true"})) set_bool("hints", true);
    }
    return values;
  };
  auto pulse_local_answer = [](std::string prompt) {
    const auto text = lower(trim(std::move(prompt)));
    if (text.empty()) {
      return std::string("Pose-moi une question sur Archipelago, les settings, les trackers ou SekaiLink et je vais te repondre avec ma base locale.");
    }
    if (text.find("keysanity") != std::string::npos || text.find("key sanity") != std::string::npos) {
      return std::string("Keysanity veut dire que les cles, et parfois les maps/compasses/big keys selon le jeu, deviennent des checks/items randomises au lieu de rester dans leur structure normale. C'est plus long a router et le tracker devient beaucoup plus important.");
    }
    if (text.find("sphere") != std::string::npos || text.find("spheres") != std::string::npos) {
      return std::string("Une sphere est une couche de progression du spoiler log. La sphere 0 correspond au depart, puis chaque sphere suivante liste les checks rendus logiquement accessibles grace aux items obtenus dans les spheres precedentes.");
    }
    if (text.find("spoiler") != std::string::npos || text.find("hint") != std::string::npos || text.find("indice") != std::string::npos) {
      return std::string("Je peux aider a lire un spoiler log ou donner un indice, mais il faut que la seed ne soit pas en contexte de race et que le joueur accepte les spoilers. En mode safe, je peux expliquer la logique sans reveler les placements exacts.");
    }
    if (text.find("bk") != std::string::npos || text.find("bloque") != std::string::npos || text.find("blocked") != std::string::npos) {
      return std::string("BK veut dire que tu es bloque cote progression. Logical BK signifie qu'il te manque un item logique venant probablement d'un autre check ou joueur. Full BK signifie que tu n'as vraiment plus de check raisonnable a faire sans aide, hint ou verification de sync.");
    }
    if (text.find("deathlink") != std::string::npos || text.find("death link") != std::string::npos) {
      return std::string("DeathLink est une option reseau ou la mort d'un joueur peut envoyer une mort aux autres joueurs qui ont aussi active DeathLink. Ce n'est pas universel: il faut que le jeu ou le client le supporte explicitement.");
    }
    if (text.find("yaml") != std::string::npos || text.find("setting") != std::string::npos || text.find("option") != std::string::npos) {
      return std::string("Le YAML est la configuration d'un slot Archipelago: jeu, nom du joueur, objectif, logique, shuffles et options propres au monde. Pour bien l'expliquer, il faut separer les options communes d'Archipelago des options specifiques au jeu.");
    }
    if (text.find("tracker") != std::string::npos || text.find("poptracker") != std::string::npos) {
      return std::string("Un tracker sert a visualiser les items, les checks restants et parfois la logique accessible. PopTracker est la cible principale de SekaiLink pour le moment; Universal Tracker est interessant, mais reste une avenue separee.");
    }
    if (text.find("apworld") != std::string::npos || text.find("archipelago") != std::string::npos) {
      return std::string("Un APWorld est le module Archipelago qui decrit un jeu: ses items, locations, options, logique et generation. SekaiLink utilise ces informations pour aider la creation de seeds, le suivi des rooms et l'integration client/emulateur.");
    }
    if (text.find("sync") != std::string::npos || text.find("connexion") != std::string::npos || text.find("connect") != std::string::npos) {
      return std::string("Pour diagnostiquer une sync, verifie dans l'ordre: le bon serveur/room, le bon slot, le client connecte, l'etat safe du jeu pour recevoir les items, puis les logs SKLMI/Sekaiemu. Certains jeux retardent les items en combat, menu ou script.");
    }
    return std::string("Je suis en mode base de connaissances locale pour l'instant. Je peux expliquer les termes de randomizer, les settings Archipelago, les trackers, la logique de seed et les diagnostics de connexion. Donne-moi le jeu et le setting qui te melange.");
  };
  if (parts.size() == 2 && parts[0] == "pulse" && parts[1] == "chat") {
    if (method != "POST") return pulse_error(405, "method_not_allowed");
    const auto request = body.value_or(nlohmann::json::object());
    const auto prompt = first_string_limited(request, {"prompt", "message", "content"}, "", 4000);
    return {
        200,
        nlohmann::json{
            {"ok", true},
            {"answer", pulse_local_answer(prompt)},
            {"source", "pulse.local_knowledge_fallback"},
            {"degraded", true},
        }.dump()};
  }
  if (parts.size() == 2 && parts[0] == "pulse" && parts[1] == "feedback") {
    if (method != "POST") return pulse_error(405, "method_not_allowed");
    return {200, nlohmann::json{{"ok", true}, {"source", "pulse.feedback_ack"}}.dump()};
  }
  if (parts.size() >= 3 && parts[0] == "pulse" && parts[1] == "seed-config" && parts[2] == "sessions") {
    if (!is_decimal_id_string(me)) return pulse_error(502, "seed_config_user_mapping_unavailable");
    if (method == "POST" && parts.size() == 3) {
      const auto request = body.value_or(nlohmann::json::object());
      auto game_key = canonical_seed_game_key(first_string_limited(request, {"game_key"}, "", 120));
      if (game_key.empty()) game_key = canonical_seed_game_key(first_string_limited(request, {"game_name", "game"}, "", 120));
      if (game_key.empty()) return pulse_error(400, "missing_game_key");
      const auto game_name = seed_game_display_name(game_key, first_string_limited(request, {"game_name", "game"}, game_key, 120));
      nlohmann::json session = {
          {"v", 1},
          {"game_key", game_key},
          {"game_name", game_name},
          {"title", "Pulse " + game_name},
          {"answers", nlohmann::json::array()},
      };
      return pulse_response_for_session(std::move(session));
    }
    if (method == "POST" && parts.size() == 5 && parts[4] == "answers") {
      auto session = pulse_session_from_id(parts[3]);
      if (!session.has_value()) return pulse_error(400, "invalid_pulse_session");
      const auto questions = pulse_questions();
      auto answers = session->contains("answers") && session->at("answers").is_array()
                         ? session->at("answers")
                         : nlohmann::json::array();
      if (answers.size() >= questions.size()) return pulse_response_for_session(*session);
      const auto request = body.value_or(nlohmann::json::object());
      const auto expected_question_id = first_string_limited(questions.at(answers.size()), {"id"}, "", 80);
      const auto question_id = first_string_limited(request, {"question_id"}, "", 80);
      const auto answer_id = first_string_limited(request, {"answer_id", "id"}, "", 80);
      if (question_id != expected_question_id || answer_id.empty()) return pulse_error(400, "invalid_pulse_answer");
      bool allowed = false;
      for (const auto& answer : questions.at(answers.size()).at("answers")) {
        if (first_string_limited(answer, {"id"}, "", 80) == answer_id) {
          allowed = true;
          break;
        }
      }
      if (!allowed) return pulse_error(400, "invalid_pulse_answer");
      answers.push_back({
          {"question_id", question_id},
          {"answer_id", answer_id},
          {"answer_label", first_string_limited(request, {"answer_label", "label"}, answer_id, 120)},
      });
      (*session)["answers"] = std::move(answers);
      if (session->at("answers").is_array() && session->at("answers").size() >= questions.size()) {
        const auto game_key = canonical_seed_game_key(first_string_limited(*session, {"game_key"}, "", 120));
        const auto ids = pulse_answer_ids(*session);
        const auto values = pulse_values_from_answers(game_key, ids);
        (*session)["summary"] = "Pulse selected a guided " + first_string_limited(*session, {"game_name"}, game_key, 120) +
                                " setup based on your answers. Review the settings below before creating the seed.";
        (*session)["seed"] = {
            {"title", first_string_limited(*session, {"title"}, "Pulse Seed", 120)},
            {"game", first_string_limited(*session, {"game_name"}, game_key, 120)},
            {"game_key", game_key},
            {"source", "pulse"},
            {"values", values},
        };
      }
      return pulse_response_for_session(std::move(*session));
    }
    if (method == "POST" && parts.size() == 5 && parts[4] == "confirm") {
      auto session = pulse_session_from_id(parts[3]);
      if (!session.has_value()) return pulse_error(400, "invalid_pulse_session");
      const auto ids = pulse_answer_ids(*session);
      if (ids.size() < pulse_questions().size()) return pulse_error(400, "pulse_session_incomplete");
      const auto game_key = canonical_seed_game_key(first_string_limited(*session, {"game_key"}, "", 120));
      if (game_key.empty()) return pulse_error(400, "missing_game_key");
      const auto game_name = seed_game_display_name(game_key, first_string_limited(*session, {"game_name"}, game_key, 120));
      const auto values = pulse_values_from_answers(game_key, ids);
      const auto requested_name = first_string_limited(body.value_or(nlohmann::json::object()), {"title", "name"}, "", 120);
      const auto name = requested_name.empty()
          ? sanitized_text("Pulse " + game_name + " " + utc_now_iso8601(), 120)
          : sanitized_text(requested_name, 120);
      nlohmann::json upstream_body = {
          {"game_key", game_key},
          {"name", name},
          {"description", "Created by Pulse from SekaiLink Easy Config."},
          {"values", values},
      };
      const auto upstream = forward_to_seed_config("POST", "/users/" + me + "/seed-configs", upstream_body);
      const auto parsed = parse_body_json(upstream);
      const auto status = response_status_from_body(upstream, parsed);
      if (status != 200 || !parsed.value("ok", false) || !parsed.contains("config") || !parsed.at("config").is_object()) {
        return {status, parsed.dump()};
      }
      const auto& config = parsed.at("config");
      const auto config_id = json_id_or(config, "config_id", json_id_or(config, "id"));
      const auto version_id = json_id_or(config, "version_id");
      const auto title = first_string_limited(config, {"name", "title"}, name, 120);
      if (const auto validation_error = validate_saved_seed_config(config_id, version_id, game_key, game_name, title);
          validation_error.has_value()) {
        return *validation_error;
      }
      nlohmann::json seed = {
          {"id", config_id},
          {"config_id", config_id},
          {"yaml_id", config_id},
          {"title", title},
          {"name", title},
          {"game", game_name},
          {"game_key", game_key},
          {"game_display_name", game_name},
          {"linkedworld_id", linkedworld_id_from_game(game_key)},
          {"player_name", identity_display_name(identity)},
          {"custom", true},
          {"source", "pulse.seed_config_api"},
          {"validation_status", config_.generation_handoff_command.empty() ? "skipped" : "validated"},
          {"values", config.contains("values") && config.at("values").is_object() ? config.at("values") : values},
      };
      if (!version_id.empty()) seed["version_id"] = version_id;
      return {200, nlohmann::json{{"ok", true}, {"seed", seed}, {"yaml", seed}, {"source", "pulse.seed_config_api"}}.dump()};
    }
    return pulse_error(404, "not_found");
  }
  if (method == "GET" && parts.size() == 1 && parts[0] == "yamls" &&
      !config_.seed_config_user_token.empty() && is_decimal_id_string(me)) {
    const auto upstream = forward_to_seed_config("GET", "/users/" + me + "/seed-configs", std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status == 200 && parsed.value("ok", false) && parsed.contains("configs") && parsed.at("configs").is_array()) {
      nlohmann::json yamls = nlohmann::json::array();
      for (const auto& config : parsed.at("configs")) {
        if (!config.is_object()) continue;
        const auto config_id = json_id_or(config, "config_id", json_id_or(config, "id"));
        if (config_id.empty()) continue;
        const auto version_id = json_id_or(config, "version_id");
        const auto game_key = first_string_limited(config, {"game_key"}, "", 120);
        const auto game_display_name = seed_game_display_name(
            game_key,
            first_string_limited(config, {"game_display_name", "game"}, game_key.empty() ? std::string("unknown") : game_key, 120));
        auto source = first_string_limited(config, {"source"}, "", 80);
        if (source.empty()) {
          const auto description = lower(first_string_limited(config, {"description"}, "", 240));
          const auto title = lower(first_string_limited(config, {"name", "title"}, config_id, 120));
          if (description.find("pulse") != std::string::npos || title.rfind("pulse ", 0) == 0) {
            source = "pulse";
          } else if (description.find("easy") != std::string::npos || title.rfind("easy ", 0) == 0) {
            source = "easy";
          } else {
            source = "nexus.seed_config_api";
          }
        }
        nlohmann::json item = {
            {"id", config_id},
            {"config_id", config_id},
            {"yaml_id", config_id},
            {"title", first_string_limited(config, {"name", "title"}, config_id, 120)},
            {"name", first_string_limited(config, {"name", "title"}, config_id, 120)},
            {"game", game_display_name},
            {"game_key", game_key},
            {"game_display_name", game_display_name},
            {"linkedworld_id", game_key.empty() ? linkedworld_id_from_game(game_display_name) : linkedworld_id_from_game(game_key)},
            {"player_name", identity_display_name(identity)},
            {"custom", true},
            {"source", source},
        };
        if (!version_id.empty()) item["version_id"] = version_id;
        if (config.contains("description") && config.at("description").is_string()) item["description"] = config.at("description");
        if (config.contains("values") && config.at("values").is_object()) item["values"] = config.at("values");
        if (config.contains("values_hash") && config.at("values_hash").is_string()) item["values_hash"] = config.at("values_hash");
        if (config.contains("updated_at") && config.at("updated_at").is_string()) item["updated_at"] = config.at("updated_at");
        yamls.push_back(item);
      }
      return {200, nlohmann::json{{"ok", true}, {"yamls", yamls}, {"source", "nexus.seed_config_api"}}.dump()};
    }
  }
  if (method == "GET" && parts.size() == 3 && parts[0] == "yamls" && parts[2] == "raw" &&
      !config_.seed_config_user_token.empty() && is_decimal_id_string(me)) {
    const auto id = url_decode(parts[1]);
    const auto upstream = forward_to_seed_config("POST", "/users/" + me + "/seed-configs/" + id + "/export-yaml", std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    if (status == 200 && parsed.value("ok", false) && parsed.contains("yaml") && parsed.at("yaml").is_string()) {
      const auto game_key = first_string_limited(parsed, {"game_key"}, "", 120);
      const auto game_display_name = seed_game_display_name(game_key, game_key.empty() ? std::string("unknown") : game_key);
      return {
          200,
          nlohmann::json{
              {"ok", true},
              {"id", id},
              {"config_id", id},
              {"yaml_id", id},
              {"yaml_title", id},
              {"title", id},
              {"game", game_display_name},
              {"game_key", game_key},
              {"game_display_name", game_display_name},
              {"content", parsed.at("yaml")},
              {"source", "nexus.seed_config_api"},
          }.dump()};
    }
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "yamls" && parts[2] == "duplicate" &&
      !config_.seed_config_user_token.empty() && is_decimal_id_string(me)) {
    const auto id = url_decode(parts[1]);
    const auto list = forward_to_seed_config("GET", "/users/" + me + "/seed-configs", std::nullopt);
    const auto parsed_list = parse_body_json(list);
    const auto list_status = response_status_from_body(list, parsed_list);
    if (list_status != 200 || !parsed_list.value("ok", false) || !parsed_list.contains("configs") || !parsed_list.at("configs").is_array()) {
      return {list_status, list.body};
    }
    std::optional<nlohmann::json> match;
    for (const auto& config : parsed_list.at("configs")) {
      if (!config.is_object()) continue;
      if (json_id_or(config, "config_id", json_id_or(config, "id")) == id) {
        match = config;
        break;
      }
    }
    if (!match.has_value()) return {404, R"({"ok":false,"error":"config_not_found"})"};
    auto game_key = canonical_seed_game_key(first_string_limited(*match, {"game_key"}, "", 120));
    if (game_key.empty()) return {400, R"({"ok":false,"error":"missing_game_key"})"};
    auto values = match->contains("values") && match->at("values").is_object()
        ? match->at("values")
        : nlohmann::json::object();
    values = explicit_seed_config_values(game_key, values);
    const auto base_name = first_string_limited(*match, {"name", "title"}, "Seed", 90);
    const auto copy_suffix = " Copy " + utc_now_iso8601();
    nlohmann::json upstream_body = {
        {"game_key", game_key},
        {"name", sanitized_text(base_name + copy_suffix, 120)},
        {"description", "Duplicated from SekaiLink Client Core."},
        {"values", values},
    };
    const auto duplicated = forward_to_seed_config("POST", "/users/" + me + "/seed-configs", upstream_body);
    const auto parsed = parse_body_json(duplicated);
    const auto status = response_status_from_body(duplicated, parsed);
    if (status == 200 && parsed.value("ok", false) && parsed.contains("config") && parsed.at("config").is_object()) {
      const auto& config = parsed.at("config");
      const auto config_id = json_id_or(config, "config_id", json_id_or(config, "id"));
      const auto version_id = json_id_or(config, "version_id");
      const auto game_display_name = seed_game_display_name(game_key, game_key);
      nlohmann::json item = {
          {"id", config_id},
          {"config_id", config_id},
          {"yaml_id", config_id},
          {"title", first_string_limited(config, {"name", "title"}, upstream_body.value("name", std::string("Seed Copy")), 120)},
          {"name", first_string_limited(config, {"name", "title"}, upstream_body.value("name", std::string("Seed Copy")), 120)},
          {"game", game_display_name},
          {"game_key", game_key},
          {"game_display_name", game_display_name},
          {"linkedworld_id", linkedworld_id_from_game(game_key)},
          {"player_name", identity_display_name(identity)},
          {"custom", true},
          {"source", "nexus.seed_config_api"},
      };
      if (!version_id.empty()) item["version_id"] = version_id;
      if (config.contains("values") && config.at("values").is_object()) item["values"] = config.at("values");
      if (config.contains("values_hash") && config.at("values_hash").is_string()) item["values_hash"] = config.at("values_hash");
      return {200, nlohmann::json{{"ok", true}, {"id", config_id}, {"yaml", item}, {"source", "nexus.seed_config_api"}}.dump()};
    }
    return {status, parsed.dump()};
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "yamls" && parts[2] == "delete" &&
      !config_.seed_config_user_token.empty() && is_decimal_id_string(me)) {
    const auto id = url_decode(parts[1]);
    const auto upstream = forward_to_seed_config("DELETE", "/users/" + me + "/seed-configs/" + id, std::nullopt);
    const auto parsed = parse_body_json(upstream);
    const auto status = response_status_from_body(upstream, parsed);
    return {status, parsed.dump()};
  }
  if (method == "POST" && ((parts.size() == 1 && parts[0] == "yamls") || (parts.size() == 2 && parts[0] == "yamls" && parts[1] == "new")) &&
      !config_.seed_config_user_token.empty() && is_decimal_id_string(me)) {
    const auto request = body.value_or(nlohmann::json::object());
    auto game_key = canonical_seed_game_key(first_string_limited(request, {"game_key"}, "", 120));
    if (game_key.empty()) {
      const auto game = first_string_limited(request, {"game"}, "", 120);
      game_key = canonical_seed_game_key(game);
    }
    if (!game_key.empty()) {
      const auto values = request.contains("values") && request.at("values").is_object()
          ? request.at("values")
          : nlohmann::json::object();
      const auto requested_source = first_string_limited(request, {"source", "mode"}, "", 80);
      nlohmann::json upstream_body = {
          {"game_key", game_key},
          {"name", first_string_limited(request, {"title", "name"}, "SekaiLink Seed", 120)},
          {"description", first_string_limited(request, {"description"}, "Created from SekaiLink Client Core.", 240)},
          {"values", values},
      };
      const auto upstream = forward_to_seed_config("POST", "/users/" + me + "/seed-configs", upstream_body);
      const auto parsed = parse_body_json(upstream);
      const auto status = response_status_from_body(upstream, parsed);
      if (status == 200 && parsed.value("ok", false) && parsed.contains("config") && parsed.at("config").is_object()) {
        const auto& config = parsed.at("config");
        const auto config_id = json_id_or(config, "config_id", json_id_or(config, "id"));
        const auto version_id = json_id_or(config, "version_id");
        const auto game_display_name = seed_game_display_name(game_key, first_string_limited(request, {"game"}, game_key, 120));
        nlohmann::json item = {
            {"id", config_id},
            {"config_id", config_id},
            {"yaml_id", config_id},
            {"title", first_string_limited(config, {"name", "title"}, upstream_body.value("name", std::string("SekaiLink Seed")), 120)},
            {"name", first_string_limited(config, {"name", "title"}, upstream_body.value("name", std::string("SekaiLink Seed")), 120)},
            {"game", game_display_name},
            {"game_key", game_key},
            {"game_display_name", game_display_name},
            {"linkedworld_id", linkedworld_id_from_game(game_key)},
            {"player_name", identity_display_name(identity)},
            {"custom", true},
            {"source", requested_source.empty() ? std::string("nexus.seed_config_api") : requested_source},
        };
        if (!version_id.empty()) item["version_id"] = version_id;
        return {200, nlohmann::json{{"ok", true}, {"id", config_id}, {"yaml", item}, {"source", "nexus.seed_config_api"}}.dump()};
      }
      if (status != 404) {
        return {status, parsed.dump()};
      }
    }
  }
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
