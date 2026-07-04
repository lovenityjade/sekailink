std::string CanonicalIdentityForItem(const TrackerItemIdentity& item) {
    if (item.item_id != 0) return std::to_string(item.item_id);
    if (!item.event_key.empty()) return item.event_key;
    if (!item.item_name.empty()) return item.item_name;
    return item.mapped_value;
}

std::string DisplayIdentityForItem(const TrackerItemIdentity& item) {
    if (!item.item_name.empty()) return item.item_name;
    if (!item.mapped_value.empty()) return item.mapped_value;
    if (!item.event_key.empty()) return item.event_key;
    if (item.item_id != 0) return std::to_string(item.item_id);
    return {};
}

bool MatchesIdentity(const TrackerItemIdentity& slot_item,
                     std::uint64_t canonical_id,
                     std::string_view event_key,
                     std::string_view value) {
    if (canonical_id != 0 && slot_item.item_id == canonical_id) return true;
    if (!event_key.empty() && slot_item.event_key == event_key) return true;
    if (!value.empty() && (slot_item.item_name == value || slot_item.mapped_value == value)) return true;
    return false;
}

const TrackerTabDefinition* FindTab(const TrackerBundleModel& bundle, std::string_view id) {
    for (const auto& tab : bundle.tabs) {
        if (tab.id == id) return &tab;
    }
    return nullptr;
}

const TrackerPinLayerDefinition* FindPinLayer(const TrackerBundleModel& bundle, std::string_view group_id) {
    for (const auto& layer : bundle.pin_layers) {
        if (layer.group_id == group_id) return &layer;
    }
    return nullptr;
}

std::string CanonicalPathStem(std::string_view path) {
    if (path.empty()) return {};
    return CanonicalToken(std::filesystem::path(std::string(path)).stem().string());
}

std::string ResolvePackMapId(const TrackerBundleModel& bundle, std::string_view pack_map) {
    const auto wanted = CanonicalToken(pack_map);
    if (wanted.empty()) return {};

    for (const auto& map : bundle.maps) {
        if (!map.poptracker_map_name.empty() && CanonicalToken(map.poptracker_map_name) == wanted) {
            return map.id;
        }
    }
    for (const auto& map : bundle.maps) {
        if (CanonicalToken(map.id) == wanted || CanonicalToken(map.label) == wanted) {
            return map.id;
        }
    }

    std::string pack_image_stem;
    for (const auto& map : bundle.pack_maps) {
        if (CanonicalToken(map.name) == wanted) {
            pack_image_stem = CanonicalPathStem(map.image);
            break;
        }
    }
    if (!pack_image_stem.empty()) {
        for (const auto& map : bundle.maps) {
            if (CanonicalPathStem(map.image) == pack_image_stem) {
                return map.id;
            }
        }
    }
    return {};
}

std::string ResolveDetailedPinMapId(const TrackerBundleModel& bundle,
                                    std::string_view pack_map,
                                    std::string_view group_id) {
    if (const auto map_id = ResolvePackMapId(bundle, pack_map); !map_id.empty()) {
        return map_id;
    }
    if (const auto* layer = FindPinLayer(bundle, group_id); layer != nullptr) {
        return layer->map_id;
    }
    return {};
}

const TrackerLocationGroupDefinition* FindLocationGroupByLocation(const TrackerBundleModel& bundle,
                                                                  std::uint64_t canonical_id,
                                                                  std::string_view event_key) {
    for (const auto& group : bundle.location_groups) {
        for (const auto& location : group.locations) {
            if (canonical_id != 0 && location.location_id == canonical_id) return &group;
            if (!event_key.empty() && location.event_key == event_key) return &group;
        }
    }
    return nullptr;
}

const TrackerLocationDefinition* FindLocationByAnyId(const TrackerBundleModel& bundle, std::string_view location_or_group_id) {
    for (const auto& group : bundle.location_groups) {
        for (const auto& location : group.locations) {
            if (std::to_string(location.location_id) == location_or_group_id || location.event_key == location_or_group_id ||
                location.location_name == location_or_group_id) {
                return &location;
            }
        }
    }
    return nullptr;
}

std::optional<bool> ParseCommandBool(const std::string& line, const std::string& key) {
    return detail::extract_bool_field(line, key);
}

std::optional<std::string> ParseCommandString(const std::string& line, const std::string& key) {
    return detail::extract_string_field(line, key);
}

std::optional<std::uint64_t> ParseCommandUint(const std::string& line, const std::string& key) {
    return detail::extract_uint_field(line, key);
}

const TrackerItemIconBindingDefinition* FindItemIconBinding(const TrackerBundleModel& bundle, std::string_view slot_id) {
    for (const auto& binding : bundle.item_icon_bindings) {
        if (binding.slot_id == slot_id) return &binding;
    }
    return nullptr;
}

const TrackerPackItemVisualDefinition* FindPackItemVisual(const TrackerBundleModel& bundle,
                                                          std::string_view code_or_name) {
    const auto raw = std::string(code_or_name);
    const auto canonical = CanonicalToken(code_or_name);
    for (const auto& visual : bundle.pack_item_visuals) {
        if (visual.primary_code == raw || visual.name == raw || CanonicalToken(visual.name) == canonical) {
            return &visual;
        }
        for (const auto& alias : visual.aliases) {
            if (alias == raw || CanonicalToken(alias) == canonical) {
                return &visual;
            }
        }
    }
    return nullptr;
}

const TrackerPackItemVisualDefinition* FindPackItemVisualForSlot(
    const TrackerBundleModel& bundle,
    const TrackerItemSlotDefinition& slot,
    const TrackerItemIconBindingDefinition* icon_binding) {
    auto find_candidate = [&](std::string_view candidate) -> const TrackerPackItemVisualDefinition* {
        if (candidate.empty()) return nullptr;
        return FindPackItemVisual(bundle, candidate);
    };

    if (const auto* visual = find_candidate(slot.slot_id); visual != nullptr) return visual;
    if (const auto* visual = find_candidate(slot.label); visual != nullptr) return visual;
    if (icon_binding != nullptr) {
        if (const auto* visual = find_candidate(icon_binding->icon_key); visual != nullptr) return visual;
        const auto dot = icon_binding->icon_key.find_last_of('.');
        if (dot != std::string::npos) {
            if (const auto* visual = find_candidate(icon_binding->icon_key.substr(dot + 1)); visual != nullptr) {
                return visual;
            }
        }
    }
    for (const auto& item : slot.items) {
        if (const auto* visual = find_candidate(item.item_name); visual != nullptr) return visual;
        if (const auto* visual = find_candidate(item.mapped_value); visual != nullptr) return visual;
        if (const auto* visual = find_candidate(item.event_key); visual != nullptr) return visual;
    }
    return nullptr;
}

bool IsNormalizedItemSlotCode(const TrackerBundleModel& bundle, std::string_view code) {
    for (const auto& slot : bundle.item_slots) {
        if (slot.slot_id == code) {
            return true;
        }
    }
    return false;
}

bool IsKnownPackItemCode(const TrackerBundleModel& bundle, std::string_view code) {
    if (FindPackItemVisual(bundle, code) != nullptr) {
        return true;
    }
    for (const auto& mapping : bundle.pack_item_mappings) {
        if (std::find(mapping.codes.begin(), mapping.codes.end(), code) != mapping.codes.end()) {
            return true;
        }
    }
    return false;
}

std::string PackCodeLabel(const TrackerBundleModel& bundle, std::string_view code) {
    if (const auto* visual = FindPackItemVisual(bundle, code); visual != nullptr && !visual->name.empty()) {
        return visual->name;
    }
    return std::string(code);
}

void ApplyPackCodeStateForBehavior(TrackerItemSlotState& state, std::string_view behavior) {
    if (behavior == "consumable" || behavior == "combined_consumable") {
        state.count += 1;
        state.owned = state.count > 0;
        return;
    }
    if (behavior == "progressive" || behavior == "progressive_toggle" ||
        behavior == "progressive-toggle") {
        state.owned = true;
        state.stage = state.stage == 0 ? 1 : state.stage + 1;
        return;
    }
    state.owned = true;
    state.stage = state.stage == 0 ? 1 : state.stage;
}

const TrackerDetailedPinDefinition* FindDetailedPinByLocationId(const TrackerBundleModel& bundle, std::uint64_t location_id) {
    for (const auto& pin : bundle.detailed_pins) {
        if (pin.location_id == location_id) return &pin;
    }
    return nullptr;
}

std::string FindPackMapAsset(const TrackerBundleModel& bundle, std::string_view pack_map) {
    for (const auto& map : bundle.pack_maps) {
        if (map.name == pack_map) {
            return "poptracker-adapted/" + map.image;
        }
    }
    return {};
}

double ParseDoubleOrZero(const std::string& value) {
    try {
        std::size_t parsed = 0;
        const double result = std::stod(value, &parsed);
        return parsed == value.size() ? result : 0.0;
    } catch (const std::exception&) {
        return 0.0;
    }
}

std::vector<TrackerPackItemVisualDefinition> LoadPackItemVisualDefinitions(const std::filesystem::path& bundle_path) {
    std::vector<TrackerPackItemVisualDefinition> visuals;
    const auto items_root = bundle_path / "poptracker-adapted" / "items";
    if (!std::filesystem::exists(items_root)) {
        return visuals;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(items_root, ec)) {
        if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
        const auto file_text = ReadTextFile(entry.path());
        for (const auto& item_block : ExtractTopLevelObjectBlocks(file_text)) {
            TrackerPackItemVisualDefinition visual;
            visual.name = ExtractTopLevelStringField(item_block, "name").value_or("");
            visual.type = ExtractTopLevelStringField(item_block, "type").value_or("");
            visual.image = NormalizePackAssetPath(ExtractTopLevelStringField(item_block, "img").value_or(""));
            visual.disabled_image =
                NormalizePackAssetPath(ExtractTopLevelStringField(item_block, "disabled_img").value_or(""));
            visual.image_mods = ExtractTopLevelStringField(item_block, "img_mods").value_or("");
            visual.disabled_image_mods = ExtractTopLevelStringField(item_block, "disabled_img_mods").value_or("");
            visual.base_item = ExtractTopLevelStringField(item_block, "base_item").value_or("");
            visual.item_left = ExtractTopLevelStringField(item_block, "item_left").value_or("");
            visual.item_right = ExtractTopLevelStringField(item_block, "item_right").value_or("");
            visual.initial_quantity = ExtractTopLevelUintField(item_block, "initial_quantity", 0);
            visual.min_quantity = ExtractTopLevelUintField(item_block, "min_quantity", 0);
            visual.max_quantity = ExtractTopLevelUintField(item_block, "max_quantity", 0);
            visual.increment = std::max<std::uint64_t>(1, ExtractTopLevelUintField(item_block, "increment", 1));
            visual.initial_stage = ExtractTopLevelUintField(item_block, "initial_stage_idx", 0);
            visual.loop_stages = ExtractTopLevelBoolField(item_block, "loop", false).value_or(false);
            visual.allow_disabled = ExtractTopLevelBoolField(item_block, "allow_disabled", true).value_or(true);
            visual.initial_active_state =
                ExtractTopLevelBoolField(item_block, "initial_active_state", false).value_or(false);
            visual.codes = ParseTopLevelCodeList(item_block, "codes");
            visual.secondary_codes = ParseTopLevelCodeList(item_block, "secondary_codes");
            AppendUniqueList(visual.aliases, visual.codes);
            AppendUniqueList(visual.aliases, visual.secondary_codes);

            for (const auto& stage_block : detail::extract_object_blocks(item_block, "stages")) {
                TrackerPackItemStageDefinition stage;
                stage.image = NormalizePackAssetPath(ExtractTopLevelStringField(stage_block, "img").value_or(""));
                stage.disabled_image =
                    NormalizePackAssetPath(ExtractTopLevelStringField(stage_block, "disabled_img").value_or(""));
                stage.image_mods = ExtractTopLevelStringField(stage_block, "img_mods").value_or("");
                stage.disabled_image_mods = ExtractTopLevelStringField(stage_block, "disabled_img_mods").value_or("");
                stage.codes = ParseTopLevelCodeList(stage_block, "codes");
                stage.secondary_codes = ParseTopLevelCodeList(stage_block, "secondary_codes");
                AppendUniqueList(visual.aliases, stage.codes);
                AppendUniqueList(visual.aliases, stage.secondary_codes);
                visual.stages.push_back(std::move(stage));
            }

            for (const auto& image_block : detail::extract_object_blocks(item_block, "images")) {
                TrackerPackItemImageDefinition image;
                image.image = NormalizePackAssetPath(ExtractTopLevelStringField(image_block, "img").value_or(""));
                image.image_mods = ExtractTopLevelStringField(image_block, "img_mods").value_or("");
                image.left = ExtractTopLevelBoolField(image_block, "left", false).value_or(false);
                image.right = ExtractTopLevelBoolField(image_block, "right", false).value_or(false);
                image.codes = ParseTopLevelCodeList(image_block, "codes");
                image.secondary_codes = ParseTopLevelCodeList(image_block, "secondary_codes");
                AppendUniqueList(visual.aliases, image.codes);
                AppendUniqueList(visual.aliases, image.secondary_codes);
                visual.images.push_back(std::move(image));
            }

            if (visual.aliases.empty() && !visual.name.empty()) {
                AppendUnique(visual.aliases, CanonicalToken(visual.name));
            }
            if (!visual.aliases.empty()) {
                visual.primary_code = visual.aliases.front();
            }
            if (!visual.primary_code.empty() || !visual.name.empty()) {
                visuals.push_back(std::move(visual));
            }
        }
    }
    return visuals;
}

std::vector<TrackerPackItemMappingDefinition> LoadPackItemMappings(const std::filesystem::path& bundle_path) {
    std::vector<TrackerPackItemMappingDefinition> mappings;
    std::vector<std::filesystem::path> candidates;
    const auto preferred = bundle_path / "poptracker-adapted" / "scripts" / "autotracking" / "item_mapping.lua";
    if (std::filesystem::exists(preferred)) {
        candidates.push_back(preferred);
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(bundle_path, ec)) {
        if (ec || !entry.is_regular_file() || entry.path().filename() != "item_mapping.lua") {
            continue;
        }
        if (std::find(candidates.begin(), candidates.end(), entry.path()) == candidates.end()) {
            candidates.push_back(entry.path());
        }
    }

    std::unordered_set<std::uint64_t> seen_item_ids;
    for (const auto& path : candidates) {
        const auto text = StripLuaLineComments(ReadTextFile(path));
        std::size_t cursor = 0;
        while (cursor < text.size()) {
            const auto open_bracket = text.find('[', cursor);
            if (open_bracket == std::string::npos) break;
            const auto close_bracket = text.find(']', open_bracket + 1);
            if (close_bracket == std::string::npos) break;
            const auto raw_id = detail::trim_copy(text.substr(open_bracket + 1, close_bracket - open_bracket - 1));
            const auto item_id = detail::parse_u64(raw_id);
            if (!item_id.has_value()) {
                cursor = close_bracket + 1;
                continue;
            }
            const auto equals = text.find('=', close_bracket + 1);
            if (equals == std::string::npos) break;
            const auto outer_open = text.find('{', equals + 1);
            if (outer_open == std::string::npos) break;
            const auto outer_close = FindMatchingLuaBrace(text, outer_open);
            if (!outer_close.has_value()) break;

            const auto inner_open = text.find('{', outer_open + 1);
            if (inner_open != std::string::npos && inner_open < *outer_close) {
                const auto inner_close = FindMatchingLuaBrace(text, inner_open);
                if (inner_close.has_value() && *inner_close < *outer_close && !seen_item_ids.contains(*item_id)) {
                    TrackerPackItemMappingDefinition mapping;
                    mapping.item_id = *item_id;
                    mapping.codes = ExtractLuaStringLiterals(text.substr(inner_open + 1, *inner_close - inner_open - 1));
                    const auto tail = text.substr(*inner_close + 1, *outer_close - *inner_close - 1);
                    const auto behavior = ExtractLuaStringLiterals(tail);
                    if (!behavior.empty()) {
                        mapping.behavior = behavior.front();
                    }
                    if (!mapping.codes.empty()) {
                        mappings.push_back(std::move(mapping));
                        seen_item_ids.insert(*item_id);
                    }
                }
            }
            cursor = *outer_close + 1;
        }
    }
    return mappings;
}

std::uint64_t VisualStageIndex(const TrackerItemSlotDefinition& slot,
                               const TrackerPackItemVisualDefinition* visual,
                               const TrackerItemSlotState& state) {
    if (visual == nullptr || visual->stages.empty()) {
        return state.stage;
    }
    if (state.stage == 0) {
        return 0;
    }
    if (slot.behavior == "progressive" || slot.behavior == "progressive-toggle" ||
        slot.behavior == "progressive_toggle" || visual->type == "progressive" ||
        visual->type == "progressive-toggle" || visual->type == "progressive_toggle") {
        if (!visual->allow_disabled || visual->initial_active_state) {
            return std::min<std::uint64_t>(visual->stages.size() - 1, state.stage);
        }
        return std::min<std::uint64_t>(visual->stages.size() - 1, state.stage - 1);
    }
    return std::min<std::uint64_t>(visual->stages.size() - 1, state.stage);
}

std::string SelectPackVisualImage(const TrackerItemSlotDefinition& slot,
                                  const TrackerPackItemVisualDefinition* visual,
                                  const TrackerItemSlotState& state,
                                  bool disabled) {
    if (visual == nullptr) return {};
    if (!visual->stages.empty()) {
        const auto index = static_cast<std::size_t>(VisualStageIndex(slot, visual, state));
        if (disabled && index < visual->stages.size() && !visual->stages[index].disabled_image.empty()) {
            return visual->stages[index].disabled_image;
        }
        if (index < visual->stages.size()) {
            return visual->stages[index].image;
        }
    }
    if (disabled && !visual->disabled_image.empty()) {
        return visual->disabled_image;
    }
    return visual->image;
}
