namespace {

bool HasItemStateValue(const TrackerItemSlotState& state) {
    return state.owned || state.stage > 0 || state.count > 0;
}

bool HasVisualItemStateValue(const TrackerItemSlotState& state,
                             const TrackerPackItemVisualDefinition* visual) {
    if (state.owned || state.stage > 0) {
        return true;
    }
    if (visual != nullptr && visual->min_quantity > 0) {
        return state.count > visual->min_quantity;
    }
    return state.count > 0;
}

const TrackerItemSlotState& EffectiveItemSlotState(const TrackerHeadlessRuntimeState& state,
                                                   const std::string& slot_id,
                                                   const TrackerItemSlotState& empty_state) {
    if (const auto logic_it = state.logic_item_slots.find(slot_id); logic_it != state.logic_item_slots.end()) {
        return logic_it->second;
    }
    if (const auto manual_it = state.item_slots.find(slot_id); manual_it != state.item_slots.end()) {
        return manual_it->second;
    }
    return empty_state;
}

const TrackerItemSlotState& EffectivePackCodeState(const TrackerHeadlessRuntimeState& state,
                                                   const std::string& code,
                                                   const TrackerItemSlotState& empty_state) {
    if (const auto logic_it = state.logic_pack_code_states.find(code); logic_it != state.logic_pack_code_states.end()) {
        return logic_it->second;
    }
    if (const auto manual_it = state.pack_code_states.find(code); manual_it != state.pack_code_states.end()) {
        return manual_it->second;
    }
    return empty_state;
}

bool HasPackCodeStateValue(const TrackerBundleModel& bundle,
                           const TrackerHeadlessRuntimeState& state,
                           std::string_view code,
                           const TrackerItemSlotState& empty_state) {
    return HasVisualItemStateValue(EffectivePackCodeState(state, std::string(code), empty_state),
                                   FindPackItemVisual(bundle, code));
}

bool IsMappedPackItemCode(const TrackerBundleModel& bundle, std::string_view code) {
    for (const auto& mapping : bundle.pack_item_mappings) {
        for (const auto& mapped_code : mapping.codes) {
            if (mapped_code == code) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> CollectActivePackCodes(const TrackerBundleModel& bundle,
                                                const TrackerHeadlessRuntimeState& state) {
    TrackerItemSlotState empty_state;
    std::unordered_set<std::string> seen;
    std::vector<std::string> codes;
    const auto consider = [&](const std::unordered_map<std::string, TrackerItemSlotState>& source) {
        for (const auto& [code, _] : source) {
            if (!seen.insert(code).second) continue;
            if (HasPackCodeStateValue(bundle, state, code, empty_state)) {
                codes.push_back(code);
            }
        }
    };
    consider(state.logic_pack_code_states);
    consider(state.pack_code_states);
    std::sort(codes.begin(), codes.end());
    return codes;
}

std::vector<std::string> CollectVisibleActivePackCodes(const TrackerBundleModel& bundle,
                                                       const TrackerHeadlessRuntimeState& state) {
    const auto active_codes = CollectActivePackCodes(bundle, state);
    std::vector<std::string> visible_codes;
    visible_codes.reserve(active_codes.size());
    for (const auto& code : active_codes) {
        if (!IsNormalizedItemSlotCode(bundle, code) && IsMappedPackItemCode(bundle, code)) {
            visible_codes.push_back(code);
        }
    }
    return visible_codes;
}

}  // namespace

std::string TrackerHeadlessRuntime::BuildSnapshotJson() const {
    std::ostringstream out;
    const auto active_map_id = ResolveActiveMapId();
    const bool room_connected = RoomMetadataConnected(state_.room_metadata);
    const bool include_static_payload = !static_snapshot_payload_published_;
    std::size_t total_locations = 0;
    std::size_t checked_locations = 0;
    std::size_t received_count = 0;
    const auto empty_state = TrackerItemSlotState{};
    for (const auto& slot : bundle_.item_slots) {
        const auto* icon_binding = FindItemIconBinding(bundle_, slot.slot_id);
        const auto* pack_visual = FindPackItemVisualForSlot(bundle_, slot, icon_binding);
        if (HasVisualItemStateValue(EffectiveItemSlotState(state_, slot.slot_id, empty_state), pack_visual)) {
            ++received_count;
        }
    }
    for (const auto& group : bundle_.location_groups) {
        total_locations += group.locations.size();
        for (const auto& location : group.locations) {
            if (state_.checked_locations.contains(location.location_id)) {
                ++checked_locations;
            }
        }
    }

    out << "{";
    out << "\"schema\":\"sekailink.tracker.snapshot.v1\",";
    out << "\"revision\":" << state_.revision << ",";
    out << "\"game\":" << JsonString(bundle_.display_name) << ",";
    out << "\"connected\":" << (room_connected ? "true" : "false") << ",";
    out << "\"ap_connected\":" << (room_connected ? "true" : "false") << ",";
    out << "\"assets_root\":"
        << JsonString(!config_.assets_root.empty() ? config_.assets_root.string() : resolved_bundle_path_.string())
        << ",";
    out << "\"slot\":" << JsonString(state_.slot_id) << ",";
    out << "\"slot_id\":" << JsonString(state_.slot_id) << ",";
    out << "\"seed\":" << JsonString(state_.seed_id) << ",";
    out << "\"seed_id\":" << JsonString(state_.seed_id) << ",";
    out << "\"slot_name\":" << JsonString(state_.slot_name) << ",";
    out << "\"player_alias\":" << JsonString(state_.player_alias) << ",";
    out << "\"room_id\":" << JsonString(state_.room_id) << ",";
    out << "\"active_map\":" << JsonString(active_map_id) << ",";
    out << "\"active_tab\":" << JsonString(state_.active_tab_id) << ",";
    out << "\"auto_follow_map\":" << (state_.auto_follow ? "true" : "false") << ",";

    out << "\"room_metadata\":{";
    bool first_meta = true;
    for (const auto& [key, value] : state_.room_metadata) {
        if (key == "slot_data") continue;
        if (key == "chat_messages") continue;
        if (!first_meta) out << ",";
        first_meta = false;
        out << JsonString(key) << ":" << JsonString(value);
    }
    out << "},";

    out << "\"slot_data\":";
    if (!state_.slot_data_json.empty() &&
        (state_.slot_data_json.front() == '{' || state_.slot_data_json.front() == '[')) {
        out << state_.slot_data_json;
    } else {
        out << "{}";
    }
    out << ",";

    out << "\"chat_messages\":";
    if (const auto chat = state_.room_metadata.find("chat_messages");
        chat != state_.room_metadata.end() && !chat->second.empty() && chat->second.front() == '[') {
        out << chat->second;
    } else {
        out << "[]";
    }
    out << ",";

    out << "\"chat_commands\":[\"/hint <itemname>\",\"/collect\",\"/remaining\"],";
    out << "\"hint_item_names\":";
    if (const auto items = state_.room_metadata.find("hint_item_names");
        items != state_.room_metadata.end() && !items->second.empty() && items->second.front() == '[') {
        out << items->second;
    } else {
        out << "[]";
    }
    out << ",";

    out << "\"checked_locations\":[";
    bool first = true;
    for (const auto& group : bundle_.location_groups) {
        for (const auto& location : group.locations) {
            if (!state_.checked_locations.contains(location.location_id)) continue;
            if (!first) out << ",";
            first = false;
            out << "{"
                << "\"location_id\":" << location.location_id << ","
                << "\"location_name\":" << JsonString(location.location_name) << ","
                << "\"event_key\":" << JsonString(location.event_key) << ","
                << "\"group_id\":" << JsonString(group.group_id) << ","
                << "\"group_label\":" << JsonString(group.label)
                << "}";
        }
    }
    out << "],";

    out << "\"received_items\":[";
    first = true;
    for (const auto& slot : bundle_.item_slots) {
        const auto& item_state = EffectiveItemSlotState(state_, slot.slot_id, empty_state);
        const auto* icon_binding = FindItemIconBinding(bundle_, slot.slot_id);
        const auto* pack_visual = FindPackItemVisualForSlot(bundle_, slot, icon_binding);
        if (!HasVisualItemStateValue(item_state, pack_visual)) continue;
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(slot.slot_id) << ","
            << "\"slot_id\":" << JsonString(slot.slot_id) << ","
            << "\"item_name\":" << JsonString(slot.label) << ","
            << "\"group_id\":" << JsonString(slot.group_id) << ","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count
            << "}";
    }
    const auto received_pack_codes = std::vector<std::string>{};
    for (const auto& code : received_pack_codes) {
        const auto& item_state = EffectivePackCodeState(state_, code, empty_state);
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(code) << ","
            << "\"slot_id\":" << JsonString(code) << ","
            << "\"code\":" << JsonString(code) << ","
            << "\"item_name\":" << JsonString(PackCodeLabel(bundle_, code)) << ","
            << "\"group_id\":\"pack_items\","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count
            << "}";
    }
    out << "],";

    out << "\"tabs\":[";
    first = true;
    for (const auto& tab_id : bundle_.tab_order) {
        const auto* tab = FindTab(bundle_, tab_id);
        if (tab == nullptr) continue;
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(tab->id) << ","
            << "\"label\":" << JsonString(tab->label) << ","
            << "\"map_id\":" << JsonString(tab->map_id) << ","
            << "\"active\":" << (tab->id == state_.active_tab_id ? "true" : "false")
            << "}";
    }
    out << "],";

    out << "\"maps\":[";
    first = true;
    for (const auto& map : bundle_.maps) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(map.id) << ","
            << "\"label\":" << JsonString(map.label) << ","
            << "\"image\":" << JsonString(map.image) << ","
            << "\"art_origin\":" << JsonString(map.art_origin)
            << "}";
    }
    out << "],";

    out << "\"pack_maps\":[";
    first = true;
    for (const auto& map : bundle_.pack_maps) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"name\":" << JsonString(map.name) << ","
            << "\"image\":" << JsonString(map.image)
            << "}";
    }
    out << "],";

    if (include_static_payload) {
        out << "\"pack_layouts\":[";
        first = true;
        for (const auto& layout : bundle_.pack_layouts) {
            if (!first) out << ",";
            first = false;
            out << "{"
                << "\"file\":" << JsonString(layout.file_name) << ","
                << "\"layout_ids\":";
            WriteJsonStringArray(out, layout.layout_ids);
            out << ",\"json\":";
            if (!layout.json.empty() && layout.json.front() == '{' && layout.json.back() == '}') {
                out << layout.json;
            } else {
                out << "{}";
            }
            out << "}";
        }
        out << "],";
    }

    out << "\"item_icon_groups\":[";
    first = true;
    for (const auto& group : bundle_.item_icon_groups) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"group_id\":" << JsonString(group.group_id) << ","
            << "\"default_palette\":" << JsonString(group.default_palette)
            << "}";
    }
    out << "],";

    if (include_static_payload) {
        out << "\"pack_item_visuals\":[";
        first = true;
        for (const auto& visual : bundle_.pack_item_visuals) {
            if (!first) out << ",";
            first = false;
            out << "{"
                << "\"primary_code\":" << JsonString(visual.primary_code) << ","
                << "\"name\":" << JsonString(visual.name) << ","
                << "\"type\":" << JsonString(visual.type) << ","
                << "\"image\":" << JsonString(visual.image) << ","
                << "\"disabled_image\":" << JsonString(visual.disabled_image) << ","
                << "\"image_mods\":" << JsonString(visual.image_mods) << ","
                << "\"disabled_image_mods\":" << JsonString(visual.disabled_image_mods) << ","
                << "\"base_item\":" << JsonString(visual.base_item) << ","
                << "\"item_left\":" << JsonString(visual.item_left) << ","
                << "\"item_right\":" << JsonString(visual.item_right) << ","
                << "\"initial_quantity\":" << visual.initial_quantity << ","
                << "\"min_quantity\":" << visual.min_quantity << ","
                << "\"max_quantity\":" << visual.max_quantity << ","
                << "\"increment\":" << visual.increment << ","
                << "\"initial_stage\":" << visual.initial_stage << ","
                << "\"loop_stages\":" << (visual.loop_stages ? "true" : "false") << ","
                << "\"allow_disabled\":" << (visual.allow_disabled ? "true" : "false") << ","
                << "\"initial_active_state\":" << (visual.initial_active_state ? "true" : "false") << ","
                << "\"codes\":";
            WriteJsonStringArray(out, visual.codes);
            out << ",\"secondary_codes\":";
            WriteJsonStringArray(out, visual.secondary_codes);
            out << ",\"aliases\":";
            WriteJsonStringArray(out, visual.aliases);
            out << ",\"stages\":[";
            bool first_stage = true;
            for (const auto& stage : visual.stages) {
                if (!first_stage) out << ",";
                first_stage = false;
                out << "{"
                    << "\"image\":" << JsonString(stage.image) << ","
                    << "\"disabled_image\":" << JsonString(stage.disabled_image) << ","
                    << "\"image_mods\":" << JsonString(stage.image_mods) << ","
                    << "\"disabled_image_mods\":" << JsonString(stage.disabled_image_mods) << ","
                    << "\"codes\":";
                WriteJsonStringArray(out, stage.codes);
                out << ",\"secondary_codes\":";
                WriteJsonStringArray(out, stage.secondary_codes);
                out << "}";
            }
            out << "],\"images\":[";
            bool first_image = true;
            for (const auto& image : visual.images) {
                if (!first_image) out << ",";
                first_image = false;
                out << "{"
                    << "\"image\":" << JsonString(image.image) << ","
                    << "\"image_mods\":" << JsonString(image.image_mods) << ","
                    << "\"left\":" << (image.left ? "true" : "false") << ","
                    << "\"right\":" << (image.right ? "true" : "false") << ","
                    << "\"codes\":";
                WriteJsonStringArray(out, image.codes);
                out << ",\"secondary_codes\":";
                WriteJsonStringArray(out, image.secondary_codes);
                out << "}";
            }
            out << "]"
                << "}";
        }
        out << "],";
    }

    out << "\"items\":[";
    first = true;
    for (const auto& slot : bundle_.item_slots) {
        const auto& item_state = EffectiveItemSlotState(state_, slot.slot_id, empty_state);
        const auto* icon_binding = FindItemIconBinding(bundle_, slot.slot_id);
        const auto* pack_visual = FindPackItemVisualForSlot(bundle_, slot, icon_binding);
        const bool acquired = HasVisualItemStateValue(item_state, pack_visual);
        const bool item_disabled = !acquired;
        const auto visual_stage_index = VisualStageIndex(slot, pack_visual, item_state);
        const auto visual_image = SelectPackVisualImage(slot, pack_visual, item_state, item_disabled);
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(slot.slot_id) << ","
            << "\"code\":" << JsonString(slot.slot_id) << ","
            << "\"label\":" << JsonString(slot.label) << ","
            << "\"group_id\":" << JsonString(slot.group_id) << ","
            << "\"behavior\":" << JsonString(slot.behavior) << ","
            << "\"icon_key\":" << JsonString(icon_binding != nullptr ? icon_binding->icon_key : std::string{}) << ","
            << "\"render_hint\":" << JsonString(icon_binding != nullptr ? icon_binding->render_hint : std::string{}) << ","
            << "\"pack_visual_code\":" << JsonString(pack_visual != nullptr ? pack_visual->primary_code : std::string{}) << ","
            << "\"image\":" << JsonString(visual_image) << ","
            << "\"stage_index\":" << visual_stage_index << ","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count << ","
            << "\"acquired\":"
            << (acquired ? "true" : "false") << ","
            << "\"asset_candidates\":[";
        bool first_asset = true;
        if (icon_binding != nullptr) {
            for (const auto& asset : icon_binding->asset_candidates) {
                if (!first_asset) out << ",";
                first_asset = false;
                out << JsonString(asset);
            }
        }
        out << "]"
            << "}";
    }
    const auto active_pack_codes = CollectVisibleActivePackCodes(bundle_, state_);
    for (const auto& code : active_pack_codes) {
        const auto& item_state = EffectivePackCodeState(state_, code, empty_state);
        const auto* pack_visual = FindPackItemVisual(bundle_, code);
        const auto label = PackCodeLabel(bundle_, code);
        std::string visual_image;
        std::uint64_t visual_stage_index = item_state.stage;
        if (pack_visual != nullptr) {
            TrackerItemSlotDefinition synthetic_slot;
            synthetic_slot.slot_id = code;
            synthetic_slot.label = label;
            synthetic_slot.behavior = pack_visual->type;
            visual_stage_index = VisualStageIndex(synthetic_slot, pack_visual, item_state);
            visual_image = SelectPackVisualImage(
                synthetic_slot,
                pack_visual,
                item_state,
                !HasVisualItemStateValue(item_state, pack_visual));
        }
        const bool acquired = HasVisualItemStateValue(item_state, pack_visual);
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(code) << ","
            << "\"code\":" << JsonString(code) << ","
            << "\"label\":" << JsonString(label) << ","
            << "\"group_id\":\"pack_items\","
            << "\"behavior\":\"pack-code\","
            << "\"icon_key\":\"\","
            << "\"render_hint\":\"pack-code\","
            << "\"pack_visual_code\":" << JsonString(code) << ","
            << "\"image\":" << JsonString(visual_image) << ","
            << "\"stage_index\":" << visual_stage_index << ","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count << ","
            << "\"acquired\":"
            << (acquired ? "true" : "false") << ","
            << "\"asset_candidates\":[]"
            << "}";
    }
    out << "],";

    out << "\"pins\":[";
    first = true;
    for (const auto& group : bundle_.location_groups) {
        const auto* layer = FindPinLayer(bundle_, group.group_id);
        const auto pin_map_id = layer != nullptr && !layer->map_id.empty() ? layer->map_id : bundle_.default_map_id;
        std::size_t group_checked = 0;
        for (const auto& location : group.locations) {
            if (state_.checked_locations.contains(location.location_id)) {
                ++group_checked;
            }
        }
        const auto remaining = group.locations.size() - group_checked;
        std::string color = "red";
        if (remaining == 0) {
            color = "black";
        } else if (const auto logic_it = state_.logic_groups.find(group.group_id); logic_it != state_.logic_groups.end()) {
            color = logic_it->second.color;
        }
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(group.group_id) << ","
            << "\"location_id\":" << JsonString(group.group_id) << ","
            << "\"label\":" << JsonString(group.label) << ","
            << "\"map_id\":" << JsonString(pin_map_id) << ","
            << "\"tab_id\":" << JsonString(group.preferred_tab) << ","
            << "\"kind\":" << JsonString(layer != nullptr ? layer->pin_kind : std::string("location-group")) << ","
            << "\"checked_count\":" << group_checked << ","
            << "\"total_count\":" << group.locations.size() << ","
            << "\"remaining_count\":" << remaining << ","
            << "\"mixed\":" << (group_checked > 0 && remaining > 0 ? "true" : "false") << ","
            << "\"color\":" << JsonString(color)
            << "}";
    }
    out << "],";

    out << "\"pins_detailed\":[";
    first = true;
    const auto grouped_pins = BuildGroupedSnapshotPins(bundle_, state_);
    for (const auto& grouped_pin : grouped_pins) {
        const auto& pin = grouped_pin.base;
        const auto color = AggregatePinColor(grouped_pin, state_);
        const auto pack_it = state_.pack_location_states.find(pin.pack_location_id);
        const int state_bits = pack_it != state_.pack_location_states.end() ? LogicStateBits(pack_it->second)
                                                                            : (color == "black" ? 0 : 2);
        const bool checked = grouped_pin.total_count > 0 && grouped_pin.checked_count >= grouped_pin.total_count;
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(pin.pin_id) << ","
            << "\"group_id\":" << JsonString(pin.group_id) << ","
            << "\"pack_location_id\":" << JsonString(pin.pack_location_id) << ","
            << "\"section_id\":\"\","
            << "\"location_id\":" << pin.location_id << ","
            << "\"label\":" << JsonString(pin.location_name) << ","
            << "\"map_id\":" << JsonString(pin.map_id) << ","
            << "\"pack_map\":" << JsonString(pin.pack_map) << ","
            << "\"map_asset\":" << JsonString(pin.map_asset) << ","
            << "\"kind\":" << JsonString(pin.pin_kind) << ","
            << "\"x\":" << pin.x << ","
            << "\"y\":" << pin.y << ","
            << "\"size\":" << pin.size << ","
            << "\"state\":" << state_bits << ","
            << "\"checked\":" << (checked ? "true" : "false") << ","
            << "\"checked_count\":" << grouped_pin.checked_count << ","
            << "\"total_count\":" << grouped_pin.total_count << ","
            << "\"remaining_count\":" << (grouped_pin.total_count - grouped_pin.checked_count) << ","
            << "\"mixed\":" << (!checked && (grouped_pin.segments.size() > 1 || grouped_pin.checked_count > 0) ? "true" : "false") << ","
            << "\"color\":" << JsonString(color) << ",";

        out << "\"location_ids\":[";
        bool first_location = true;
        for (const auto location_id : grouped_pin.location_ids) {
            if (!first_location) out << ",";
            first_location = false;
            out << location_id;
        }
        out << "],";

        out << "\"segments\":[";
        bool first_segment = true;
        for (const auto& segment : grouped_pin.segments) {
            const auto segment_checked = static_cast<std::uint64_t>(
                std::count_if(segment.checks.begin(), segment.checks.end(), [](const SnapshotPinCheck& check) {
                    return check.checked;
                }));
            const auto segment_color = SegmentColorForPin(segment);
            TrackerLogicGroupState segment_state;
            segment_state.color = segment_color;
            segment_state.normal_count = segment.normal_count;
            segment_state.sequence_break_count = segment.sequence_break_count;
            segment_state.inspect_count = segment.inspect_count;
            segment_state.none_count = segment.none_count;
            if (!first_segment) out << ",";
            first_segment = false;
            out << "{"
                << "\"section_id\":" << JsonString(segment.section_id) << ","
                << "\"label\":" << JsonString(segment.label) << ","
                << "\"state\":" << LogicStateBits(segment_state) << ","
                << "\"color\":" << JsonString(segment_color) << ","
                << "\"checked_count\":" << segment_checked << ","
                << "\"total_count\":" << segment.checks.size() << ","
                << "\"remaining_count\":" << (segment.checks.size() - segment_checked) << ","
                << "\"mixed\":" << (segment_checked > 0 && segment_checked < segment.checks.size() ? "true" : "false")
                << ",\"checks\":[";
            bool first_check = true;
            for (const auto& check : segment.checks) {
                if (!first_check) out << ",";
                first_check = false;
                out << "{"
                    << "\"location_id\":" << check.location_id << ","
                    << "\"label\":" << JsonString(check.label) << ","
                    << "\"checked\":" << (check.checked ? "true" : "false")
                    << "}";
            }
            out << "]}";
        }
        out << "]}";
    }
    out << "],";

    out << "\"summary\":{"
        << "\"checked\":" << checked_locations << ","
        << "\"total\":" << total_locations << ","
        << "\"received\":" << received_count
        << "},";

    out << "\"status\":{"
        << "\"ap_connected\":" << (room_connected ? "true" : "false") << ","
        << "\"logic_ready\":" << (state_.logic_ready ? "true" : "false") << ","
        << "\"pack\":" << JsonString(state_.tracker_pack) << ","
        << "\"variant\":" << JsonString(state_.tracker_variant) << ","
        << "\"assets_root\":"
        << JsonString(!config_.assets_root.empty() ? config_.assets_root.string() : resolved_bundle_path_.string())
        << "}";
    out << "}";
    return out.str();
}
