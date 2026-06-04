bool TrackerHeadlessRuntime::ResolveBundlePath(std::string* error) {
    resolved_bundle_path_ = config_.bundle_path;
    extracted_bundle_root_.clear();
    if (config_.bundle_path.empty()) {
        if (error) *error = "tracker_bundle_missing";
        return false;
    }
    if (std::filesystem::is_directory(config_.bundle_path)) {
        return true;
    }
    if (!std::filesystem::is_regular_file(config_.bundle_path)) {
        if (error) *error = "tracker_bundle_path_missing";
        return false;
    }
    const auto extension = config_.bundle_path.extension().string();
    if (extension != ".zip" && extension != ".pt" && extension != ".bundle") {
        if (error) *error = "tracker_bundle_path_unsupported";
        return false;
    }

    const auto root = !config_.snapshot_path.empty()
                          ? config_.snapshot_path.parent_path() / "tracker.pack.extracted"
                          : std::filesystem::temp_directory_path() / "sklmi-tracker-pack.extracted";
    extracted_bundle_root_ = root;
    return ExtractTrackerArchive(config_.bundle_path, extracted_bundle_root_, &resolved_bundle_path_, error);
}

void TrackerHeadlessRuntime::LoadRoomMetadata() {
    const auto fallback_player_alias =
        !state_.player_alias.empty() ? state_.player_alias : config_.initial_player_alias;
    state_.room_metadata.clear();
    state_.slot_data_json.clear();
    std::vector<std::filesystem::path> candidates;
    if (!config_.room_state_path.empty()) {
        candidates.push_back(config_.room_state_path);
        const auto bridge_root = config_.room_state_path.parent_path();
        const auto runtime_root = bridge_root / "runtime";
        if (std::filesystem::exists(runtime_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(runtime_root, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto filename = entry.path().filename().string();
                if (filename.find(".room-sync.state") != std::string::npos) {
                    candidates.push_back(entry.path());
                }
            }
        }
    }

    for (const auto& path : candidates) {
        if (path.empty() || !std::filesystem::exists(path)) {
            continue;
        }
        std::ifstream input(path);
        if (!input) {
            continue;
        }

        std::string line;
        while (std::getline(input, line)) {
            std::string kind;
            std::string key;
            std::string value;
            if (!detail::parse_state_line(line, &kind, &key, &value) || kind != "meta") continue;
            state_.room_metadata[key] = value;
            if (key == "slot_id" || key == "slot") {
                state_.slot_id = value;
            } else if (key == "seed_id") {
                state_.seed_id = value;
            } else if (key == "seed_name" && state_.seed_id.empty()) {
                state_.seed_id = value;
            } else if (key == "slot_name") {
                state_.slot_name = value;
            } else if (key == "player_alias" || key == "slot_alias") {
                state_.player_alias = value;
            } else if (key == "room_id") {
                state_.room_id = value;
            } else if (key == "slot_data") {
                state_.slot_data_json = value;
            } else if (key == "tracker_pack") {
                state_.tracker_pack = value;
            } else if (key == "tracker_variant" && state_.tracker_variant.empty()) {
                state_.tracker_variant = value;
            }
        }
    }

    if (state_.player_alias.empty()) {
        state_.player_alias = fallback_player_alias;
    }
    if (!state_.player_alias.empty()) {
        state_.room_metadata["player_alias"] = state_.player_alias;
        state_.room_metadata["player_display_name"] = state_.player_alias;
    }
}

std::filesystem::path TrackerHeadlessRuntime::AutosavePath() const {
    if (!config_.snapshot_path.empty()) {
        return config_.snapshot_path.parent_path() / "tracker.autosave.state";
    }
    if (!config_.command_log_path.empty()) {
        return config_.command_log_path.parent_path() / "tracker.autosave.state";
    }
    return {};
}

bool TrackerHeadlessRuntime::LoadAutosaveState() {
    const auto path = AutosavePath();
    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }
    std::ifstream input(path);
    if (!input) {
        return false;
    }

    std::string saved_slot_id;
    std::string saved_seed_id;
    std::string active_tab_id;
    std::string active_map_id;
    std::optional<bool> auto_follow;
    std::unordered_map<std::string, TrackerItemSlotState> item_slots;
    std::unordered_set<std::uint64_t> checked_locations;
    std::unordered_set<std::string> received_delivery_ids;
    std::vector<TrackerReceivedItemEvent> received_item_events;

    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "meta") {
            if (key == "slot_id") saved_slot_id = value;
            if (key == "seed_id") saved_seed_id = value;
            continue;
        }
        if (kind == "ui") {
            if (key == "active_tab_id") active_tab_id = value;
            if (key == "active_map_id") active_map_id = value;
            if (key == "auto_follow") auto_follow = (value == "1" || value == "true");
            continue;
        }
        if (kind == "item") {
            std::stringstream fields(value);
            std::string owned;
            std::string stage;
            std::string count;
            std::getline(fields, owned, '|');
            std::getline(fields, stage, '|');
            std::getline(fields, count, '|');
            auto& slot = item_slots[key];
            slot.owned = owned == "1" || owned == "true";
            slot.stage = detail::parse_u64(stage).value_or(0);
            slot.count = detail::parse_u64(count).value_or(0);
            continue;
        }
        if (kind == "source") {
            item_slots[key].owned_sources.insert(value);
            continue;
        }
        if (kind == "check") {
            if (auto location_id = detail::parse_u64(key); location_id.has_value() && (value == "1" || value == "true")) {
                checked_locations.insert(*location_id);
            }
            continue;
        }
        if (kind == "delivery") {
            if (!key.empty() && (value == "1" || value == "true")) {
                received_delivery_ids.insert(key);
            }
            continue;
        }
        if (kind == "ap_item") {
            std::stringstream fields(value);
            std::string index;
            std::string item_id;
            std::string player_number;
            std::getline(fields, index, '|');
            std::getline(fields, item_id, '|');
            std::getline(fields, player_number, '|');
            std::string item_name;
            std::getline(fields, item_name);
            received_delivery_ids.insert(key);
            received_item_events.push_back(TrackerReceivedItemEvent{
                key,
                detail::parse_u64(index).value_or(received_item_events.size()),
                detail::parse_u64(item_id).value_or(0),
                detail::parse_u64(player_number).value_or(0),
                item_name,
            });
        }
    }

    if (!saved_slot_id.empty() && !state_.slot_id.empty() && saved_slot_id != state_.slot_id) {
        return false;
    }
    if (!saved_seed_id.empty() && !state_.seed_id.empty() && saved_seed_id != state_.seed_id) {
        return false;
    }

    for (auto& [slot_id, saved_state] : item_slots) {
        if (state_.item_slots.contains(slot_id)) {
            state_.item_slots[slot_id] = std::move(saved_state);
        } else if (IsKnownPackItemCode(bundle_, slot_id)) {
            state_.pack_code_states[slot_id] = std::move(saved_state);
        }
    }
    for (const auto location_id : checked_locations) {
        state_.checked_locations.insert(location_id);
    }
    for (const auto& delivery_id : received_delivery_ids) {
        state_.received_delivery_ids.insert(delivery_id);
    }
    state_.received_item_events.insert(state_.received_item_events.end(),
                                       received_item_events.begin(),
                                       received_item_events.end());
    if (!active_tab_id.empty()) state_.active_tab_id = active_tab_id;
    if (!active_map_id.empty()) state_.active_map_id = active_map_id;
    if (auto_follow.has_value()) state_.auto_follow = *auto_follow;
    autosave_dirty_ = false;
    return true;
}

bool TrackerHeadlessRuntime::SaveAutosaveState(std::string* error) {
    if (!autosave_dirty_) {
        return true;
    }
    const auto path = AutosavePath();
    if (path.empty()) {
        autosave_dirty_ = false;
        return true;
    }

    std::ostringstream output;
    output << "meta|schema|sekailink.tracker.autosave.v1\n";
    output << "meta|slot_id|" << detail::state_file_escape(state_.slot_id) << "\n";
    output << "meta|seed_id|" << detail::state_file_escape(state_.seed_id) << "\n";
    output << "ui|active_tab_id|" << detail::state_file_escape(state_.active_tab_id) << "\n";
    output << "ui|active_map_id|" << detail::state_file_escape(state_.active_map_id) << "\n";
    output << "ui|auto_follow|" << (state_.auto_follow ? "1" : "0") << "\n";
    for (const auto& slot : bundle_.item_slots) {
        const auto state_it = state_.item_slots.find(slot.slot_id);
        if (state_it == state_.item_slots.end()) continue;
        const auto& item_state = state_it->second;
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0 || !item_state.owned_sources.empty())) {
            continue;
        }
        output << "item|" << detail::state_file_escape(slot.slot_id) << "|"
               << (item_state.owned ? "1" : "0") << "|"
               << item_state.stage << "|"
               << item_state.count << "\n";
        for (const auto& source : item_state.owned_sources) {
            output << "source|" << detail::state_file_escape(slot.slot_id) << "|"
                   << detail::state_file_escape(source) << "\n";
        }
    }
    std::vector<std::string> pack_codes;
    pack_codes.reserve(state_.pack_code_states.size());
    for (const auto& [code, _] : state_.pack_code_states) {
        pack_codes.push_back(code);
    }
    std::sort(pack_codes.begin(), pack_codes.end());
    for (const auto& code : pack_codes) {
        const auto& item_state = state_.pack_code_states.at(code);
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0 || !item_state.owned_sources.empty())) {
            continue;
        }
        output << "item|" << detail::state_file_escape(code) << "|"
               << (item_state.owned ? "1" : "0") << "|"
               << item_state.stage << "|"
               << item_state.count << "\n";
    }
    for (const auto location_id : state_.checked_locations) {
        output << "check|" << location_id << "|1\n";
    }
    for (const auto& delivery_id : state_.received_delivery_ids) {
        output << "delivery|" << detail::state_file_escape(delivery_id) << "|1\n";
    }
    for (const auto& item : state_.received_item_events) {
        output << "ap_item|" << detail::state_file_escape(item.delivery_id) << "|"
               << item.index << "|"
               << item.item_id << "|"
               << item.player_number << "|"
               << detail::state_file_escape(item.item_name) << "\n";
    }

    if (!AtomicWriteTextFile(path, output.str(), error)) {
        return false;
    }
    autosave_dirty_ = false;
    return true;
}
