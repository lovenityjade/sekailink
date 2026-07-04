struct SnapshotPinCheck {
    std::uint64_t location_id = 0;
    std::string label;
    bool checked = false;
};

struct SnapshotPinSegment {
    std::string section_id;
    std::string label;
    std::string color = "red";
    std::uint64_t normal_count = 0;
    std::uint64_t sequence_break_count = 0;
    std::uint64_t inspect_count = 0;
    std::uint64_t none_count = 0;
    std::vector<SnapshotPinCheck> checks;
};

struct SnapshotGroupedPin {
    TrackerDetailedPinDefinition base;
    std::vector<SnapshotPinSegment> segments;
    std::vector<std::uint64_t> location_ids;
    std::uint64_t checked_count = 0;
    std::uint64_t total_count = 0;
};

std::string PinGroupingKey(const TrackerDetailedPinDefinition& pin) {
    std::ostringstream key;
    key << pin.pack_location_id << '\x1f'
        << pin.map_id << '\x1f'
        << pin.pack_map << '\x1f'
        << pin.map_asset << '\x1f'
        << pin.x << '\x1f'
        << pin.y << '\x1f'
        << pin.size;
    return key.str();
}

std::string LastPathComponent(std::string_view value) {
    const auto slash = value.find_last_of('/');
    if (slash == std::string_view::npos || slash + 1 >= value.size()) {
        return std::string(value);
    }
    return std::string(value.substr(slash + 1));
}

std::string SectionDisplayName(std::string_view section_id, std::string_view pack_location_id) {
    if (!pack_location_id.empty() && section_id.size() > pack_location_id.size() + 1 &&
        section_id.substr(0, pack_location_id.size()) == pack_location_id &&
        section_id[pack_location_id.size()] == '/') {
        return std::string(section_id.substr(pack_location_id.size() + 1));
    }
    return LastPathComponent(section_id);
}

int LogicStateBits(const TrackerLogicGroupState& state) {
    if (state.color == "hidden") return -1;
    if (state.color == "black") return 0;
    int bits = 0;
    if (state.normal_count > 0) bits |= 1;
    if (state.none_count > 0) bits |= 2;
    if (state.sequence_break_count > 0) bits |= 4;
    if (state.inspect_count > 0) bits |= 8;
    if (bits != 0) return bits;
    if (state.color == "green") return 1;
    if (state.color == "red") return 2;
    if (state.color == "yellow") return 4;
    if (state.color == "blue") return 8;
    return 2;
}

void CopyLogicStateToSegment(const TrackerLogicGroupState& state, SnapshotPinSegment* segment) {
    segment->color = state.color;
    segment->normal_count = state.normal_count;
    segment->sequence_break_count = state.sequence_break_count;
    segment->inspect_count = state.inspect_count;
    segment->none_count = state.none_count;
}

std::string SegmentColorForPin(const SnapshotPinSegment& segment) {
    if (!segment.checks.empty()) {
        const auto checked = static_cast<std::uint64_t>(
            std::count_if(segment.checks.begin(), segment.checks.end(), [](const SnapshotPinCheck& check) {
                return check.checked;
            }));
        if (checked >= segment.checks.size()) {
            return "black";
        }
    }
    return segment.color.empty() ? std::string("red") : segment.color;
}

std::string AggregatePinColor(const SnapshotGroupedPin& pin,
                              const TrackerHeadlessRuntimeState& state) {
    if (pin.total_count > 0 && pin.checked_count >= pin.total_count) {
        return "black";
    }
    if (const auto pack_it = state.pack_location_states.find(pin.base.pack_location_id);
        pack_it != state.pack_location_states.end() && pack_it->second.color != "hidden") {
        return pack_it->second.color;
    }
    std::string first_color;
    for (const auto& segment : pin.segments) {
        const auto color = SegmentColorForPin(segment);
        if (color == "hidden") continue;
        if (first_color.empty()) {
            first_color = color;
        } else if (first_color != color) {
            return "mixed";
        }
    }
    return first_color.empty() ? std::string("red") : first_color;
}

std::vector<SnapshotGroupedPin> BuildGroupedSnapshotPins(const TrackerBundleModel& bundle,
                                                        const TrackerHeadlessRuntimeState& state) {
    const bool runtime_pack = !bundle.pack_maps.empty() || !bundle.pack_layouts.empty();
    if (runtime_pack && bundle.has_pack_location_mapping && !state.logic_ready) {
        return {};
    }
    const auto& detailed_pins =
        (state.runtime_detailed_pins.empty() && (!runtime_pack || !bundle.has_pack_location_mapping))
            ? bundle.detailed_pins
            : state.runtime_detailed_pins;
    std::vector<SnapshotGroupedPin> grouped;
    std::unordered_map<std::string, std::size_t> group_index_by_key;
    for (const auto& pin : detailed_pins) {
        if (!IsSupportedPackMap(bundle, pin.pack_map)) continue;
        const auto section_it = state.pack_section_states.find(pin.section_id);
        if (section_it != state.pack_section_states.end() && section_it->second.color == "hidden") {
            continue;
        }
        const auto pack_it = state.pack_location_states.find(pin.pack_location_id);
        if (pack_it != state.pack_location_states.end() && pack_it->second.color == "hidden") {
            continue;
        }

        const auto group_key = PinGroupingKey(pin);
        auto grouped_it = group_index_by_key.find(group_key);
        if (grouped_it == group_index_by_key.end()) {
            grouped_it = group_index_by_key.emplace(group_key, grouped.size()).first;
            SnapshotGroupedPin group;
            group.base = pin;
            std::ostringstream pin_id;
            pin_id << pin.pack_location_id << ":" << pin.pack_map << ":" << pin.x << ":" << pin.y;
            group.base.pin_id = pin_id.str();
            group.base.location_name = LastPathComponent(pin.pack_location_id);
            grouped.push_back(std::move(group));
        }
        auto& group = grouped[grouped_it->second];
        if (std::find(group.location_ids.begin(), group.location_ids.end(), pin.location_id) == group.location_ids.end()) {
            group.location_ids.push_back(pin.location_id);
        }

        auto segment_it = std::find_if(group.segments.begin(), group.segments.end(),
                                       [&](const SnapshotPinSegment& segment) {
                                           return segment.section_id == pin.section_id;
                                       });
        if (segment_it == group.segments.end()) {
            SnapshotPinSegment segment;
            segment.section_id = pin.section_id;
            segment.label = SectionDisplayName(pin.section_id, pin.pack_location_id);
            if (section_it != state.pack_section_states.end()) {
                CopyLogicStateToSegment(section_it->second, &segment);
            } else if (pack_it != state.pack_location_states.end()) {
                CopyLogicStateToSegment(pack_it->second, &segment);
            }
            group.segments.push_back(std::move(segment));
            segment_it = std::prev(group.segments.end());
        }
        if (std::none_of(segment_it->checks.begin(), segment_it->checks.end(), [&](const SnapshotPinCheck& check) {
                return check.location_id == pin.location_id;
            })) {
            const bool checked = state.checked_locations.contains(pin.location_id);
            segment_it->checks.push_back(SnapshotPinCheck{pin.location_id, pin.location_name, checked});
            group.total_count += 1;
            if (checked) {
                group.checked_count += 1;
            }
        }
    }
    for (auto& group : grouped) {
        if (!group.location_ids.empty()) {
            const auto first_unchecked = std::find_if(group.location_ids.begin(), group.location_ids.end(),
                                                      [&](std::uint64_t location_id) {
                                                          return !state.checked_locations.contains(location_id);
                                                      });
            group.base.location_id =
                first_unchecked != group.location_ids.end() ? *first_unchecked : group.location_ids.front();
        }
    }
    return grouped;
}
