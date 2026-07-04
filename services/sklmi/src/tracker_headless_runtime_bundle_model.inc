bool TrackerHeadlessRuntime::LoadBundleModel(const std::filesystem::path& bundle_path, std::string* error) {
    try {
        const auto manifest_text = ReadTextFile(bundle_path / "manifest.json");
        const auto item_slots_text = ReadTextFile(bundle_path / "item-slots.complete.json");
        const auto location_groups_text = ReadTextFile(bundle_path / "location-groups.complete.json");
        const auto pin_metadata_text = ReadTextFile(bundle_path / "map-pin-metadata.json");
        const auto surface_text = ReadTextFile(bundle_path / "surface.complete.json");
        const auto item_icon_metadata_text = std::filesystem::exists(bundle_path / "item-icon-metadata.json")
                                                 ? ReadTextFile(bundle_path / "item-icon-metadata.json")
                                                 : std::string{};
        const auto autotabbing_hints_text = std::filesystem::exists(bundle_path / "autotabbing-hints.json")
                                                ? ReadTextFile(bundle_path / "autotabbing-hints.json")
                                                : std::string{};
        const auto tracker_flow_text = std::filesystem::exists(bundle_path / "tracker-flow.v1.json")
                                           ? ReadTextFile(bundle_path / "tracker-flow.v1.json")
                                           : std::string{};
        const auto poptracker_maps_path = bundle_path / "poptracker-adapted" / "maps" / "maps.json";
        const auto poptracker_maps_text =
            std::filesystem::exists(poptracker_maps_path) ? ReadTextFile(poptracker_maps_path) : std::string{};
        bundle_.has_pack_location_mapping =
            std::filesystem::exists(bundle_path / "poptracker-adapted" / "scripts" / "autotracking" / "location_mapping.lua");

        bundle_.linkedworld_id = detail::extract_string_field(manifest_text, "linkedworld_id").value_or("");
        bundle_.display_name = detail::extract_string_field(manifest_text, "display_name").value_or(bundle_.linkedworld_id);
        bundle_.default_tab_id = detail::extract_string_field(manifest_text, "default_tab_id").value_or("");
        bundle_.default_map_id = detail::extract_string_field(manifest_text, "default_map_id").value_or("");

        for (const auto& block : detail::extract_object_blocks(manifest_text, "tabs")) {
            TrackerTabDefinition tab;
            tab.id = detail::extract_string_field(block, "id").value_or("");
            tab.label = detail::extract_string_field(block, "label").value_or(tab.id);
            tab.map_id = detail::extract_string_field(block, "map_id")
                             .value_or(detail::extract_string_field(block, "map").value_or(""));
            if (!tab.id.empty()) {
                if (!tab.map_id.empty()) {
                    bundle_.tab_map_hints[tab.id] = tab.map_id;
                }
                bundle_.tabs.push_back(std::move(tab));
            }
        }

        for (const auto& block : detail::extract_object_blocks(manifest_text, "maps")) {
            TrackerMapDefinition map;
            map.id = detail::extract_string_field(block, "id").value_or("");
            map.label = detail::extract_string_field(block, "label").value_or(map.id);
            map.poptracker_map_name =
                detail::extract_string_field(block, "poptracker_map_name")
                    .value_or(detail::extract_string_field(block, "pack_map_name")
                                  .value_or(detail::extract_string_field(block, "packMapName").value_or("")));
            map.image = detail::extract_string_field(block, "image").value_or("");
            map.art_origin = detail::extract_string_field(block, "art_origin").value_or("");
            if (!map.id.empty()) bundle_.maps.push_back(std::move(map));
        }

        bundle_.tab_order = ExtractStringArray(surface_text, "tab_order");
        if (bundle_.tab_order.empty()) {
            for (const auto& tab : bundle_.tabs) bundle_.tab_order.push_back(tab.id);
        }

        for (const auto& block : detail::extract_object_blocks(autotabbing_hints_text, "location_group_hints")) {
            const auto group_id = detail::extract_string_field(block, "group_id").value_or("");
            const auto tab_id = detail::extract_string_field(block, "tab_id").value_or("");
            if (!group_id.empty() && !tab_id.empty()) {
                bundle_.location_group_tab_hints[group_id] = tab_id;
            }
        }
        for (const auto& block : detail::extract_object_blocks(tracker_flow_text, "tab_to_map")) {
            const auto tab_id = detail::extract_string_field(block, "tab_id").value_or("");
            const auto map_id = detail::extract_string_field(block, "map_id")
                                    .value_or(detail::extract_string_field(block, "map").value_or(""));
            if (!tab_id.empty() && !map_id.empty()) {
                bundle_.tab_map_hints[tab_id] = map_id;
            }
        }
        for (const auto& block : detail::extract_object_blocks(tracker_flow_text, "autotab_zone_rules")) {
            const auto zone_id = detail::extract_string_field(block, "zone_id").value_or("");
            TrackerAutoTabTarget target;
            target.tab_id = detail::extract_string_field(block, "tab_id").value_or("");
            target.map_id = detail::extract_string_field(block, "map_id")
                                .value_or(detail::extract_string_field(block, "map").value_or(""));
            if (!zone_id.empty() && (!target.tab_id.empty() || !target.map_id.empty())) {
                bundle_.zone_tab_hints[zone_id] = std::move(target);
            }
        }

        for (const auto& block : detail::extract_object_blocks(item_slots_text, "slots")) {
            TrackerItemSlotDefinition slot;
            slot.slot_id = detail::extract_string_field(block, "slot_id").value_or("");
            slot.label = detail::extract_string_field(block, "label").value_or(slot.slot_id);
            slot.group_id = detail::extract_string_field(block, "group_id").value_or("");
            slot.behavior = detail::extract_string_field(block, "behavior").value_or("toggle");
            slot.max_stage = detail::extract_uint_field(block, "max_stage").value_or(0);
            for (const auto& item_block : detail::extract_object_blocks(block, "items")) {
                TrackerItemIdentity item;
                item.item_id = detail::extract_uint_field(item_block, "item_id").value_or(0);
                item.event_key = detail::extract_string_field(item_block, "event_key").value_or("");
                item.item_name = detail::extract_string_field(item_block, "item_name").value_or("");
                item.mapped_value = detail::extract_string_field(item_block, "mapped_value").value_or("");
                slot.items.push_back(std::move(item));
            }
            if (!slot.slot_id.empty()) bundle_.item_slots.push_back(std::move(slot));
        }

        for (const auto& block : detail::extract_object_blocks(location_groups_text, "groups")) {
            TrackerLocationGroupDefinition group;
            group.group_id = detail::extract_string_field(block, "group_id").value_or("");
            group.label = detail::extract_string_field(block, "label").value_or(group.group_id);
            group.preferred_tab = detail::extract_string_field(block, "preferred_tab").value_or("");
            for (const auto& location_block : detail::extract_object_blocks(block, "locations")) {
                TrackerLocationDefinition location;
                location.location_id = detail::extract_uint_field(location_block, "location_id").value_or(0);
                location.location_name = detail::extract_string_field(location_block, "location_name").value_or("");
                location.event_key = detail::extract_string_field(location_block, "event_key").value_or("");
                location.domain_id = detail::extract_string_field(location_block, "domain_id").value_or("");
                group.locations.push_back(std::move(location));
            }
            if (!group.group_id.empty()) bundle_.location_groups.push_back(std::move(group));
        }

        for (const auto& block : detail::extract_object_blocks(pin_metadata_text, "pin_layers")) {
            TrackerPinLayerDefinition layer;
            layer.group_id = detail::extract_string_field(block, "group_id").value_or("");
            layer.preferred_tab = detail::extract_string_field(block, "preferred_tab").value_or("");
            layer.map_id = detail::extract_string_field(block, "map_id").value_or("");
            layer.pin_count = detail::extract_uint_field(block, "pin_count").value_or(0);
            layer.pin_kind = detail::extract_string_field(block, "pin_kind").value_or("");
            if (!layer.group_id.empty()) bundle_.pin_layers.push_back(std::move(layer));
        }

        for (const auto& block : detail::extract_object_blocks(item_icon_metadata_text, "icon_groups")) {
            TrackerItemIconGroupDefinition group;
            group.group_id = detail::extract_string_field(block, "group_id").value_or("");
            group.default_palette = detail::extract_string_field(block, "default_palette").value_or("");
            if (!group.group_id.empty()) bundle_.item_icon_groups.push_back(std::move(group));
        }

        std::vector<std::filesystem::path> item_asset_files;
        const auto item_asset_root = bundle_path / "poptracker-adapted" / "images" / "items";
        if (std::filesystem::exists(item_asset_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(item_asset_root, ec)) {
                if (ec) break;
                if (entry.is_regular_file()) item_asset_files.push_back(entry.path());
            }
        }
        for (const auto& block : detail::extract_object_blocks(item_icon_metadata_text, "slot_icon_bindings")) {
            TrackerItemIconBindingDefinition binding;
            binding.slot_id = detail::extract_string_field(block, "slot_id").value_or("");
            binding.icon_key = detail::extract_string_field(block, "icon_key").value_or("");
            binding.render_hint = detail::extract_string_field(block, "render_hint").value_or("");
            if (binding.slot_id.empty()) continue;
            const auto leaf =
                binding.icon_key.find_last_of('.') == std::string::npos
                    ? binding.icon_key
                    : binding.icon_key.substr(binding.icon_key.find_last_of('.') + 1);
            const auto canonical_leaf = CanonicalToken(leaf);
            for (const auto& asset_path : item_asset_files) {
                const auto canonical_asset = CanonicalToken(asset_path.stem().string());
                if (canonical_leaf.empty() || canonical_asset.find(canonical_leaf) == std::string::npos) {
                    continue;
                }
                binding.asset_candidates.push_back(
                    asset_path.lexically_relative(bundle_path).generic_string());
            }
            bundle_.item_icon_bindings.push_back(std::move(binding));
        }

        bundle_.pack_item_visuals = LoadPackItemVisualDefinitions(bundle_path);
        bundle_.pack_item_mappings = LoadPackItemMappings(bundle_path);

        const auto layouts_root = bundle_path / "poptracker-adapted" / "layouts";
        if (std::filesystem::exists(layouts_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(layouts_root, ec)) {
                if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
                auto layout_json = detail::trim_copy(StripJsonComments(ReadTextFile(entry.path())));
                if (layout_json.empty() || layout_json.front() != '{' || layout_json.back() != '}') {
                    continue;
                }
                TrackerPackLayoutDefinition layout;
                layout.file_name = entry.path().filename().string();
                layout.layout_ids = ExtractTopLevelObjectKeys(layout_json);
                layout.json = std::move(layout_json);
                bundle_.pack_layouts.push_back(std::move(layout));
            }
        }
        bundle_.supported_pack_maps = DeriveSupportedPackMaps(bundle_path, bundle_.pack_layouts);

        for (const auto& block : ExtractTopLevelObjectBlocks(poptracker_maps_text)) {
            TrackerPackMapDefinition map;
            map.name = detail::extract_string_field(block, "name").value_or("");
            map.image = detail::extract_string_field(block, "img").value_or(
                detail::extract_string_field(block, "image").value_or(""));
            if (!map.name.empty() && !map.image.empty()) {
                if (map.image.front() == '/') {
                    map.image.erase(map.image.begin());
                }
                bundle_.pack_maps.push_back(std::move(map));
            }
        }

        std::unordered_map<std::string, std::string> map_asset_by_name;
        for (const auto& map : bundle_.pack_maps) {
            map_asset_by_name[map.name] = "poptracker-adapted/" + map.image;
        }
        LocationCatalog location_catalog;
        std::vector<TrackerGroupLabel> group_labels;
        std::unordered_map<std::uint64_t, std::string> group_id_by_location_id;
        for (const auto& group : bundle_.location_groups) {
            if (!group.group_id.empty() && !group.label.empty()) {
                group_labels.push_back(TrackerGroupLabel{group.group_id, group.label});
            }
            for (const auto& location : group.locations) {
                location_catalog.push_back(MatchedTrackerLocation{group.group_id, &location});
                group_id_by_location_id[location.location_id] = group.group_id;
            }
        }
        std::unordered_map<std::string, const TrackerPinLayerDefinition*> layer_by_group;
        for (const auto& layer : bundle_.pin_layers) {
            layer_by_group[layer.group_id] = &layer;
        }

        const auto locations_root = bundle_path / "poptracker-adapted" / "locations";
        if (std::filesystem::exists(locations_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(locations_root, ec)) {
                if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
                const auto file_text = ReadTextFile(entry.path());
                for (const auto& group_block : ExtractTopLevelObjectBlocks(file_text)) {
                    const auto group_name = detail::extract_string_field(group_block, "name").value_or("");
                    std::vector<std::string> child_blocks = detail::extract_object_blocks(group_block, "children");
                    const bool has_children = !child_blocks.empty();
                    if (child_blocks.empty()) child_blocks.push_back(group_block);
                    for (const auto& child_block : child_blocks) {
                        const auto child_name = detail::extract_string_field(child_block, "name").value_or("");
                        const auto pack_location_id = has_children && !group_name.empty()
                                                          ? group_name + "/" + child_name
                                                          : (child_name.empty() ? group_name : child_name);
                        const auto group_hint = ResolvePopTrackerGroupHint(group_labels, child_name, group_name);
                        const auto matched_locations =
                            ResolvePopTrackerChildLocations(location_catalog, child_block, child_name, group_hint);
                        if (matched_locations.empty()) continue;

                        for (const auto& map_location_block : detail::extract_object_blocks(child_block, "map_locations")) {
                            const auto pack_map = detail::extract_string_field(map_location_block, "map").value_or("");
                            if (!IsSupportedPackMap(bundle_, pack_map)) continue;
                            const auto x = detail::extract_uint_field(map_location_block, "x").value_or(0);
                            const auto y = detail::extract_uint_field(map_location_block, "y").value_or(0);
                            const auto size = detail::extract_uint_field(map_location_block, "size").value_or(0);
                            for (const auto& match : matched_locations) {
                                if (match.location == nullptr) continue;
                                const auto group_id = !match.group_id.empty()
                                                          ? match.group_id
                                                          : group_id_by_location_id[match.location->location_id];
                                bool duplicate = false;
                                for (const auto& existing : bundle_.detailed_pins) {
                                    if (existing.group_id == group_id &&
                                        existing.pack_location_id == pack_location_id &&
                                        existing.location_id == match.location->location_id &&
                                        existing.pack_map == pack_map &&
                                        existing.x == static_cast<double>(x) &&
                                        existing.y == static_cast<double>(y)) {
                                        duplicate = true;
                                        break;
                                    }
                                }
                                if (duplicate) continue;

                                TrackerDetailedPinDefinition pin;
                                pin.pin_id = group_id.empty() ? std::to_string(match.location->location_id)
                                                              : group_id + ":" + std::to_string(match.location->location_id);
                                pin.group_id = group_id;
                                pin.pack_location_id = pack_location_id;
                                pin.location_id = match.location->location_id;
                                pin.location_name = match.location->location_name;
                                pin.pack_map = pack_map;
                                pin.map_asset = map_asset_by_name.contains(pack_map)
                                                    ? map_asset_by_name.at(pack_map)
                                                    : std::string{};
                                pin.pin_kind = layer_by_group.contains(group_id)
                                                   ? layer_by_group.at(group_id)->pin_kind
                                                   : std::string{};
                                pin.map_id = ResolveDetailedPinMapId(bundle_, pack_map, group_id);
                                pin.x = static_cast<double>(x);
                                pin.y = static_cast<double>(y);
                                pin.size = static_cast<double>(size);
                                bundle_.detailed_pins.push_back(std::move(pin));
                            }
                        }
                    }
                }
            }
        }

        if (bundle_.tabs.empty()) {
            for (const auto& tab_id : bundle_.tab_order) {
                TrackerTabDefinition tab;
                tab.id = tab_id;
                tab.label = tab_id;
                for (const auto& layer : bundle_.pin_layers) {
                    if (layer.preferred_tab == tab_id && !layer.map_id.empty()) {
                        tab.map_id = layer.map_id;
                        bundle_.tab_map_hints[tab.id] = tab.map_id;
                        break;
                    }
                }
                bundle_.tabs.push_back(std::move(tab));
            }
        }

        if (bundle_.linkedworld_id.empty() || bundle_.default_tab_id.empty() || bundle_.item_slots.empty() ||
            bundle_.location_groups.empty()) {
            if (error) *error = "tracker_bundle_incomplete";
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        if (error) *error = exception.what();
        return false;
    }
}
