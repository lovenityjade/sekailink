void TrackerHeadlessRuntime::ApplyRoomItemIdentity(const TrackerItemIdentity& identity, std::string delivery_id) {
    bool applied = false;
    for (const auto& slot : bundle_.item_slots) {
        for (const auto& slot_item : slot.items) {
            if (!MatchesIdentity(slot_item, identity.item_id, identity.event_key,
                                 !identity.item_name.empty() ? identity.item_name : identity.mapped_value)) {
                continue;
            }
            auto& state = state_.item_slots[slot.slot_id];
            if (slot.behavior == "progressive") {
                const auto max_stage = slot.max_stage == 0 ? 1 : slot.max_stage;
                state.stage = std::min<std::uint64_t>(max_stage, state.stage + 1);
                state.owned = state.stage > 0;
            } else if (slot.behavior == "toggle") {
                state.owned = true;
                state.stage = 1;
            } else if (slot.behavior == "multi-source-toggle") {
                state.owned = true;
                state.stage = 1;
                state.owned_sources.insert(DisplayIdentityForItem(slot_item));
                state.count = state.owned_sources.size();
            } else if (slot.behavior == "counter" || slot.behavior == "resource") {
                state.count += 1;
                state.owned = state.count > 0;
            } else {
                state.owned = true;
                state.stage = state.stage == 0 ? 1 : state.stage;
            }
            state_.last_received_label = slot.label;
            applied = true;
            break;
        }
        if (applied) {
            break;
        }
    }
    const bool pack_applied = ApplyPackItemMapping(identity);
    if (applied || pack_applied) {
        if (!delivery_id.empty()) {
            state_.received_delivery_ids.insert(delivery_id);
        }
        autosave_dirty_ = true;
    }
}

bool TrackerHeadlessRuntime::ApplyPackItemMapping(const TrackerItemIdentity& identity) {
    if (identity.item_id == 0) {
        return false;
    }
    bool applied = false;
    for (const auto& mapping : bundle_.pack_item_mappings) {
        if (mapping.item_id != identity.item_id) {
            continue;
        }
        for (const auto& code : mapping.codes) {
            if (code.empty() || IsNormalizedItemSlotCode(bundle_, code)) {
                continue;
            }
            auto& state = state_.pack_code_states[code];
            ApplyPackCodeStateForBehavior(state, mapping.behavior);
            const auto label = PackCodeLabel(bundle_, code);
            if (!label.empty()) {
                state.owned_sources.insert(label);
                state_.last_received_label = label;
            }
            applied = true;
        }
        break;
    }
    return applied;
}

std::optional<std::uint64_t> ParsePositiveU64(std::string_view text) {
    if (text.empty()) return std::nullopt;
    std::uint64_t value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') return std::nullopt;
        value = value * 10 + static_cast<std::uint64_t>(ch - '0');
    }
    return value;
}

bool HasPopTrackerInitScript(const std::filesystem::path& resolved_bundle_path) {
    return std::filesystem::exists(resolved_bundle_path / "poptracker-adapted" / "scripts" / "init.lua");
}

TrackerReceivedItemEvent BuildReceivedItemEvent(const Event& event,
                                                std::string delivery_id,
                                                std::uint64_t fallback_index,
                                                std::uint64_t fallback_player) {
    TrackerReceivedItemEvent item;
    item.delivery_id = std::move(delivery_id);
    item.index = fallback_index;
    item.item_id = event.canonical_id;
    item.player_number = event.player_number != 0 ? event.player_number : fallback_player;
    item.item_name = event.value;

    if (event.key.rfind("ap:", 0) == 0) {
        std::stringstream fields(event.key);
        std::string part;
        std::getline(fields, part, ':');
        if (std::getline(fields, part, ':')) {
            item.index = ParsePositiveU64(part).value_or(item.index);
        }
        if (std::getline(fields, part, ':')) {
            item.item_id = ParsePositiveU64(part).value_or(item.item_id);
        }
        std::getline(fields, part, ':');
        if (std::getline(fields, part, ':')) {
            item.player_number = ParsePositiveU64(part).value_or(item.player_number);
        }
    } else if (event.key.rfind("delivery:", 0) == 0) {
        item.index = ParsePositiveU64(std::string_view(event.key).substr(9)).value_or(item.index);
    }
    if (item.player_number == 0) {
        item.player_number = 1;
    }
    return item;
}

bool TrackerHeadlessRuntime::ApplyAutoTabForLocation(std::uint64_t canonical_id, std::string_view event_key) {
    if (!state_.auto_follow) {
        return false;
    }
    const auto* group = FindLocationGroupByLocation(bundle_, canonical_id, event_key);
    if (group == nullptr) {
        return false;
    }

    auto tab_id = group->preferred_tab;
    const auto hint = bundle_.location_group_tab_hints.find(group->group_id);
    if (hint != bundle_.location_group_tab_hints.end() && !hint->second.empty()) {
        tab_id = hint->second;
    }
    if (tab_id.empty()) {
        return false;
    }
    const auto* tab = FindTab(bundle_, tab_id);
    if (tab == nullptr) {
        return false;
    }

    std::string map_id = tab->map_id;
    const auto map_hint = bundle_.tab_map_hints.find(tab_id);
    if (map_hint != bundle_.tab_map_hints.end() && !map_hint->second.empty()) {
        map_id = map_hint->second;
    }

    bool changed = false;
    if (state_.active_tab_id != tab_id) {
        state_.active_tab_id = tab_id;
        changed = true;
    }
    if (!map_id.empty() && state_.active_map_id != map_id) {
        state_.active_map_id = std::move(map_id);
        changed = true;
    }
    if (changed) {
        autosave_dirty_ = true;
    }
    return changed;
}

bool TrackerHeadlessRuntime::SetAutoFollowMap(bool enabled) {
    state_.auto_follow = enabled;
    if (!enabled) {
        return true;
    }

    std::string target_map = bundle_.default_map_id;
    std::string target_tab = bundle_.default_tab_id;
    if (target_map.empty() && !bundle_.maps.empty()) {
        target_map = bundle_.maps.front().id;
    }
    if (!target_map.empty()) {
        for (const auto& tab : bundle_.tabs) {
            const auto map_hint = bundle_.tab_map_hints.find(tab.id);
            const auto tab_map = map_hint != bundle_.tab_map_hints.end() && !map_hint->second.empty()
                                     ? map_hint->second
                                     : tab.map_id;
            if (tab_map == target_map) {
                target_tab = tab.id;
                break;
            }
        }
    } else if (!target_tab.empty()) {
        if (const auto* tab = FindTab(bundle_, target_tab); tab != nullptr) {
            target_map = tab->map_id;
        }
    }
    if (!target_tab.empty()) {
        state_.active_tab_id = target_tab;
    }
    if (!target_map.empty()) {
        state_.active_map_id = target_map;
    } else {
        state_.active_map_id = ResolveActiveMapId();
    }
    return true;
}

void TrackerHeadlessRuntime::ApplyLocationCheck(std::uint64_t canonical_id,
                                                std::string_view event_key,
                                                std::string_view value) {
    if (canonical_id != 0) {
        state_.checked_locations.insert(canonical_id);
    } else {
        const auto* location = FindLocationByAnyId(bundle_, std::string(event_key));
        if (location != nullptr && location->location_id != 0) {
            state_.checked_locations.insert(location->location_id);
        }
    }
    state_.last_check_label = std::string(value);
    ApplyAutoTabForLocation(canonical_id, event_key);
    autosave_dirty_ = true;
}

bool TrackerHeadlessRuntime::ApplyMapChangedEvent(const Event& event) {
    if (!state_.auto_follow) {
        return false;
    }

    std::string tab_id = event.tab_id;
    std::string map_id = event.map_id;
    const auto apply_zone_hint = [&](std::string_view zone_id) {
        if (zone_id.empty()) return;
        const auto hint = bundle_.zone_tab_hints.find(std::string(zone_id));
        if (hint == bundle_.zone_tab_hints.end()) return;
        if (tab_id.empty()) tab_id = hint->second.tab_id;
        if (map_id.empty()) map_id = hint->second.map_id;
    };
    apply_zone_hint(event.zone_id);
    apply_zone_hint(event.key);
    apply_zone_hint(event.value);

    if (!tab_id.empty()) {
        const auto* tab = FindTab(bundle_, tab_id);
        if (tab == nullptr) {
            return false;
        }
        if (map_id.empty()) {
            const auto map_hint = bundle_.tab_map_hints.find(tab_id);
            map_id = map_hint != bundle_.tab_map_hints.end() && !map_hint->second.empty()
                         ? map_hint->second
                         : tab->map_id;
        }
    }

    if (tab_id.empty() && !map_id.empty()) {
        for (const auto& tab : bundle_.tabs) {
            const auto map_hint = bundle_.tab_map_hints.find(tab.id);
            const auto candidate_map = map_hint != bundle_.tab_map_hints.end() && !map_hint->second.empty()
                                           ? map_hint->second
                                           : tab.map_id;
            if (candidate_map == map_id) {
                tab_id = tab.id;
                break;
            }
        }
    }

    if (tab_id.empty() && map_id.empty()) {
        return false;
    }

    bool changed = false;
    if (!tab_id.empty() && state_.active_tab_id != tab_id) {
        state_.active_tab_id = tab_id;
        changed = true;
    }
    if (!map_id.empty() && state_.active_map_id != map_id) {
        state_.active_map_id = map_id;
        changed = true;
    }
    if (changed) {
        autosave_dirty_ = true;
    }
    return changed;
}

void TrackerHeadlessRuntime::ApplyEvent(const Event& event) {
    if (event.type == EventType::location_checked) {
        ApplyLocationCheck(event.canonical_id, event.key, event.value);
        return;
    }
    if (event.type == EventType::map_changed) {
        ApplyMapChangedEvent(event);
        return;
    }
    if (event.type != EventType::item_received) {
        return;
    }
    std::string delivery_id;
    if (IsRoomDeliveryKey(event.key)) {
        delivery_id = event.key;
    } else if (event.canonical_id != 0) {
        delivery_id = std::to_string(event.canonical_id);
    } else {
        delivery_id = event.key;
    }
    if (!delivery_id.empty() && state_.received_delivery_ids.contains(delivery_id)) {
        return;
    }
    if (event.canonical_id != 0 && IsRoomDeliveryKey(event.key) && HasPopTrackerInitScript(resolved_bundle_path_)) {
        state_.received_delivery_ids.insert(delivery_id);
        state_.received_item_events.push_back(BuildReceivedItemEvent(event,
                                                                     delivery_id,
                                                                     state_.received_item_events.size(),
                                                                     ParsePositiveU64(state_.slot_id).value_or(1)));
        state_.last_received_label = event.value;
        autosave_dirty_ = true;
        return;
    }
    ApplyRoomItemIdentity({event.canonical_id, event.key, event.value, event.value}, delivery_id);
}

bool CommandStateHasValue(const TrackerItemSlotState& state) {
    return state.owned || state.stage > 0 || state.count > 0 || !state.owned_sources.empty();
}

bool TrackerHeadlessRuntime::ApplyClickItemCommand(std::string_view slot_id, std::string_view button) {
    auto found = state_.item_slots.find(std::string(slot_id));
    const auto it = std::find_if(bundle_.item_slots.begin(), bundle_.item_slots.end(),
                                 [&](const auto& slot) { return slot.slot_id == slot_id; });
    if (found != state_.item_slots.end() && it != bundle_.item_slots.end()) {
        auto& state = found->second;
        if (!CommandStateHasValue(state)) {
            if (const auto logic_it = state_.logic_item_slots.find(std::string(slot_id));
                logic_it != state_.logic_item_slots.end()) {
                state = logic_it->second;
            }
        }
        state_.logic_item_slots.erase(std::string(slot_id));
        const auto* icon_binding = FindItemIconBinding(bundle_, it->slot_id);
        const auto* pack_visual = FindPackItemVisualForSlot(bundle_, *it, icon_binding);
        const bool progressive_visual =
            pack_visual != nullptr && (pack_visual->type == "progressive" ||
                                       pack_visual->type == "progressive-toggle" ||
                                       pack_visual->type == "progressive_toggle");
        if (it->behavior == "progressive" || it->behavior == "variant-family" || progressive_visual) {
            std::uint64_t max_stage = it->max_stage == 0 ? 1 : it->max_stage;
            if (pack_visual != nullptr && !pack_visual->stages.empty()) {
                max_stage = static_cast<std::uint64_t>(pack_visual->stages.size());
                if (!pack_visual->allow_disabled || pack_visual->initial_active_state) {
                    max_stage = max_stage == 0 ? 0 : max_stage - 1;
                }
            }
            if (button == "right") {
                state.stage = state.stage == 0 ? 0 : state.stage - 1;
            } else if (max_stage > 0 && pack_visual != nullptr && pack_visual->loop_stages &&
                       state.stage >= max_stage) {
                state.stage = 0;
            } else {
                state.stage = std::min<std::uint64_t>(max_stage, state.stage + 1);
            }
            state.owned = state.stage > 0;
        } else if (it->behavior == "counter" || it->behavior == "resource") {
            if (button == "right") {
                state.count = state.count == 0 ? 0 : state.count - 1;
            } else {
                state.count += 1;
            }
            state.owned = state.count > 0;
        } else {
            state.owned = button == "right" ? false : !state.owned;
            state.stage = state.owned ? 1 : 0;
        }
        autosave_dirty_ = true;
        return true;
    }

    const auto* pack_visual = FindPackItemVisual(bundle_, slot_id);
    if (pack_visual == nullptr || pack_visual->type == "static") {
        return false;
    }
    auto& state = state_.pack_code_states[std::string(slot_id)];
    if (!CommandStateHasValue(state)) {
        if (const auto logic_it = state_.logic_pack_code_states.find(std::string(slot_id));
            logic_it != state_.logic_pack_code_states.end()) {
            state = logic_it->second;
        }
    }
    state_.logic_pack_code_states.erase(std::string(slot_id));
    const bool progressive_visual = pack_visual->type == "progressive" ||
                                    pack_visual->type == "progressive-toggle" ||
                                    pack_visual->type == "progressive_toggle";
    if (progressive_visual) {
        std::uint64_t max_stage = pack_visual->stages.empty()
                                      ? 1
                                      : static_cast<std::uint64_t>(pack_visual->stages.size());
        if (!pack_visual->stages.empty() &&
            (!pack_visual->allow_disabled || pack_visual->initial_active_state)) {
            max_stage = max_stage == 0 ? 0 : max_stage - 1;
        }
        if (button == "right") {
            state.stage = state.stage == 0 ? 0 : state.stage - 1;
        } else if (max_stage > 0 && pack_visual->loop_stages && state.stage >= max_stage) {
            state.stage = 0;
        } else {
            state.stage = std::min<std::uint64_t>(max_stage, state.stage + 1);
        }
        state.owned = state.stage > 0;
    } else if (pack_visual->type == "counter" || pack_visual->type == "resource" ||
               pack_visual->type == "consumable") {
        const auto increment = std::max<std::uint64_t>(1, pack_visual->increment);
        if (button == "right") {
            state.count = state.count <= increment ? 0 : state.count - increment;
        } else {
            state.count += increment;
            if (pack_visual->max_quantity > 0) {
                state.count = std::min<std::uint64_t>(state.count, pack_visual->max_quantity);
            }
        }
        state.owned = state.count > 0;
    } else {
        state.owned = button == "right" ? false : !state.owned;
        state.stage = state.owned ? 1 : 0;
    }
    autosave_dirty_ = true;
    return true;
}

bool TrackerHeadlessRuntime::ApplyClickPinCommand(std::string_view location_or_group_id, std::string_view button) {
    const auto* location = FindLocationByAnyId(bundle_, location_or_group_id);
    if (location != nullptr && location->location_id != 0) {
        if (button == "right") {
            state_.checked_locations.erase(location->location_id);
        } else {
            state_.checked_locations.insert(location->location_id);
            ApplyLocationCheck(location->location_id, location->event_key, location->location_name);
        }
        autosave_dirty_ = true;
        return true;
    }

    for (const auto& group : bundle_.location_groups) {
        if (group.group_id != location_or_group_id) continue;
        if (button == "right") {
            for (const auto& candidate : group.locations) {
                if (state_.checked_locations.erase(candidate.location_id) > 0) {
                    break;
                }
            }
        } else {
            for (const auto& candidate : group.locations) {
                if (!state_.checked_locations.contains(candidate.location_id)) {
                    state_.checked_locations.insert(candidate.location_id);
                    ApplyLocationCheck(candidate.location_id, candidate.event_key, candidate.location_name);
                    break;
                }
            }
        }
        autosave_dirty_ = true;
        return true;
    }
    return false;
}

bool TrackerHeadlessRuntime::ApplyCommandLine(const std::string& line) {
    const auto command = ParseCommandString(line, "cmd").value_or("");
    if (command == "tracker.set_map") {
        const auto map = ParseCommandString(line, "map").value_or(ParseCommandString(line, "map_id").value_or(""));
        if (map.empty()) {
            return false;
        }
        bool known_map = false;
        for (const auto& candidate : bundle_.maps) {
            if (candidate.id == map) {
                known_map = true;
                break;
            }
        }
        if (!known_map) {
            return false;
        }

        state_.active_map_id = map;
        state_.auto_follow = false;

        const auto requested_tab = ParseCommandString(line, "tab").value_or(ParseCommandString(line, "tab_id").value_or(""));
        if (!requested_tab.empty()) {
            if (const auto* tab = FindTab(bundle_, requested_tab); tab != nullptr && tab->map_id == map) {
                state_.active_tab_id = tab->id;
            }
        }
        const auto* active_tab = state_.active_tab_id.empty() ? nullptr : FindTab(bundle_, state_.active_tab_id);
        if (active_tab == nullptr || active_tab->map_id != map) {
            for (const auto& tab : bundle_.tabs) {
                if (tab.map_id == map) {
                    state_.active_tab_id = tab.id;
                    break;
                }
            }
        }
        autosave_dirty_ = true;
        return true;
    }
    if (command == "tracker.set_tab") {
        const auto tab = ParseCommandString(line, "tab").value_or("");
        if (!tab.empty()) {
            state_.active_tab_id = tab;
            if (state_.auto_follow) {
                state_.active_map_id = ResolveActiveMapId();
            }
            autosave_dirty_ = true;
            return true;
        }
        return false;
    }
    if (command == "tracker.set_auto_follow") {
        SetAutoFollowMap(ParseCommandBool(line, "enabled").value_or(true));
        autosave_dirty_ = true;
        return true;
    }
    if (command == "chat.say") {
        const auto text = ParseCommandString(line, "text").value_or("");
        if (!text.empty() && config_.send_chat_message) {
            std::string error;
            config_.send_chat_message(text, &error);
        }
        return false;
    }
    if (command == "tracker.click_item") {
        const auto code = ParseCommandString(line, "code").value_or(ParseCommandString(line, "slot_id").value_or(""));
        const auto button = ParseCommandString(line, "button").value_or("left");
        return !code.empty() && ApplyClickItemCommand(code, button);
    }
    if (command == "tracker.click_pin") {
        const auto location_id = ParseCommandUint(line, "location_id");
        const auto location = ParseCommandString(line, "location");
        const auto group_id = ParseCommandString(line, "group_id");
        const auto button = ParseCommandString(line, "button").value_or("left");
        if (location_id.has_value()) {
            return ApplyClickPinCommand(std::to_string(*location_id), button);
        } else if (location.has_value()) {
            return ApplyClickPinCommand(*location, button);
        } else if (group_id.has_value()) {
            return ApplyClickPinCommand(*group_id, button);
        }
    }
    return false;
}

bool TrackerHeadlessRuntime::PollCommands() {
    if (config_.command_log_path.empty() || !std::filesystem::exists(config_.command_log_path)) {
        return false;
    }
    std::ifstream input(config_.command_log_path, std::ios::binary);
    if (!input) return false;
    input.seekg(static_cast<std::streamoff>(command_log_offset_), std::ios::beg);
    bool changed = false;
    std::string line;
    while (std::getline(input, line)) {
        changed = ApplyCommandLine(line) || changed;
    }
    const auto end = input.tellg();
    if (end >= 0) {
        command_log_offset_ = static_cast<std::uintmax_t>(end);
    } else {
        command_log_offset_ = std::filesystem::file_size(config_.command_log_path);
    }
    return changed;
}
