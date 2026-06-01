#include "tracker_headless_runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace sekailink::sklmi;

namespace {

std::string Slurp(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::size_t CountOccurrences(const std::string& text, const std::string& needle) {
    if (needle.empty()) return 0;
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

bool IsDungeonPackCode(const std::string& code) {
    return Contains(code, "_smallkey") ||
           Contains(code, "_bigkey") ||
           Contains(code, "_compass") ||
           Contains(code, "_map");
}

bool IsPackCounterBehavior(const std::string& behavior) {
    return behavior == "consumable" || behavior == "combined_consumable";
}

bool ContainsOrdered(const std::string& text, std::initializer_list<std::string> needles) {
    std::size_t position = 0;
    for (const auto& needle : needles) {
        const auto found = text.find(needle, position);
        if (found == std::string::npos) {
            return false;
        }
        position = found + needle.size();
    }
    return true;
}

void AppendText(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::app);
    output << text;
}

void WriteText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

std::vector<TrackerPackItemMappingDefinition> DungeonPackItemMappings(const TrackerBundleModel& bundle) {
    std::vector<TrackerPackItemMappingDefinition> mappings;
    for (const auto& mapping : bundle.pack_item_mappings) {
        for (const auto& code : mapping.codes) {
            if (IsDungeonPackCode(code)) {
                mappings.push_back(mapping);
                break;
            }
        }
    }
    return mappings;
}

}  // namespace

int main() {
    const char* canonical_root = std::getenv("SEKAILINK_CANONICAL_ROOT");
    const fs::path bundle_path =
        canonical_root != nullptr
            ? fs::path(canonical_root) / "linkedworlds/alttp/tracker/default.bundle"
            : fs::path("linkedworlds/alttp/tracker/default.bundle");
    if (!fs::exists(bundle_path / "manifest.json")) {
        std::cerr << "tracker_bundle_missing\n";
        return EXIT_FAILURE;
    }

    const auto root = fs::temp_directory_path() / "sklmi-tracker-alttp-bundle";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    WriteText(root / "room.state",
              "meta|slot_id|Jade-ALTTP\n"
              "meta|seed_id|seed-alttp-001\n"
              "meta|tracker_pack|alttp-ap-tracker\n"
              "meta|tracker_variant|Map Tracker - AP\n"
              "meta|slot_data|{\"goal\":\"ganon\",\"mode\":\"open\"}\n");

    TrackerHeadlessRuntime runtime;
    std::string error;
    if (!runtime.Initialize(
            {.bundle_path = bundle_path,
             .snapshot_path = root / "tracker.snapshot.json",
             .command_log_path = root / "tracker.commands.jsonl",
             .room_state_path = root / "room.state",
             .tracker_variant = "Map Tracker - AP"},
            &error)) {
        std::cerr << "tracker_init_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    runtime.ApplyEvent({EventType::item_received, "item.progressive_sword", "Progressive Sword", "", "", "", 94});
    runtime.ApplyEvent({EventType::item_received, "item.progressive_glove", "Progressive Glove", "", "", "", 97});
    runtime.ApplyEvent({EventType::item_received, "item.progressive_bow", "Progressive Bow", "", "", "", 100});
    runtime.ApplyEvent({EventType::item_received, "item.progressive_mail", "Progressive Mail", "", "", "", 96});
    runtime.ApplyEvent({EventType::item_received, "item.blue_boomerang", "Blue Boomerang", "", "", "", 12});
    runtime.ApplyEvent({EventType::item_received, "item.red_boomerang", "Red Boomerang", "", "", "", 42});
    runtime.ApplyEvent({EventType::item_received, "item.bombs_10", "Bomb Upgrade (+10)", "", "", "", 27});
    runtime.ApplyEvent({EventType::item_received, "ap:0:61:60025:1", "Bottle (Fairy)", "", "", "", 61});
    runtime.ApplyEvent({EventType::item_received, "ap:2:54:59836:1", "Rupees (20)", "", "", "", 54});
    const auto dungeon_item_mappings = DungeonPackItemMappings(runtime.bundle());
    if (dungeon_item_mappings.size() < 50) {
        std::cerr << "tracker_alttp_dungeon_item_mapping_incomplete\n";
        return EXIT_FAILURE;
    }
    std::uint64_t pack_delivery_index = 0;
    for (const auto& mapping : dungeon_item_mappings) {
        runtime.ApplyEvent({EventType::item_received,
                            "ap:pack-dungeon:" + std::to_string(pack_delivery_index++),
                            "Dungeon Pack Item",
                            "",
                            "",
                            "",
                            mapping.item_id});
    }
    runtime.ApplyEvent({EventType::location_checked, "0xEB0F", "Blind's Hideout - Top", "", "", "", 60175});
    runtime.ApplyEvent({EventType::location_checked, "0xEA79", "Sanctuary", "", "", "", 60025});

    AppendText(root / "tracker.commands.jsonl",
               "{\"cmd\":\"tracker.click_item\",\"code\":\"sword\",\"button\":\"left\"}\n"
               "{\"cmd\":\"tracker.click_pin\",\"group_id\":\"light_world_caves\",\"button\":\"left\"}\n");
    runtime.PollCommands();

    if (!runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto snapshot = Slurp(root / "tracker.snapshot.json");
    if (!Contains(snapshot, "\"game\":\"A Link to the Past\"") ||
        !Contains(snapshot, "\"slot\":\"Jade-ALTTP\"") ||
        !Contains(snapshot, "\"seed\":\"seed-alttp-001\"") ||
        !Contains(snapshot, "\"slot_data\":{\"goal\":\"ganon\",\"mode\":\"open\"}") ||
        !Contains(snapshot, "\"maps\":[") ||
        !Contains(snapshot, "\"pack_maps\":[") ||
        !Contains(snapshot, "\"pack_layouts\":[") ||
        !Contains(snapshot, "\"layout_ids\":[") ||
        !Contains(snapshot, "\"item_icon_groups\":[") ||
        !Contains(snapshot, "\"pack_item_visuals\":[") ||
        !Contains(snapshot, "\"name\":\"Eastern Palace Reward\"") ||
        !Contains(snapshot, "\"primary_code\":\"ep\"") ||
        !Contains(snapshot, "\"stages\":[") ||
        !Contains(snapshot, "\"checked_locations\":[") ||
        !Contains(snapshot, "\"received_items\":[") ||
        !Contains(snapshot, "\"id\":\"sword\"") ||
        !Contains(snapshot, "\"icon_key\":\"item.sword\"") ||
        !Contains(snapshot, "\"render_hint\":\"progressive-stage\"") ||
        !Contains(snapshot, "\"pack_visual_code\":\"sword\"") ||
        !Contains(snapshot, "\"image\":\"poptracker-adapted/images/items/sword2.png\"") ||
        !Contains(snapshot, "\"stage_index\":1") ||
        !Contains(snapshot, "\"stage\":2") ||
        !Contains(snapshot, "\"id\":\"glove\"") ||
        !Contains(snapshot, "\"asset_candidates\":[") ||
        !Contains(snapshot, "\"id\":\"bow\"") ||
        !Contains(snapshot, "\"id\":\"mail\"") ||
        !Contains(snapshot, "\"pack_visual_code\":\"armor\"") ||
        !Contains(snapshot, "\"image\":\"poptracker-adapted/images/items/Blue_Mail.png\"") ||
        !Contains(snapshot, "\"id\":\"boomerang\"") ||
        !Contains(snapshot, "\"count\":2") ||
        !Contains(snapshot, "\"id\":\"bombs\"") ||
        !ContainsOrdered(snapshot, {"\"id\":\"bottle\"", "\"stage\":1", "\"acquired\":true"}) ||
        !ContainsOrdered(snapshot, {"\"id\":\"dungeon_items\"", "\"stage\":1", "\"acquired\":true"}) ||
        !ContainsOrdered(snapshot, {"\"id\":\"rupees\"", "\"stage\":1", "\"acquired\":true"}) ||
        !Contains(snapshot, "\"group_id\":\"resources\"") ||
        !Contains(snapshot, "\"id\":\"light_world_caves\"") ||
        !Contains(snapshot, "\"pins_detailed\":[") ||
        !Contains(snapshot, "\"location_id\":60175") ||
        !Contains(snapshot, "\"mixed\":true") ||
        !Contains(snapshot, "\"color\":\"green\"") ||
        !Contains(snapshot, "\"logic_ready\":true") ||
        !Contains(snapshot, "\"pack\":\"alttp-ap-tracker\"") ||
        !Contains(snapshot, "\"variant\":\"Map Tracker - AP\"")) {
        std::cerr << "tracker_alttp_snapshot_invalid\n";
        return EXIT_FAILURE;
    }
    if (!Contains(snapshot, "\"active_map\":\"light_world\"")) {
        std::cerr << "tracker_alttp_location_check_changed_active_map\n";
        return EXIT_FAILURE;
    }
    if (!ContainsOrdered(snapshot, {"\"pack_location_id\":\"Lightworld/Kakariko Well\"", "\"label\":\"Kakariko Well\"", "\"pack_map\":\"Lightworld\"", "\"x\":40", "\"y\":850"}) ||
        !Contains(snapshot, "\"section_id\":\"Lightworld/Kakariko Well/Back Chest\"") ||
        !Contains(snapshot, "\"section_id\":\"Lightworld/Kakariko Well/Well Items\"") ||
        !Contains(snapshot, "\"label\":\"Kakariko Well - Top\"") ||
        !Contains(snapshot, "\"label\":\"Kakariko Well - Bottom\"") ||
        !ContainsOrdered(snapshot, {"\"pack_location_id\":\"Lightworld/Blind's Hideout\"", "\"label\":\"Blind's Hideout\"", "\"pack_map\":\"Lightworld\"", "\"x\":250", "\"y\":840"}) ||
        !Contains(snapshot, "\"section_id\":\"Lightworld/Blind's Hideout/Back Chest\"") ||
        !Contains(snapshot, "\"section_id\":\"Lightworld/Blind's Hideout/Hideout\"") ||
        !Contains(snapshot, "\"label\":\"Blind's Hideout - Far Right\"")) {
        std::cerr << "tracker_alttp_multi_check_pins_invalid\n";
        return EXIT_FAILURE;
    }
    if (!ContainsOrdered(snapshot, {"\"pack_location_id\":\"Lightworld/Lost Woods\"", "\"pack_map\":\"Lightworld\"", "\"x\":250", "\"y\":235"}) ||
        !Contains(snapshot, "\"label\":\"Mushroom\"") ||
        !ContainsOrdered(snapshot, {"\"pack_location_id\":\"Lightworld/Zora's Domain\"", "\"pack_map\":\"Lightworld\"", "\"x\":1915", "\"y\":250"}) ||
        !Contains(snapshot, "\"label\":\"King Zora\"") ||
        !Contains(snapshot, "\"label\":\"Zora's Ledge\"") ||
        !ContainsOrdered(snapshot, {"\"pack_location_id\":\"Lightworld/Race\"", "\"pack_map\":\"Lightworld\"", "\"x\":55", "\"y\":1400"}) ||
        !Contains(snapshot, "\"label\":\"Maze Race\"") ||
        !ContainsOrdered(snapshot, {"\"label\":\"Hobo\"", "\"pack_map\":\"Lightworld\"", "\"x\":1420", "\"y\":1390"}) ||
        !ContainsOrdered(snapshot, {"\"pack_location_id\":\"Lightworld/Secret Passage\"", "\"pack_map\":\"Lightworld\""}) ||
        !Contains(snapshot, "\"label\":\"Link's Uncle\"") ||
        !ContainsOrdered(snapshot, {"\"pack_location_id\":\"Darkworld Bottom/Hype Cave\"", "\"pack_map\":\"Darkworld\"", "\"x\":1195", "\"y\":1555"}) ||
        !Contains(snapshot, "\"label\":\"Hype Cave - Top\"")) {
        std::cerr << "tracker_alttp_cross_group_pack_pins_missing\n";
        return EXIT_FAILURE;
    }
    if (!Contains(snapshot, "\"pack_location_id\":\"Lightworld/Kakariko Well\"") ||
        !Contains(snapshot, "\"pack_location_id\":\"Eastern Palace/Big Key Chest\"") ||
        Contains(snapshot, "\"pack_location_id\":\"Darkworld Bottom/Link's House\"")) {
        std::cerr << "tracker_alttp_pack_location_pin_state_invalid\n";
        return EXIT_FAILURE;
    }
    if (CountOccurrences(snapshot, "\"pack_location_id\":\"Lightworld/Link's House\"") != 1 ||
        !ContainsOrdered(snapshot,
                         {"\"pack_location_id\":\"Lightworld/Link's House\"",
                          "\"map_id\":\"light_world\"",
                          "\"pack_map\":\"Lightworld\"",
                          "\"total_count\":1",
                          "\"color\":\"green\""}) ||
        !ContainsOrdered(snapshot,
                         {"\"pack_location_id\":\"EP/Eastern Palace\"",
                          "\"map_id\":\"light_world\"",
                          "\"pack_map\":\"Lightworld\"",
                          "\"total_count\":8"}) ||
        !ContainsOrdered(snapshot,
                         {"\"pack_location_id\":\"ToH/Tower of Hera\"",
                          "\"map_id\":\"light_world\"",
                          "\"pack_map\":\"Lightworld\""})) {
        std::cerr << "tracker_alttp_poptracker_map_location_visibility_invalid\n";
        return EXIT_FAILURE;
    }
    if (Contains(snapshot, "\"pack_map\":\"lightworld_er\"") ||
        Contains(snapshot, "\"pack_map\":\"lightworld_caves_er\"") ||
        Contains(snapshot, "\"pack_map\":\"lightworld_caves_reduced_er\"") ||
        Contains(snapshot, "\"pack_map\":\"darkworld_er\"") ||
        Contains(snapshot, "\"pack_map\":\"darkworld_caves_er\"") ||
        Contains(snapshot, "\"pack_map\":\"darkworld_caves_reduced_er\"")) {
        std::cerr << "tracker_alttp_entrance_shuffle_pins_leaked\n";
        return EXIT_FAILURE;
    }
    if (Contains(snapshot, "\"label\":\"Link's House\",\"map_id\":\"light_world\",\"pack_map\":\"dam\"") ||
        Contains(snapshot, "\"label\":\"Link's House\",\"map_id\":\"light_world\",\"pack_map\":\"dam_caves\"")) {
        std::cerr << "tracker_alttp_visual_entrance_pin_leaked_as_check\n";
        return EXIT_FAILURE;
    }
    const auto logic_result = Slurp(root / "tracker.logic.result");
    if (!Contains(logic_result, "pack_location|Lightworld/Kakariko Well|green|") ||
        !Contains(logic_result, "pack_location|Darkworld Bottom/Link's House|hidden|")) {
        std::cerr << "tracker_alttp_pack_location_logic_invalid\n";
        return EXIT_FAILURE;
    }
    const auto logic_state = Slurp(root / "tracker.logic.state");
    for (const auto& mapping : dungeon_item_mappings) {
        const bool counter = IsPackCounterBehavior(mapping.behavior);
        for (const auto& code : mapping.codes) {
            if (!IsDungeonPackCode(code)) continue;
            if (counter) {
                if (!ContainsOrdered(snapshot, {"\"id\":\"" + code + "\"", "\"count\":1", "\"acquired\":true"}) ||
                    !Contains(logic_state, "slot|" + code + "|1|0|1\n")) {
                    std::cerr << "tracker_alttp_pack_code_counter_invalid:" << code << "\n";
                    return EXIT_FAILURE;
                }
            } else {
                if (!ContainsOrdered(snapshot, {"\"id\":\"" + code + "\"", "\"stage\":1", "\"acquired\":true"}) ||
                    !Contains(logic_state, "slot|" + code + "|1|1|0\n")) {
                    std::cerr << "tracker_alttp_pack_code_toggle_invalid:" << code << "\n";
                    return EXIT_FAILURE;
                }
            }
        }
    }

    std::cout << "sklmi_tracker_alttp_bundle_smoke_ok\n";
    return EXIT_SUCCESS;
}
