#include "overlay_canvas.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <filesystem>
#include <iostream>

namespace {

const nlohmann::json* FindPanel(const std::vector<nlohmann::json>& panels, std::string_view id) {
  for (const auto& panel : panels) {
    if (!panel.is_object()) {
      continue;
    }
    if (panel.value("id", std::string{}) == id) {
      return &panel;
    }
  }
  return nullptr;
}

const nlohmann::json* FindField(const nlohmann::json& panel, std::string_view id) {
  if (!panel.is_object()) {
    return nullptr;
  }
  const auto fields_it = panel.find("fields");
  if (fields_it == panel.end() || !fields_it->is_array()) {
    return nullptr;
  }
  for (const auto& field : *fields_it) {
    if (!field.is_object()) {
      continue;
    }
    if (field.value("id", std::string{}) == id) {
      return &field;
    }
  }
  return nullptr;
}

bool CanvasHasVisiblePixels(const sekaiemu::spike::OverlayCanvas& canvas) {
  const auto* data = canvas.Data();
  const auto pixel_count = static_cast<std::size_t>(canvas.Width()) * canvas.Height() * 4u;
  for (std::size_t index = 3; index < pixel_count; index += 4) {
    if (data[index] != 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  using namespace sekaiemu::spike;

  if (argc < 2) {
    std::cerr << "tracker_alttp_native_bundle_smoke_missing_root\n";
    return 1;
  }

  const fs::path bundle_path = fs::path(argv[1]) / "tracker-bundles" / "alttp-native";
  if (!fs::exists(bundle_path / "manifest.json")) {
    std::cerr << "tracker_alttp_native_bundle_manifest_missing\n";
    return 1;
  }

  TrackerRuntime runtime;
  runtime.LoadBundle(TrackerBundle::LoadFromPath(bundle_path));
  runtime.ApplyServerSnapshot({
      {"world_instance_id", "alttp-world-live"},
      {"linkedworld_id", "alttp"},
      {"slot_id", "slot-3"},
      {"slot_name", "Link"},
      {"player_alias", "Jade"},
      {"room_id", "private-live-room"},
      {"room_name", "Internal ALTTP Live"},
      {"room_type", "private-room"},
      {"session_id", "session-live-001"},
      {"seed_id", "seed-live-001"},
      {"room_metadata",
       {
           {"driver_instance_id", "driver-link-01"},
           {"core_profile", "bsnes-mercury-performance"},
       }},
      {"seed_metadata",
       {
           {"seed_hash", "EFGH"},
           {"tracker_pack", "alttp-native"},
           {"tracker_variant", "side-by-side"},
           {"slot_data",
            {
                {"mode", "open"},
                {"goal", "fast_ganon"},
                {"difficulty", "normal"},
                {"weapons", "randomized"},
                {"entrance_shuffle", "none"},
                {"item_pool", "normal"},
                {"item_functionality", "normal"},
                {"dark_room_logic", "lamp"},
                {"dungeon_counters", "pickup"},
                {"open_pyramid", "goal"},
                {"name", "internal-live"},
            }},
       }},
      {"checked_locations",
       nlohmann::json::array({
           nlohmann::json{{"location_id", 59836}, {"location_name", "Link's House"}},
           nlohmann::json{{"location_id", 60025}, {"location_name", "Sanctuary"}},
           nlohmann::json{{"location_id", 60028}, {"location_name", "Escape - Boomerang Chest"}},
           nlohmann::json{{"location_id", 60033}, {"location_name", "Sewers - Secret Room"}, {"timestamp", "00:03:12"}},
       })},
      {"missing_locations", nlohmann::json::array({60030, 60031})},
      {"received_items",
       nlohmann::json::array({
           nlohmann::json{{"item_id", 1001}, {"item_name", "Lamp"}, {"sender_alias", "Zelda"}},
           nlohmann::json{{"item_id", 1002}, {"item_name", "Hammer"}, {"sender_alias", "Sahasrahla"}},
           nlohmann::json{{"item_id", 1003}, {"item_name", "Hookshot"}, {"sender_alias", "Jade"}, {"timestamp", "00:03:05"}},
       })},
  });
  runtime.ApplyRuntimeContext({{"zone_id", "alttp.sewers"}});
  runtime.ApplySklmiEvent({
      {"event_type", "location_checked"},
      {"location_id", 60033},
      {"label", "Sewers - Secret Room"},
      {"timestamp", "00:03:12"},
  });

  const auto resolved = runtime.ResolvedViewState();
  if (resolved.active_map_id != "hyrule-sewers") {
    std::cerr << "tracker_alttp_native_bundle_zone_map_failed\n";
    return 1;
  }
  if (resolved.info_panels.size() < 11) {
    std::cerr << "tracker_alttp_native_bundle_info_panels_missing\n";
    return 1;
  }
  if (!resolved.info_panels[0].is_object() || resolved.info_panels[0].value("id", std::string{}) != "session" ||
      !resolved.info_panels[1].is_object() || resolved.info_panels[1].value("id", std::string{}) != "progress" ||
      !resolved.info_panels[2].is_object() || resolved.info_panels[2].value("id", std::string{}) != "live-feed" ||
      !resolved.info_panels[3].is_object() || resolved.info_panels[3].value("id", std::string{}) != "tracker-state") {
    std::cerr << "tracker_alttp_native_bundle_panel_order_failed\n";
    return 1;
  }

  const auto* session_panel = FindPanel(resolved.info_panels, "session");
  const auto* progress_panel = FindPanel(resolved.info_panels, "progress");
  const auto* live_panel = FindPanel(resolved.info_panels, "live-feed");
  const auto* tracker_panel = FindPanel(resolved.info_panels, "tracker-state");
  const auto* slot_info_panel = FindPanel(resolved.info_panels, "slot-info");
  const auto* seed_meta_panel = FindPanel(resolved.info_panels, "seed-meta");
  const auto* room_meta_panel = FindPanel(resolved.info_panels, "room-meta");
  const auto* runtime_meta_panel = FindPanel(resolved.info_panels, "runtime-meta");
  const auto* journal_meta_panel = FindPanel(resolved.info_panels, "journal-meta");
  const auto* settings_panel = FindPanel(resolved.info_panels, "settings-core");
  const auto* rules_panel = FindPanel(resolved.info_panels, "settings-rules");
  if (session_panel == nullptr || progress_panel == nullptr || live_panel == nullptr || tracker_panel == nullptr ||
      slot_info_panel == nullptr || seed_meta_panel == nullptr || room_meta_panel == nullptr ||
      runtime_meta_panel == nullptr || journal_meta_panel == nullptr || settings_panel == nullptr ||
      rules_panel == nullptr) {
    std::cerr << "tracker_alttp_native_bundle_required_panel_missing\n";
    return 1;
  }
  if (session_panel->value("surface", std::string{}) != "summary" ||
      slot_info_panel->value("surface", std::string{}) != "details" ||
      room_meta_panel->value("priority", -1) != 25 ||
      runtime_meta_panel->value("priority", -1) != 26 ||
      journal_meta_panel->value("priority", -1) != 27) {
    std::cerr << "tracker_alttp_native_bundle_panel_surface_priority_failed\n";
    return 1;
  }

  if (const auto* room_field = FindField(*session_panel, "room");
      room_field == nullptr || room_field->value("value", std::string{}) != "private-live-room") {
    std::cerr << "tracker_alttp_native_bundle_room_field_failed\n";
    return 1;
  }
  if (const auto* alias_field = FindField(*slot_info_panel, "player-alias");
      alias_field == nullptr || alias_field->value("value", std::string{}) != "Jade") {
    std::cerr << "tracker_alttp_native_bundle_alias_field_failed\n";
    return 1;
  }
  if (const auto* hash_field = FindField(*seed_meta_panel, "seed-hash");
      hash_field == nullptr || hash_field->value("value", std::string{}) != "EFGH") {
    std::cerr << "tracker_alttp_native_bundle_seed_meta_failed\n";
    return 1;
  }
  if (const auto* room_name_field = FindField(*room_meta_panel, "room-name");
      room_name_field == nullptr || room_name_field->value("value", std::string{}) != "Internal ALTTP Live") {
    std::cerr << "tracker_alttp_native_bundle_room_name_failed\n";
    return 1;
  }
  if (const auto* linkedworld_field = FindField(*room_meta_panel, "linkedworld");
      linkedworld_field == nullptr || linkedworld_field->value("value", std::string{}) != "alttp") {
    std::cerr << "tracker_alttp_native_bundle_linkedworld_field_failed\n";
    return 1;
  }
  if (const auto* clear_field = FindField(*progress_panel, "clear");
      clear_field == nullptr || clear_field->value("value", std::string{}) != "66%") {
    std::cerr << "tracker_alttp_native_bundle_progress_field_failed\n";
    return 1;
  }
  if (const auto* items_field = FindField(*progress_panel, "items");
      items_field == nullptr || items_field->value("value", std::string{}) != "3") {
    std::cerr << "tracker_alttp_native_bundle_items_field_failed\n";
    return 1;
  }
  if (const auto* last_item_field = FindField(*live_panel, "last-item");
      last_item_field == nullptr || last_item_field->value("value", std::string{}) != "Hookshot") {
    std::cerr << "tracker_alttp_native_bundle_live_item_failed\n";
    return 1;
  }
  if (const auto* from_field = FindField(*live_panel, "last-from");
      from_field == nullptr || from_field->value("value", std::string{}) != "Jade") {
    std::cerr << "tracker_alttp_native_bundle_live_sender_failed\n";
    return 1;
  }
  if (const auto* last_check_field = FindField(*live_panel, "last-check");
      last_check_field == nullptr || last_check_field->value("value", std::string{}) != "Sewers - Secret Room") {
    std::cerr << "tracker_alttp_native_bundle_live_check_failed\n";
    return 1;
  }
  if (const auto* event_count_field = FindField(*runtime_meta_panel, "event-count");
      event_count_field == nullptr || event_count_field->value("value", std::string{}) != "1") {
    std::cerr << "tracker_alttp_native_bundle_event_count_failed\n";
    return 1;
  }
  if (const auto* layout_field = FindField(*runtime_meta_panel, "layout");
      layout_field == nullptr || layout_field->value("value", std::string{}) != "side-by-side") {
    std::cerr << "tracker_alttp_native_bundle_layout_field_failed\n";
    return 1;
  }
  if (const auto* driver_field = FindField(*runtime_meta_panel, "driver-instance");
      driver_field == nullptr || driver_field->value("value", std::string{}) != "driver-link-01") {
    std::cerr << "tracker_alttp_native_bundle_driver_field_failed\n";
    return 1;
  }
  if (const auto* checks_field = FindField(*journal_meta_panel, "check-progress");
      checks_field == nullptr || checks_field->value("value", std::string{}) != "4/6") {
    std::cerr << "tracker_alttp_native_bundle_journal_progress_failed\n";
    return 1;
  }
  if (const auto* item_at_field = FindField(*journal_meta_panel, "last-item-at");
      item_at_field == nullptr || item_at_field->value("value", std::string{}) != "00:03:05") {
    std::cerr << "tracker_alttp_native_bundle_journal_item_time_failed\n";
    return 1;
  }
  if (const auto* check_at_field = FindField(*journal_meta_panel, "last-check-at");
      check_at_field == nullptr || check_at_field->value("value", std::string{}) != "00:03:12") {
    std::cerr << "tracker_alttp_native_bundle_journal_check_time_failed\n";
    return 1;
  }
  if (const auto* map_field = FindField(*tracker_panel, "active-map");
      map_field == nullptr || map_field->value("value", std::string{}) != "hyrule-sewers") {
    std::cerr << "tracker_alttp_native_bundle_tracker_state_failed\n";
    return 1;
  }
  if (const auto* mode_field = FindField(*settings_panel, "mode");
      mode_field == nullptr || mode_field->value("value", std::string{}) != "open") {
    std::cerr << "tracker_alttp_native_bundle_mode_field_failed\n";
    return 1;
  }
  if (const auto* difficulty_field = FindField(*settings_panel, "difficulty");
      difficulty_field == nullptr || difficulty_field->value("value", std::string{}) != "normal") {
    std::cerr << "tracker_alttp_native_bundle_difficulty_field_failed\n";
    return 1;
  }
  if (const auto* preset_field = FindField(*seed_meta_panel, "slot-data-name");
      preset_field == nullptr || preset_field->value("value", std::string{}) != "internal-live") {
    std::cerr << "tracker_alttp_native_bundle_preset_field_failed\n";
    return 1;
  }
  if (const auto* shuffle_field = FindField(*rules_panel, "shuffle");
      shuffle_field == nullptr || shuffle_field->value("value", std::string{}) != "none") {
    std::cerr << "tracker_alttp_native_bundle_shuffle_field_failed\n";
    return 1;
  }
  if (const auto* dark_room_field = FindField(*rules_panel, "dark-room");
      dark_room_field == nullptr || dark_room_field->value("value", std::string{}) != "lamp") {
    std::cerr << "tracker_alttp_native_bundle_rules_field_failed\n";
    return 1;
  }

  OverlayCanvas canvas(960, 540);
  canvas.Clear({0, 0, 0, 0});
  RenderTrackerPanel(canvas,
                     runtime,
                     resolved,
                     TrackerPanelLayout{560, 0, 400, 540},
                     false,
                     "ALTTP NATIVE");
  if (!CanvasHasVisiblePixels(canvas)) {
    std::cerr << "tracker_alttp_native_bundle_canvas_empty\n";
    return 1;
  }

  std::cout << "tracker_alttp_native_bundle_smoke_ok\n";
  return 0;
}
