std::string TrackerHeadlessRuntime::ResolveActiveMapId() const {
    if (!state_.auto_follow && !state_.active_map_id.empty()) {
        return state_.active_map_id;
    }
    if (const auto* tab = FindTab(bundle_, state_.active_tab_id); tab != nullptr && !tab->map_id.empty()) {
        return tab->map_id;
    }
    const auto map_hint = bundle_.tab_map_hints.find(state_.active_tab_id);
    if (map_hint != bundle_.tab_map_hints.end() && !map_hint->second.empty()) {
        return map_hint->second;
    }
    return state_.active_map_id.empty() ? bundle_.default_map_id : state_.active_map_id;
}

bool TrackerHeadlessRuntime::EvaluatePackLogic(std::string* error) {
    const auto poptracker_root = resolved_bundle_path_ / "poptracker-adapted";
    const auto logic_root = poptracker_root / "scripts" / "logic";
    if (!std::filesystem::exists(logic_root)) {
        state_.logic_ready = false;
        state_.logic_groups.clear();
        state_.pack_location_states.clear();
        state_.pack_section_states.clear();
        state_.runtime_detailed_pins.clear();
        state_.logic_item_slots.clear();
        state_.logic_pack_code_states.clear();
        return true;
    }

    const auto script_path = std::filesystem::path(__FILE__).parent_path() / "tracker_poptracker_eval.lua";
    if (!std::filesystem::exists(script_path)) {
        if (error) *error = "tracker_logic_runner_missing";
        return false;
    }

    const auto logic_state_text = BuildLogicStateFileText(resolved_bundle_path_, state_, bundle_);
    if (logic_state_cached_ && logic_state_text == last_logic_state_text_) {
        return true;
    }

    const auto state_path = config_.snapshot_path.parent_path() / "tracker.logic.state";
    const auto output_path = config_.snapshot_path.parent_path() / "tracker.logic.result";
    const auto log_path = config_.snapshot_path.parent_path() / "tracker.logic.log";
    {
        std::ofstream output(state_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "tracker_logic_state_open_failed";
            return false;
        }
        output << logic_state_text;
    }

    std::error_code ec;
    std::filesystem::remove(output_path, ec);

    const std::string runner_prefix =
#if defined(_WIN32)
        "luajit ";
#else
        "nice -n 15 luajit ";
#endif
    const std::string command =
        runner_prefix + ShellEscapeSingleQuotes(script_path.string()) +
        " " + ShellEscapeSingleQuotes(resolved_bundle_path_.string()) +
        " " + ShellEscapeSingleQuotes(state_path.string()) +
        " " + ShellEscapeSingleQuotes(output_path.string()) +
        " > " + ShellEscapeSingleQuotes(log_path.string()) + " 2>&1";
    const int result = std::system(command.c_str());
    if (result != 0 || !std::filesystem::exists(output_path)) {
        state_.logic_ready = false;
        state_.logic_groups.clear();
        state_.pack_location_states.clear();
        state_.pack_section_states.clear();
        state_.runtime_detailed_pins.clear();
        state_.logic_item_slots.clear();
        state_.logic_pack_code_states.clear();
        if (error) *error = "tracker_logic_runner_failed";
        return false;
    }

    state_.logic_groups.clear();
    state_.pack_location_states.clear();
    state_.pack_section_states.clear();
    state_.runtime_detailed_pins.clear();
    state_.logic_item_slots.clear();
    state_.logic_pack_code_states.clear();
    std::ifstream input(output_path);
    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "logic_ready") {
            state_.logic_ready = (key == "1" || value == "1");
            continue;
        }
        if (kind == "item_state") {
            std::stringstream fields(value);
            std::string owned;
            std::string stage;
            std::string count;
            std::getline(fields, owned, '|');
            std::getline(fields, stage, '|');
            std::getline(fields, count, '|');
            TrackerItemSlotState item_state;
            item_state.owned = owned == "1" || owned == "true";
            item_state.stage = detail::parse_u64(stage).value_or(0);
            item_state.count = detail::parse_u64(count).value_or(0);
            if (state_.item_slots.contains(key)) {
                state_.logic_item_slots[key] = item_state;
            } else if (IsKnownPackItemCode(bundle_, key)) {
                state_.logic_pack_code_states[key] = item_state;
            }
            continue;
        }
        if (kind == "pin") {
            std::stringstream fields(value);
            std::string group_id;
            std::string pack_location_id;
            std::string section_id;
            std::string location_id;
            std::string location_name;
            std::string pack_map;
            std::string x;
            std::string y;
            std::string size;
            std::getline(fields, group_id, '|');
            std::getline(fields, pack_location_id, '|');
            std::getline(fields, section_id, '|');
            std::getline(fields, location_id, '|');
            std::getline(fields, location_name, '|');
            std::getline(fields, pack_map, '|');
            std::getline(fields, x, '|');
            std::getline(fields, y, '|');
            std::getline(fields, size, '|');
            auto parsed_location_id = detail::parse_u64(location_id);
            if (key.empty() || pack_location_id.empty() || pack_map.empty() || !parsed_location_id.has_value()) {
                continue;
            }
            TrackerDetailedPinDefinition pin;
            pin.pin_id = key;
            pin.group_id = group_id;
            pin.pack_location_id = pack_location_id;
            pin.section_id = section_id;
            pin.location_id = *parsed_location_id;
            pin.location_name = location_name;
            pin.pack_map = pack_map;
            pin.map_asset = FindPackMapAsset(bundle_, pack_map);
            pin.map_id = ResolveDetailedPinMapId(bundle_, pack_map, group_id);
            if (const auto* layer = FindPinLayer(bundle_, group_id); layer != nullptr) {
                pin.pin_kind = layer->pin_kind;
            }
            if (pin.pin_kind.empty()) {
                pin.pin_kind = "check";
            }
            pin.x = ParseDoubleOrZero(x);
            pin.y = ParseDoubleOrZero(y);
            pin.size = ParseDoubleOrZero(size);
            state_.runtime_detailed_pins.push_back(std::move(pin));
            continue;
        }
        if (kind != "group" && kind != "pack_location" && kind != "section") continue;
        std::stringstream fields(value);
        TrackerLogicGroupState group;
        std::string part;
        std::getline(fields, group.color, '|');
        std::getline(fields, part, '|');
        group.normal_count = detail::parse_u64(part).value_or(0);
        std::getline(fields, part, '|');
        group.sequence_break_count = detail::parse_u64(part).value_or(0);
        std::getline(fields, part, '|');
        group.inspect_count = detail::parse_u64(part).value_or(0);
        std::getline(fields, part, '|');
        group.none_count = detail::parse_u64(part).value_or(0);
        if (kind == "group") {
            state_.logic_groups[key] = std::move(group);
        } else if (kind == "pack_location") {
            state_.pack_location_states[key] = std::move(group);
        } else {
            state_.pack_section_states[key] = std::move(group);
        }
    }

    if (!state_.logic_ready) {
        state_.logic_groups.clear();
        state_.pack_location_states.clear();
        state_.pack_section_states.clear();
        state_.runtime_detailed_pins.clear();
        state_.logic_item_slots.clear();
        state_.logic_pack_code_states.clear();
    }
    last_logic_state_text_ = logic_state_text;
    logic_state_cached_ = true;
    return true;
}
