std::unordered_map<std::string, std::string> BuildLayoutBlocksById(
    const std::vector<TrackerPackLayoutDefinition>& layouts) {
    std::unordered_map<std::string, std::string> blocks_by_id;
    for (const auto& layout : layouts) {
        for (const auto& layout_id : layout.layout_ids) {
            auto block = ExtractTopLevelFieldRaw(layout.json, layout_id);
            if (!block.has_value()) continue;
            *block = detail::trim_copy(*block);
            if (block->empty() || block->front() != '{') continue;
            blocks_by_id[layout_id] = std::move(*block);
        }
    }
    return blocks_by_id;
}

void CollectPackMapsFromLayoutBlock(const std::string& block,
                                    const std::unordered_map<std::string, std::string>& blocks_by_id,
                                    std::vector<std::string>& maps,
                                    std::unordered_set<std::string>& visited_layouts) {
    AppendUniqueList(maps, ExtractAllStringArrayValues(block, "maps"));
    for (const auto& layout_key : ExtractAllStringFieldValues(block, "key")) {
        if (!visited_layouts.insert(layout_key).second) continue;
        const auto block_it = blocks_by_id.find(layout_key);
        if (block_it == blocks_by_id.end()) continue;
        CollectPackMapsFromLayoutBlock(block_it->second, blocks_by_id, maps, visited_layouts);
    }
}

std::vector<std::string> DeriveSupportedPackMaps(const std::filesystem::path& bundle_path,
                                                 const std::vector<TrackerPackLayoutDefinition>& layouts) {
    const auto root_layout = bundle_path / "poptracker-adapted" / "layouts" / "tabs.json";
    if (!std::filesystem::exists(root_layout)) {
        return {};
    }
    auto root_text = detail::trim_copy(StripJsonComments(ReadTextFile(root_layout)));
    if (root_text.empty()) {
        return {};
    }
    const auto blocks_by_id = BuildLayoutBlocksById(layouts);
    std::vector<std::string> maps;
    std::unordered_set<std::string> visited_layouts;
    CollectPackMapsFromLayoutBlock(root_text, blocks_by_id, maps, visited_layouts);
    return maps;
}

bool IsSupportedPackMap(const TrackerBundleModel& bundle, const std::string& pack_map) {
    return bundle.supported_pack_maps.empty() ||
           std::find(bundle.supported_pack_maps.begin(), bundle.supported_pack_maps.end(), pack_map) !=
               bundle.supported_pack_maps.end();
}

std::string NormalizePackAssetPath(std::string path) {
    if (path.empty()) return {};
    while (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    if (path.rfind("poptracker-adapted/", 0) == 0) {
        return path;
    }
    return "poptracker-adapted/" + path;
}

std::string CanonicalToken(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

std::vector<std::string> CanonicalNameWords(std::string_view value) {
    std::vector<std::string> words;
    std::string current;
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            current.push_back(static_cast<char>(std::tolower(ch)));
            continue;
        }
        if (!current.empty()) {
            if (current != "s") {
                words.push_back(std::move(current));
            }
            current.clear();
        }
    }
    if (!current.empty() && current != "s") {
        words.push_back(std::move(current));
    }
    return words;
}

std::string JoinWords(const std::vector<std::string>& words) {
    std::string out;
    for (const auto& word : words) {
        out += word;
    }
    return out;
}

bool IsBroadLocationWord(std::string_view word) {
    static constexpr std::array<std::string_view, 17> broad_words{
        "back",
        "big",
        "bottom",
        "chest",
        "chests",
        "compass",
        "item",
        "items",
        "key",
        "left",
        "map",
        "middle",
        "right",
        "shop",
        "small",
        "top",
        "upgrade",
    };
    return std::find(broad_words.begin(), broad_words.end(), word) != broad_words.end();
}

bool ContainsAllWords(const std::vector<std::string>& haystack,
                      const std::vector<std::string>& needles) {
    if (haystack.empty() || needles.empty()) return false;
    std::size_t needle_chars = 0;
    for (const auto& needle : needles) {
        if (IsBroadLocationWord(needle)) return false;
        needle_chars += needle.size();
        if (std::find(haystack.begin(), haystack.end(), needle) == haystack.end()) {
            return false;
        }
    }
    return needle_chars >= 4;
}

struct MatchedTrackerLocation {
    std::string group_id;
    const TrackerLocationDefinition* location = nullptr;
};

struct TrackerGroupLabel {
    std::string group_id;
    std::string label;
};

void AppendUniqueLocationMatch(std::vector<MatchedTrackerLocation>& matches,
                               std::string group_id,
                               const TrackerLocationDefinition* location) {
    if (location == nullptr || location->location_id == 0) return;
    for (const auto& match : matches) {
        if (match.location != nullptr && match.location->location_id == location->location_id &&
            match.group_id == group_id) {
            return;
        }
    }
    matches.push_back(MatchedTrackerLocation{std::move(group_id), location});
}

bool StrongLocationNameMatch(std::string_view text, std::string_view location_name) {
    const auto text_words = CanonicalNameWords(text);
    const auto location_words = CanonicalNameWords(location_name);
    const auto text_token = JoinWords(text_words);
    const auto location_token = JoinWords(location_words);
    if (text_token.empty() || location_token.empty()) return false;
    if (text_token == location_token) return true;
    if ((text_token.size() >= 5 && location_token.find(text_token) != std::string::npos) ||
        (location_token.size() >= 5 && text_token.find(location_token) != std::string::npos)) {
        return true;
    }
    return ContainsAllWords(text_words, location_words) ||
           ContainsAllWords(location_words, text_words);
}

bool LocationStartsWithLabel(std::string_view label, std::string_view location_name) {
    const auto label_token = JoinWords(CanonicalNameWords(label));
    const auto location_token = JoinWords(CanonicalNameWords(location_name));
    return label_token.size() >= 5 && location_token.rfind(label_token, 0) == 0;
}

struct PopTrackerAccessTargets {
    std::vector<std::string> targets;
    bool had_access_rules = false;
    bool had_location_rule = false;
};

bool IsVisualOnlyRuleFunction(std::string_view part) {
    if (part.rfind("^$", 0) == 0) {
        part.remove_prefix(2);
    }
    return CanonicalToken(part) == "changelocationcolor";
}

PopTrackerAccessTargets ExtractPopTrackerAccessTargets(const std::string& block) {
    PopTrackerAccessTargets result;
    for (const auto& rule : ExtractStringArray(block, "access_rules")) {
        std::stringstream stream(rule);
        std::string part;
        bool saw_function = false;
        bool visual_only_rule = false;
        result.had_access_rules = true;
        while (std::getline(stream, part, '|')) {
            part = detail::trim_copy(std::move(part));
            if (part.empty()) continue;
            if (!saw_function && part.rfind("^$", 0) == 0) {
                visual_only_rule = IsVisualOnlyRuleFunction(part);
                saw_function = true;
                continue;
            }
            if (visual_only_rule) continue;
            result.had_location_rule = true;
            if (part.front() == '@') {
                part.erase(part.begin());
                std::replace(part.begin(), part.end(), '/', ' ');
            }
            if (!part.empty()) AppendUnique(result.targets, std::move(part));
        }
    }
    return result;
}

bool ShouldUseSectionNameFallback(std::string_view section_name, std::string_view group_hint) {
    const auto token = CanonicalToken(section_name);
    if (token.empty()) return false;
    if (token == "chest" || token == "chests") return false;
    if (group_hint.empty()) {
        static constexpr std::array<std::string_view, 8> broad_tokens{
            "bigchest",
            "dungeonchest",
            "dungeonchests",
            "bossitem",
            "keydrop",
            "keydrops",
            "checks",
            "locations",
        };
        return std::find(broad_tokens.begin(), broad_tokens.end(), token) == broad_tokens.end();
    }
    return true;
}

using LocationCatalog = std::vector<MatchedTrackerLocation>;

void AppendStrongLocationMatches(std::vector<MatchedTrackerLocation>& matches,
                                 const LocationCatalog& catalog,
                                 std::string_view text,
                                 std::string_view group_filter = {}) {
    const auto before = matches.size();
    for (const auto& candidate : catalog) {
        if (!group_filter.empty() && candidate.group_id != group_filter) continue;
        if (candidate.location != nullptr &&
            StrongLocationNameMatch(text, candidate.location->location_name)) {
            AppendUniqueLocationMatch(matches, candidate.group_id, candidate.location);
        }
    }
    if (group_filter.empty() || matches.size() != before) {
        return;
    }
    for (const auto& candidate : catalog) {
        if (candidate.location != nullptr &&
            StrongLocationNameMatch(text, candidate.location->location_name)) {
            AppendUniqueLocationMatch(matches, candidate.group_id, candidate.location);
        }
    }
}

void AppendChildPrefixLocationMatches(std::vector<MatchedTrackerLocation>& matches,
                                      const LocationCatalog& catalog,
                                      std::string_view child_name) {
    for (const auto& candidate : catalog) {
        if (candidate.location != nullptr &&
            LocationStartsWithLabel(child_name, candidate.location->location_name)) {
            AppendUniqueLocationMatch(matches, candidate.group_id, candidate.location);
        }
    }
}

std::vector<MatchedTrackerLocation> ResolvePopTrackerChildLocations(const LocationCatalog& catalog,
                                                                    const std::string& child_block,
                                                                    std::string_view child_name,
                                                                    std::string_view group_hint) {
    std::vector<MatchedTrackerLocation> matches;
    const auto child_targets = ExtractPopTrackerAccessTargets(child_block);
    bool has_location_signal = child_targets.had_location_rule;
    bool has_any_access_rules = child_targets.had_access_rules;
    for (const auto& target : child_targets.targets) {
        AppendStrongLocationMatches(matches, catalog, target, group_hint);
    }

    bool has_multi_check_section = false;
    for (const auto& section_block : detail::extract_object_blocks(child_block, "sections")) {
        const auto before_section = matches.size();
        const auto section_targets = ExtractPopTrackerAccessTargets(section_block);
        has_location_signal = has_location_signal || section_targets.had_location_rule;
        has_any_access_rules = has_any_access_rules || section_targets.had_access_rules;
        for (const auto& target : section_targets.targets) {
            AppendStrongLocationMatches(matches, catalog, target, group_hint);
        }
        const auto section_name = detail::extract_string_field(section_block, "name").value_or("");
        if ((section_targets.had_location_rule || !section_targets.had_access_rules) &&
            ShouldUseSectionNameFallback(section_name, group_hint)) {
            AppendStrongLocationMatches(matches, catalog, section_name, group_hint);
        }
        const bool section_allows_name_fallback =
            has_location_signal || (!child_targets.had_access_rules && !section_targets.had_access_rules);
        if (section_allows_name_fallback && matches.size() == before_section) {
            if ((section_targets.had_location_rule || !section_targets.had_access_rules) &&
                ShouldUseSectionNameFallback(section_name, group_hint)) {
                AppendStrongLocationMatches(matches, catalog, section_name, group_hint);
            }
        }
        const auto item_count = detail::extract_uint_field(section_block, "item_count").value_or(0);
        if (item_count > 1) {
            has_multi_check_section = true;
        }
    }

    const bool allows_name_fallback = has_location_signal || !has_any_access_rules;
    if (allows_name_fallback && (has_multi_check_section || matches.empty())) {
        AppendChildPrefixLocationMatches(matches, catalog, child_name);
    }
    if (allows_name_fallback && matches.empty()) {
        AppendStrongLocationMatches(matches, catalog, child_name, group_hint);
    }
    return matches;
}

std::string ResolvePopTrackerGroupHint(const std::vector<TrackerGroupLabel>& groups,
                                       std::string_view child_name,
                                       std::string_view group_name) {
    for (const auto& source : {child_name, group_name}) {
        for (const auto& group : groups) {
            if (StrongLocationNameMatch(source, group.label)) {
                return group.group_id;
            }
        }
    }
    return {};
}

bool RoomMetadataConnected(const std::unordered_map<std::string, std::string>& metadata) {
    const auto found = metadata.find("connected");
    if (found == metadata.end()) return false;
    const auto token = CanonicalToken(TrimViewCopy(found->second));
    return token == "1" || token == "true" || token == "yes" || token == "on" ||
           token == "connected";
}
