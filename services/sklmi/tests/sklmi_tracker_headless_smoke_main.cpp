#include "tracker_headless_runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace sekailink::sklmi;

namespace {

void WriteText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

void AppendText(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::app);
    output << text;
}

std::string Slurp(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
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

std::string ShellQuote(const std::string& value) {
#if defined(_WIN32)
    std::string out = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            out += "\\\"";
        } else if (ch == '%') {
            out += "%%";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('"');
    return out;
#else
    std::string out = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
#endif
}

std::string LuaString(const std::string& value) {
    std::string out = "'";
    for (const char ch : value) {
        if (ch == '\\') {
            out += "\\\\";
        } else if (ch == '\'') {
            out += "\\'";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
}

}  // namespace

int main() {
    const auto root = fs::temp_directory_path() / "sklmi-tracker-headless-smoke";
    const auto logic_runs_path = root / "logic-runs.txt";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "bundle");

    WriteText(root / "bundle" / "manifest.json", R"JSON({
  "linkedworld_id": "demo",
  "display_name": "Demo World",
  "default_tab_id": "items",
  "default_map_id": "world",
  "tabs": [
    {"id": "items", "label": "Items"},
    {"id": "world-tab", "label": "World", "map_id": "world"},
    {"id": "dungeon-tab", "label": "Dungeon", "map_id": "dungeon"}
  ],
  "maps": [
    {"id": "world", "label": "World Map"},
    {"id": "dungeon", "label": "Dungeon Map"}
  ]
})JSON");

    WriteText(root / "bundle" / "surface.complete.json", R"JSON({
  "presentation": {
    "tab_order": ["items", "world-tab", "dungeon-tab"]
  }
})JSON");

    WriteText(root / "bundle" / "item-slots.complete.json", R"JSON({
  "slots": [
    {
      "slot_id": "sword",
      "label": "Sword",
      "group_id": "combat",
      "behavior": "progressive",
      "max_stage": 4,
      "items": [{"item_id": 1, "item_name": "Progressive Sword", "event_key": "item.progressive_sword"}]
    },
    {
      "slot_id": "bombs",
      "label": "Bombs",
      "group_id": "resources",
      "behavior": "counter",
      "items": [{"item_id": 2, "item_name": "Bomb Upgrade", "event_key": "item.bombs"}]
    }
  ]
})JSON");

    WriteText(root / "bundle" / "location-groups.complete.json", R"JSON({
  "groups": [
    {
      "group_id": "village",
      "label": "Village",
      "preferred_tab": "world-tab",
      "locations": [
        {"location_id": 1001, "location_name": "Village Chest 1", "event_key": "loc.village.1"},
        {"location_id": 1002, "location_name": "Village Chest 2", "event_key": "loc.village.2"}
      ]
    }
  ]
})JSON");

    WriteText(root / "bundle" / "map-pin-metadata.json", R"JSON({
  "pin_layers": [
    {"group_id": "village", "preferred_tab": "world-tab", "map_id": "world", "pin_count": 2, "pin_kind": "check"}
  ]
})JSON");

    WriteText(root / "bundle" / "item-icon-metadata.json", R"JSON({
  "icon_groups": [
    {"group_id": "combat", "default_palette": "red-gold"},
    {"group_id": "resources", "default_palette": "amber"}
  ],
  "slot_icon_bindings": [
    {"slot_id": "sword", "icon_key": "item.sword", "render_hint": "progressive-stage"},
    {"slot_id": "bombs", "icon_key": "item.bombs", "render_hint": "countable-resource"}
  ]
})JSON");
    WriteText(root / "bundle" / "poptracker-adapted" / "maps" / "maps.json", R"JSON([
  {"name": "overworld", "img": "maps/overworld.png"}
])JSON");
    WriteText(root / "bundle" / "poptracker-adapted" / "layouts" / "tracker.json", R"JSON({
  // The loader should keep this as draw data, not execute it as logic.
  "tracker_default": {
    "type": "itemgrid",
    "rows": [["sword", "bombs"]]
  }
})JSON");
    WriteText(root / "bundle" / "poptracker-adapted" / "locations" / "overworld.json", R"JSON([
  {
    "name": "Village",
    "children": [
      {
        "name": "Village Chest 1",
        "map_locations": [{"map": "overworld", "x": 11, "y": 22, "size": 16}]
      },
      {
        "name": "Village Chest 2",
        "map_locations": [{"map": "overworld", "x": 33, "y": 44, "size": 16}]
      }
    ]
  }
])JSON");
    WriteText(root / "bundle" / "poptracker-adapted" / "images" / "items" / "Sword.png", "png");
    WriteText(root / "bundle" / "poptracker-adapted" / "images" / "items" / "Bomb.png", "png");
    WriteText(root / "bundle" / "poptracker-adapted" / "scripts" / "items_import.lua", "");
    WriteText(root / "bundle" / "poptracker-adapted" / "scripts" / "logic" / "logic_helpers.lua", "");
    WriteText(root / "bundle" / "poptracker-adapted" / "scripts" / "logic" / "logic_main.lua",
              "local f = assert(io.open(" + LuaString(logic_runs_path.string()) + ", 'a'))\n"
              "f:write('run\\n')\n"
              "f:close()\n"
              "print('logic-noise-marker')\n"
              "function CanReach(name)\n"
              "  return AccessibilityLevel.Normal\n"
              "end\n");
    WriteText(root / "bundle" / "poptracker-adapted" / "scripts" / "logic_import.lua", "");

    WriteText(root / "room.state",
	              "meta|slot_id|Demo Slot\n"
	              "meta|seed_id|Seed-001\n"
	              "meta|tracker_pack|demo-pack\n"
	              "meta|chat_messages|[{\"id\":7,\"author\":\"Jade\",\"text\":\"hello sync\"}]\n"
	              "meta|slot_data|{\"goal\":\"demo\",\"difficulty\":\"normal\"}\n");

    TrackerHeadlessRuntime runtime;
    std::string error;
    if (!runtime.Initialize(
            {.bundle_path = root / "bundle",
             .snapshot_path = root / "tracker.snapshot.json",
             .command_log_path = root / "tracker.commands.jsonl",
             .room_state_path = root / "room.state",
             .tracker_variant = "Demo Variant"},
            &error)) {
        std::cerr << "tracker_init_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    if (fs::exists(root / "tracker.autosave.state")) {
        std::cerr << "tracker_autosave_written_on_load\n";
        return EXIT_FAILURE;
    }

    runtime.ApplyEvent({EventType::item_received, "item.progressive_sword", "Progressive Sword", "", "", "", 1});
    runtime.ApplyEvent({EventType::item_received, "item.bombs", "Bomb Upgrade", "", "", "", 2});
    runtime.ApplyEvent({EventType::location_checked, "loc.village.1", "Village Chest 1", "", "", "", 1001});

    if (!runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_autotab_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    const auto autotab_snapshot = Slurp(root / "tracker.snapshot.json");
    if (!Contains(autotab_snapshot, "\"active_map\":\"world\"") ||
        !Contains(autotab_snapshot, "\"active_tab\":\"world-tab\"") ||
        !Contains(autotab_snapshot, "\"auto_follow_map\":true") ||
        !Contains(autotab_snapshot, "\"pack_layouts\":[") ||
        !Contains(autotab_snapshot, "\"file\":\"tracker.json\"") ||
        !Contains(autotab_snapshot, "\"layout_ids\":[\"tracker_default\"]")) {
        std::cerr << "tracker_location_autotab_failed\n";
        return EXIT_FAILURE;
    }
    const auto logic_runs_after_autotab_publish = Slurp(logic_runs_path);
    if (logic_runs_after_autotab_publish != "run\n" && logic_runs_after_autotab_publish != "run\r\n") {
        std::cerr << "tracker_autotab_logic_runner_invalid\n";
        return EXIT_FAILURE;
    }

    Event context_map;
    context_map.type = EventType::map_changed;
    context_map.key = "room.two";
    context_map.value = "Room Two";
    context_map.tab_id = "dungeon-tab";
    context_map.map_id = "dungeon";
    runtime.ApplyEvent(context_map);
    if (!runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_context_map_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    const auto context_map_snapshot = Slurp(root / "tracker.snapshot.json");
    if (!Contains(context_map_snapshot, "\"active_map\":\"dungeon\"") ||
        !Contains(context_map_snapshot, "\"active_tab\":\"dungeon-tab\"") ||
        !Contains(context_map_snapshot, "\"auto_follow_map\":true")) {
        std::cerr << "tracker_context_map_event_failed\n";
        return EXIT_FAILURE;
    }

    WriteText(root / "tracker.commands.jsonl",
              "{\"cmd\":\"tracker.click_item\",\"code\":\"sword\",\"button\":\"left\"}\n"
              "{\"cmd\":\"tracker.set_map\",\"map\":\"dungeon\",\"tab\":\"dungeon-tab\"}\n"
              "{\"cmd\":\"tracker.click_pin\",\"location_id\":1002,\"button\":\"left\"}\n"
              "{\"cmd\":\"tracker.click_pin\",\"location\":\"loc.village.2\",\"button\":\"right\"}\n"
              "{\"cmd\":\"tracker.click_pin\",\"location\":\"loc.village.2\",\"button\":\"left\"}\n");
    runtime.PollCommands();
    if (!runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto snapshot = Slurp(root / "tracker.snapshot.json");
    if (!Contains(snapshot, "\"schema\":\"sekailink.tracker.snapshot.v1\"") ||
        !Contains(snapshot, "\"game\":\"Demo World\"") ||
        !Contains(snapshot, "\"assets_root\":\"") ||
        !Contains(snapshot, "\"slot\":\"Demo Slot\"") ||
        !Contains(snapshot, "\"seed\":\"Seed-001\"") ||
	        !Contains(snapshot, "\"active_map\":\"dungeon\"") ||
	        !Contains(snapshot, "\"active_tab\":\"dungeon-tab\"") ||
	        !Contains(snapshot, "\"auto_follow_map\":false") ||
	        !Contains(snapshot, "\"chat_messages\":[{\"id\":7,\"author\":\"Jade\",\"text\":\"hello sync\"}]") ||
	        !Contains(snapshot, "\"slot_data\":{\"goal\":\"demo\",\"difficulty\":\"normal\"}") ||
        !Contains(snapshot, "\"tracker_pack\":\"demo-pack\"") ||
        !Contains(snapshot, "\"maps\":[{\"id\":\"world\",\"label\":\"World Map\",\"image\":\"\",\"art_origin\":\"\"},{\"id\":\"dungeon\",\"label\":\"Dungeon Map\",\"image\":\"\",\"art_origin\":\"\"}]") ||
        !Contains(snapshot, "\"pack_maps\":[{\"name\":\"overworld\",\"image\":\"maps/overworld.png\"}]") ||
        !Contains(snapshot, "\"item_icon_groups\":[{\"group_id\":\"combat\",\"default_palette\":\"red-gold\"},{\"group_id\":\"resources\",\"default_palette\":\"amber\"}]") ||
        !Contains(snapshot, "\"id\":\"sword\"") ||
        !Contains(snapshot, "\"icon_key\":\"item.sword\"") ||
        !Contains(snapshot, "\"render_hint\":\"progressive-stage\"") ||
        !Contains(snapshot, "\"asset_candidates\":[\"poptracker-adapted/images/items/Sword.png\"]") ||
        !Contains(snapshot, "\"stage\":2") ||
        !Contains(snapshot, "\"id\":\"bombs\"") ||
        !Contains(snapshot, "\"icon_key\":\"item.bombs\"") ||
        !Contains(snapshot, "\"count\":1") ||
        !Contains(snapshot, "\"id\":\"village\"") ||
        !Contains(snapshot, "\"remaining_count\":0") ||
        !Contains(snapshot, "\"color\":\"black\"") ||
        !Contains(snapshot, "\"pins_detailed\":[") ||
        !Contains(snapshot, "\"location_id\":1001") ||
        !Contains(snapshot, "\"pack_map\":\"overworld\"") ||
        !Contains(snapshot, "\"map_asset\":\"poptracker-adapted/maps/overworld.png\"") ||
        !Contains(snapshot, "\"logic_ready\":true") ||
        !Contains(snapshot, "\"pack\":\"demo-pack\"") ||
        !Contains(snapshot, "\"variant\":\"Demo Variant\"")) {
        std::cerr << "tracker_snapshot_content_invalid\n";
        return EXIT_FAILURE;
    }
    const auto logic_runs_after_first_publish = Slurp(logic_runs_path);
    if (logic_runs_after_first_publish == logic_runs_after_autotab_publish ||
        !Contains(Slurp(root / "tracker.logic.log"), "logic-noise-marker")) {
        std::cerr << "tracker_logic_runner_log_invalid\n";
        return EXIT_FAILURE;
    }
    if (!runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_republish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    if (Slurp(logic_runs_path) != logic_runs_after_first_publish) {
        std::cerr << "tracker_logic_runner_not_cached\n";
        return EXIT_FAILURE;
    }
    AppendText(root / "tracker.commands.jsonl",
               "{\"cmd\":\"tracker.click_item\",\"code\":\"sword\",\"button\":\"right\"}\n");
    if (!runtime.PollCommands()) {
        std::cerr << "tracker_fast_click_not_detected\n";
        return EXIT_FAILURE;
    }
    if (!runtime.PublishSnapshotFastIfChanged(&error)) {
        std::cerr << "tracker_fast_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    if (Slurp(logic_runs_path) != logic_runs_after_first_publish) {
        std::cerr << "tracker_fast_publish_ran_logic\n";
        return EXIT_FAILURE;
    }
    const auto fast_snapshot = Slurp(root / "tracker.snapshot.json");
    if (!ContainsOrdered(fast_snapshot, {"\"id\":\"sword\"", "\"stage\":1"})) {
        std::cerr << "tracker_fast_click_snapshot_invalid\n";
        return EXIT_FAILURE;
    }
    AppendText(root / "tracker.commands.jsonl",
               "{\"cmd\":\"tracker.click_item\",\"code\":\"sword\",\"button\":\"left\"}\n");
    if (!runtime.PollCommands() || !runtime.PublishSnapshotFastIfChanged(&error)) {
        std::cerr << "tracker_fast_click_restore_failed\n";
        return EXIT_FAILURE;
    }
    AppendText(root / "tracker.commands.jsonl",
               "{\"cmd\":\"tracker.set_auto_follow\",\"enabled\":true}\n");
    if (!runtime.PollCommands() || !runtime.PublishSnapshotFastIfChanged(&error)) {
        std::cerr << "tracker_auto_follow_restore_failed\n";
        return EXIT_FAILURE;
    }
    const auto auto_follow_snapshot = Slurp(root / "tracker.snapshot.json");
    if (!Contains(auto_follow_snapshot, "\"active_map\":\"world\"") ||
        !Contains(auto_follow_snapshot, "\"active_tab\":\"world-tab\"") ||
        !Contains(auto_follow_snapshot, "\"auto_follow_map\":true")) {
        std::cerr << "tracker_auto_follow_restore_snapshot_invalid\n";
        return EXIT_FAILURE;
    }

#ifndef _WIN32
    const auto zip_path = root / "bundle.zip";
    const auto zip_command =
        "cd " + ShellQuote(root.string()) + " && zip -qr " + ShellQuote(zip_path.filename().string()) + " bundle";
    if (std::system(zip_command.c_str()) != 0 || !fs::exists(zip_path)) {
        std::cerr << "tracker_zip_fixture_failed\n";
        return EXIT_FAILURE;
    }
    const auto zip_run_root = root / "zip-run";
    fs::create_directories(zip_run_root);
    TrackerHeadlessRuntime zipped_runtime;
    if (!zipped_runtime.Initialize(
            {.bundle_path = zip_path,
             .snapshot_path = zip_run_root / "tracker.snapshot.json",
             .command_log_path = zip_run_root / "tracker.commands.jsonl",
             .room_state_path = root / "room.state",
             .tracker_variant = "Demo Variant"},
            &error)) {
        std::cerr << "tracker_zip_init_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    zipped_runtime.ApplyEvent({EventType::item_received, "ap:0:1:1001:1", "Progressive Sword", "", "", "", 1});
    zipped_runtime.ApplyEvent({EventType::item_received, "ap:1:1:1002:2", "Progressive Sword", "", "", "", 1});
    if (!zipped_runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_zip_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    const auto zipped_snapshot = Slurp(zip_run_root / "tracker.snapshot.json");
    if (!Contains(zipped_snapshot, "\"game\":\"Demo World\"") ||
        !Contains(zipped_snapshot, "\"assets_root\":\"") ||
        !Contains(zipped_snapshot, "tracker.pack.extracted") ||
        !Contains(zipped_snapshot, "\"id\":\"sword\"") ||
        !Contains(zipped_snapshot, "\"stage\":2")) {
        std::cerr << "tracker_zip_snapshot_invalid\n";
        return EXIT_FAILURE;
    }
#endif

    const auto autosave = Slurp(root / "tracker.autosave.state");
    if (!Contains(autosave, "meta|slot_id|Demo Slot\n") ||
        !Contains(autosave, "meta|seed_id|Seed-001\n") ||
        !Contains(autosave, "item|sword|1|2|0\n") ||
        !Contains(autosave, "item|bombs|1|0|1\n") ||
        !Contains(autosave, "check|1001|1\n") ||
        !Contains(autosave, "check|1002|1\n")) {
        std::cerr << "tracker_autosave_content_invalid\n";
        return EXIT_FAILURE;
    }

    TrackerHeadlessRuntime restored_runtime;
    if (!restored_runtime.Initialize(
            {.bundle_path = root / "bundle",
             .snapshot_path = root / "tracker.restored.snapshot.json",
             .command_log_path = root / "tracker.commands.jsonl",
             .room_state_path = root / "room.state",
             .tracker_variant = "Demo Variant"},
            &error)) {
        std::cerr << "tracker_restore_init_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    restored_runtime.PollCommands();
    if (!restored_runtime.PublishSnapshotIfChanged(&error)) {
        std::cerr << "tracker_restore_publish_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    const auto restored_snapshot = Slurp(root / "tracker.restored.snapshot.json");
    if (!Contains(restored_snapshot, "\"id\":\"sword\"") ||
        !Contains(restored_snapshot, "\"stage\":2") ||
        !Contains(restored_snapshot, "\"id\":\"bombs\"") ||
        !Contains(restored_snapshot, "\"count\":1") ||
        !Contains(restored_snapshot, "\"location_id\":1001") ||
        !Contains(restored_snapshot, "\"location_id\":1002")) {
        std::cerr << "tracker_autosave_restore_invalid\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_tracker_headless_smoke_ok\n";
    return EXIT_SUCCESS;
}
