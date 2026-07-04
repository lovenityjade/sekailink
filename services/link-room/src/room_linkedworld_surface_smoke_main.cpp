#include "sekailink_server/room_server_protocol.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

nlohmann::json linkedworld_payload() {
  return {
      {"manifest",
       {
           {"module_id", "sekailink-linkedworld-alttp"},
           {"game_id", "alttp"},
           {"family_system_id", "snes"},
           {"display_name", "A Link to the Past"},
           {"version", "0.1.1-dev"},
           {"runtime_requirements",
            {
                {"host", "sekaiemu"},
                {"memory_interface", "sklmi"},
                {"system", "snes"},
                {"runner", "snes"},
            }},
           {"room_surface",
            {
                {"bridge_ref", "bridge/sklmi.phase1.json"},
                {"required_runtime_metadata",
                 {"seed_id", "seed_hash", "slot_name", "player_alias", "tracker_pack", "tracker_variant", "slot_data"}},
            }},
           {"module_blocks",
            {
                {"metadata", {{"path", "metadata/module.json"}}},
                {"patch", {{"path", "patch/patch.manifest.json"}}},
                {"preset_default", {{"path", "presets/alttp.ap-defaults.core.json"}}},
            }},
       }},
      {"room_metadata",
       {
           {"contract_id", "alttp-room-metadata-complete-v1"},
           {"linkedworld_id", "alttp"},
           {"required_keys",
            {{{"key", "seed_id"}, {"label", "Seed"}},
             {{"key", "seed_hash"}, {"label", "Seed Hash"}},
             {{"key", "slot_name"}, {"label", "Slot Name"}},
             {{"key", "tracker_pack"}, {"label", "Tracker Pack"}},
             {{"key", "tracker_variant"}, {"label", "Tracker Variant"}},
             {{"key", "slot_data"}, {"label", "Slot Data"}}}},
           {"recommended_optional_keys",
            {{{"key", "goal"}, {"label", "Goal"}}, {{"key", "mode"}, {"label", "Mode"}}}},
       }},
      {"slot_data_contract",
       {
           {"contract_id", "alttp-slot-data-complete-v1"},
           {"panels",
            {{{"panel_id", "settings_core"},
              {"fields",
               {{{"slot_data_key", "goal"}, {"label", "Goal"}},
                {{"slot_data_key", "mode"}, {"label", "Mode"}},
                {{"slot_data_key", "entrance_shuffle"}, {"label", "Entrance Shuffle"}}}}}}},
       }},
      {"preset",
       {
           {"preset_id", "alttp.runtime-complete.side-by-side"},
           {"linkedworld_id", "alttp"},
           {"runtime",
            {
                {"presentation_mode", "side-by-side"},
                {"left_panel", "game"},
                {"right_panel", "tracker"},
                {"tracker_variant", "Map Tracker - AP"},
                {"tracker_pack_path", "tracker/default.zip"},
                {"room_metadata_ref", "metadata/room-metadata.complete.json"},
                {"slot_data_ref", "metadata/slot-data.complete.json"},
                {"tracker_focus", {"items", "seed_settings"}},
            }},
           {"room_metadata",
            {
                {"slot_data_recommended_visible_fields", {"goal", "mode", "dark_room_logic"}},
            }},
       }},
      {"bridge",
       {
           {"id", "alttp"},
           {"type", "linkedworld"},
           {"sklmi",
            {
                {"bridge_id", "alttp-runtime-core"},
                {"driver_instance_id", "alttp-runtime-core-default"},
                {"core_profile", "snes_v1"},
                {"checks",
                 {{{"event_type", "location_checked"},
                   {"location_id", 60001},
                   {"location_name", "Link's House"},
                   {"event_key", "0xE9F1"},
                   {"mapped_value", "Link's House"}}}},
                {"actions",
                 {{{"event_type", "room_item_received"},
                   {"room_controlled", true},
                   {"item_id", 10},
                   {"item_name", "Hookshot"},
                   {"event_key", "item.hookshot"},
                   {"mapped_value", "Hookshot"}}}},
            }},
       }},
  };
}

nlohmann::json soh_linkedworld_payload() {
  return {
      {"manifest",
       {
           {"module_id", "sekailink-linkedworld-soh"},
           {"game_id", "oot_soh"},
           {"family_system_id", "pc"},
           {"display_name", "Ocarina of Time - Ship of Harkinian"},
           {"version", "0.1.0-dev"},
           {"runtime_requirements",
            {
                {"host", "installed_runtime"},
                {"memory_interface", "server_dispatch"},
                {"system", "pc"},
                {"runner", "soh"},
            }},
           {"room_surface",
            {
                {"required_runtime_metadata",
                 {"seed_id", "slot_name", "slot_data", "linkedworld_id"}},
            }},
           {"module_blocks",
            {
                {"metadata", {{"path", "metadata/module.json"}}},
                {"patch", {{"path", "patch/patch.manifest.json"}}},
                {"preset_default", {{"path", "presets/presets.manifest.json"}}},
            }},
       }},
      {"patch",
       {
           {"declarative_contract",
            {
                {"patch_type", "server_dispatch"},
                {"patch_mode", "server_dispatch"},
                {"server_dispatch",
                 {
                     {"enabled", true},
                     {"target", "link_room"},
                     {"transport", "room_contract"},
                     {"payload_ref", "link_room_seed_contract.json"},
                     {"ack_required", true},
                 }},
            }},
       }},
      {"room_metadata",
       {
           {"contract_id", "soh-room-metadata-v1"},
           {"linkedworld_id", "oot_soh"},
           {"required_keys",
            {{{"key", "seed_id"}, {"label", "Seed"}},
             {{"key", "slot_name"}, {"label", "Slot Name"}},
             {{"key", "slot_data"}, {"label", "Slot Data"}}}},
       }},
      {"slot_data_contract",
       {
           {"contract_id", "soh-slot-data-v1"},
           {"panels",
            {{{"panel_id", "settings_core"},
              {"fields",
               {{{"slot_data_key", "goal"}, {"label", "Goal"}},
                {{"slot_data_key", "logic_rules"}, {"label", "Logic Rules"}}}}}}},
       }},
      {"preset",
       {
           {"preset_id", "soh.server-first-proof.v1"},
           {"linkedworld_id", "oot_soh"},
           {"runtime",
            {
                {"presentation_mode", "room-focused"},
                {"left_panel", "room"},
                {"right_panel", "session"},
                {"room_metadata_ref", "metadata/room-metadata.minimal.json"},
                {"slot_data_ref", "metadata/slot-data.minimal.json"},
                {"tracker_focus", nlohmann::json::array()},
            }},
           {"room_metadata",
            {
                {"slot_data_recommended_visible_fields", {"goal", "logic_rules"}},
            }},
       }},
  };
}

nlohmann::json soh_seed_contract() {
  const nlohmann::json config_snapshot = {
      {"schema_version", "sekailink-config-snapshot-v1"},
      {"source", "Jade-SoH.yaml"},
      {"game_key", "oot_soh"},
      {"source_game_key", "ship_of_harkinian"},
      {"linkedworld_id", "oot_soh"},
      {"config_name", "Jade-SoH"},
      {"values",
       {
           {"starting_age", "child"},
           {"rainbow_bridge", "greg"},
           {"shuffle_songs", "anywhere"},
           {"skip_child_zelda", "true"},
       }},
  };
  return {
      {"schema_version", "sekailink-link-room-seed-contract-v1"},
      {"generation_scope", "multiworld"},
      {"room_id", "room-seed-soh-cycle1"},
      {"seed_id", "seed-soh-cycle1"},
      {"checks_ref", "checks.json"},
      {"items_ref", "items.json"},
      {"placements_ref", "placements.json"},
      {"slots",
       nlohmann::json::array({
           {
               {"slot_id", 2},
               {"user_id", 1001},
               {"display_name", "Jade SoH"},
               {"game_key", "oot_soh"},
               {"linkedworld_id", "oot_soh"},
               {"config_version_id", 7001},
               {"config_snapshot", config_snapshot},
           },
       })},
      {"config_versions",
       nlohmann::json::array({
           {
               {"slot_id", 2},
               {"user_id", 1001},
               {"game_key", "oot_soh"},
               {"linkedworld_id", "oot_soh"},
               {"config_version_id", 7001},
               {"source", "nexus.config_version_id"},
               {"config_snapshot", config_snapshot},
           },
       })},
  };
}

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_linkedworld_surface_smoke";
    std::filesystem::remove_all(root);

    RoomRegistry registry;
    RoomAuditStore audit_store(root);
    RoomProjectionSpool projection_spool(root / "projection");

    auto response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-linkedworld-1"},
                 {"room_type", "live"},
                 {"game", "A Link to the Past"},
                 {"slot_id", 1},
                 {"slot_name", "Jade"},
                 {"slot_alias", "Sekai Jade"},
                 {"seed_id", "seed-linkedworld-1"},
                 {"seed_hash", "hash-linkedworld-1"},
             }},
        });
    require(response["ok"] == true, "create_room");

    auto apply_command = linkedworld_payload();
    apply_command["cmd"] = "apply_linkedworld_surface";
    apply_command["room_id"] = "room-linkedworld-1";
    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {{"channel", "admin"}, {"command", apply_command}});
    require(response["ok"] == true, "apply_linkedworld_surface");
    require(response["linkedworld_surface"]["linkedworld_id"] == "alttp", "surface_linkedworld_id");
    require(response["linkedworld_surface"]["bridge"]["check_count"] == 1, "surface_check_count");
    require(response["linkedworld_surface"]["item_semantics"].size() == 1, "surface_item_semantics");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "set_slot_data"},
                 {"room_id", "room-linkedworld-1"},
                 {"slot_data", {{"goal", "ganon"}, {"mode", "open"}, {"dark_room_logic", "lamp"}}},
             }},
        });
    require(response["ok"] == true, "set_slot_data");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "enqueue_received_item"},
                 {"room_id", "room-linkedworld-1"},
                 {"item",
                  {
                      {"item_id", 1001},
                      {"item_name", "Hookshot"},
                      {"location_id", 60001},
                      {"sender_slot", 2},
                      {"sender_alias", "Zelda"},
                  }},
             }},
        });
    require(response["ok"] == true, "enqueue_received_item");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "issue_ticket"},
                 {"session_name", "room-linkedworld-1"},
                 {"slot_id", 1},
                 {"client_kind", "runtime"},
                 {"driver_instance_id", "sklmi-driver-linkedworld"},
                 {"linkedworld_id", "alttp"},
                 {"core_profile", "snes_v1"},
             }},
        });
    require(response["ok"] == true, "issue_ticket");
    require(response["slot_data"]["goal"] == "ganon", "issue_ticket_slot_data");
    require(response["linkedworld_surface"]["linkedworld_id"] == "alttp", "issue_ticket_surface");
    const auto session_token = response["session_token"].get<std::string>();

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "pending_items"},
                 {"session_name", "room-linkedworld-1"},
                 {"slot_id", 1},
                 {"session_token", session_token},
             }},
        });
    require(response["ok"] == true, "pending_items");
    require(response["pending_items"].size() == 1, "pending_items_size");
    require(response["pending_items"].at(0).at("event_key") == "item.hookshot", "pending_event_key");
    require(response["pending_items"].at(0).at("mapped_value") == "Hookshot", "pending_mapped_value");
    require(response["pending_items"].at(0).at("tracker_semantic_id") == "item.hookshot", "pending_tracker_semantic");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "runtime_event"},
                 {"session_name", "room-linkedworld-1"},
                 {"slot_id", 1},
                 {"session_token", session_token},
                 {"event_type", "location_checked"},
                 {"canonical_id", 60001},
             }},
        });
    require(response["ok"] == true, "runtime_event");
    require(response["recorded"] == true, "runtime_event_recorded");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "acknowledge_delivery"},
                 {"session_name", "room-linkedworld-1"},
                 {"slot_id", 1},
                 {"delivery_id", 0},
                 {"session_token", session_token},
             }},
        });
    require(response["ok"] == true, "acknowledge_delivery");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command", {{"cmd", "snapshot_room"}, {"room_id", "room-linkedworld-1"}}},
        });
    require(response["ok"] == true, "snapshot_room");
    require(response["snapshot"]["linkedworld_surface"]["presentation"]["mode"] == "side-by-side", "snapshot_presentation");
    require(response["snapshot"]["received_items"].at(0).at("event_key") == "item.hookshot", "snapshot_item_semantic");

    const auto room_records = projection_spool.read_room_records();
    const auto room_events = projection_spool.read_room_event_records();
    require(!room_records.empty(), "projection_room_records");
    require(room_records.back()["linkedworld_id"] == "alttp", "projection_linkedworld_id");
    require(room_records.back()["linkedworld_item_semantic_count"] == 1, "projection_item_semantic_count");
    require(!room_events.empty(), "projection_room_events");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-linkedworld-soh-1"},
                 {"room_type", "live"},
                 {"game", "Ship of Harkinian"},
                 {"slot_id", 2},
                 {"slot_name", "Jade SoH"},
                 {"seed_id", "seed-soh-1"},
             }},
        });
    require(response["ok"] == true, "create_room_soh");

    auto soh_apply_command = soh_linkedworld_payload();
    soh_apply_command["cmd"] = "apply_linkedworld_surface";
    soh_apply_command["room_id"] = "room-linkedworld-soh-1";
    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {{"channel", "admin"}, {"command", soh_apply_command}});
    require(response["ok"] == true, "apply_linkedworld_surface_soh");
    require(response["linkedworld_surface"]["linkedworld_id"] == "oot_soh", "soh_surface_linkedworld_id");
    require(response["linkedworld_surface"]["delivery"]["patch_mode"] == "server_dispatch", "soh_delivery_patch_mode");
    require(response["linkedworld_surface"]["delivery"]["server_dispatch_enabled"] == true,
            "soh_delivery_dispatch_enabled");
    require(response["linkedworld_surface"]["delivery"]["server_dispatch_target"] == "link_room",
            "soh_delivery_dispatch_target");
    require(response["linkedworld_surface"]["runtime_memory_interface"] == "server_dispatch",
            "soh_runtime_memory_interface");
    require(response["linkedworld_surface"]["tracker_pack"].is_null(), "soh_tracker_pack_null");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "apply_seed_contract"},
                 {"room_id", "room-linkedworld-soh-1"},
                 {"seed_contract", soh_seed_contract()},
             }},
        });
    require(response["ok"] == true, "apply_seed_contract_soh");
    require(response["seed_contract_summary"]["applied"] == true, "seed_contract_applied_summary");
    require(response["seed_contract_summary"]["seed_id"] == "seed-soh-cycle1", "seed_contract_seed_id");
    require(response["seed_contract_summary"]["slot_count"] == 1, "seed_contract_slot_count");
    require(response["slot_data"]["starting_age"] == "child", "seed_contract_slot_data_starting_age");
    require(response["slot_data"]["rainbow_bridge"] == "greg", "seed_contract_slot_data_rainbow_bridge");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "issue_ticket"},
                 {"session_name", "room-linkedworld-soh-1"},
                 {"slot_id", 2},
                 {"client_kind", "runtime"},
                 {"linkedworld_id", "oot_soh"},
             }},
        });
    require(response["ok"] == true, "issue_ticket_soh");
    require(response["slot_data"]["shuffle_songs"] == "anywhere", "issue_ticket_soh_slot_data");
    require(response["seed_contract_summary"]["seed_id"] == "seed-soh-cycle1", "issue_ticket_soh_seed_contract");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "sync_room"},
                 {"room_id", "room-linkedworld-soh-1"},
             }},
        });
    require(response["ok"] == true, "sync_room_soh");
    require(response["sync"]["room"]["seed_contract_summary"]["applied"] == true,
            "sync_room_soh_seed_contract_summary");
    require(response["sync"]["room"]["slot_data"]["skip_child_zelda"] == "true",
            "sync_room_soh_seed_slot_data");

    const auto soh_room_records = projection_spool.read_room_records();
    require(!soh_room_records.empty(), "soh_projection_room_records");
    require(soh_room_records.back()["seed_contract_applied"] == true,
            "soh_projection_seed_contract_applied");
    require(soh_room_records.back()["payload"]["seed_contract"]["seed_id"] == "seed-soh-cycle1",
            "soh_projection_seed_contract_payload");

    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_linkedworld_surface_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
