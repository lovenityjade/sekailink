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

bool CanvasContainsPixel(const sekaiemu::spike::OverlayCanvas& canvas,
                         std::uint8_t red,
                         std::uint8_t green,
                         std::uint8_t blue) {
  const auto* data = canvas.Data();
  const auto pixel_count = static_cast<std::size_t>(canvas.Width()) * canvas.Height() * 4u;
  for (std::size_t index = 0; index < pixel_count; index += 4) {
    if (data[index + 0] == red && data[index + 1] == green && data[index + 2] == blue &&
        data[index + 3] != 0) {
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
    std::cerr << "tracker_alttp_bundle_smoke_missing_root\n";
    return 1;
  }

  const fs::path bundle_path = fs::path(argv[1]) / "tracker-bundles" / "alttp-default";
  if (!fs::exists(bundle_path / "manifest.json")) {
    std::cerr << "tracker_alttp_bundle_manifest_missing\n";
    return 1;
  }

  TrackerRuntime runtime;
  runtime.LoadBundle(TrackerBundle::LoadFromPath(bundle_path));
  runtime.ApplyServerSnapshot({
      {"world_instance_id", "alttp-world-1"},
      {"linkedworld_id", "alttp"},
      {"slot_id", "slot-1"},
      {"slot_name", "Link"},
      {"player_alias", "Jade"},
      {"room_id", "private-alttp-room"},
      {"seed_id", "seed-alttp-001"},
      {"seed_metadata",
       {
           {"seed_hash", "ABCD"},
           {"tracker_pack", "alttp-default"},
           {"tracker_variant", "side-by-side"},
           {"slot_data",
            {
                {"mode", "open"},
                {"goal", "ganon"},
                {"difficulty", "normal"},
                {"weapons", "randomized"},
            }},
       }},
      {"checked_locations", nlohmann::json::array({59836, 60025, 60028})},
      {"missing_locations", nlohmann::json::array({60030, 60031, 60032, 60033})},
      {"received_items", nlohmann::json::array({1001, 1002})},
  });
  runtime.SetActiveTab("desert");
  runtime.SetManualMap("desert_palace");
  runtime.SetAutoMapFollow(false);
  runtime.ApplySklmiEvent({
      {"event_type", "location_checked"},
      {"location_id", 60025},
      {"label", "Sanctuary"},
      {"timestamp", "00:03:12"},
  });
  runtime.ApplySklmiEvent({
      {"event_type", "item_received"},
      {"item_id", 1002},
      {"label", "Hookshot"},
      {"timestamp", "00:03:20"},
  });

  const auto resolved = runtime.ResolvedViewState();
  if (resolved.active_map_id != "desert_palace") {
    std::cerr << "tracker_alttp_bundle_zone_map_failed\n";
    return 1;
  }
  if (resolved.info_panels.size() < 4) {
    std::cerr << "tracker_alttp_bundle_info_panels_missing\n";
    return 1;
  }
  if (resolved.recent_events.size() != 2) {
    std::cerr << "tracker_alttp_bundle_recent_events_missing\n";
    return 1;
  }

  const auto* session_panel = FindPanel(resolved.info_panels, "session");
  const auto* tracker_panel = FindPanel(resolved.info_panels, "tracker");
  const auto* settings_panel = FindPanel(resolved.info_panels, "settings");
  const auto* progress_panel = FindPanel(resolved.info_panels, "progress");
  if (session_panel == nullptr || tracker_panel == nullptr || settings_panel == nullptr ||
      progress_panel == nullptr) {
    std::cerr << "tracker_alttp_bundle_required_panel_missing\n";
    return 1;
  }

  const auto* slot_field = FindField(*session_panel, "slot");
  const auto* slot_name_field = FindField(*session_panel, "slot_name");
  const auto* alias_field = FindField(*session_panel, "player_alias");
  const auto* seed_field = FindField(*session_panel, "seed");
  const auto* world_field = FindField(*session_panel, "world");
  const auto* room_field = FindField(*session_panel, "room_id");
  const auto* map_field = FindField(*tracker_panel, "active_map");
  const auto* mode_field = FindField(*settings_panel, "mode");
  const auto* goal_field = FindField(*settings_panel, "goal");
  const auto* slot_data_name_field = FindField(*settings_panel, "slot_data_name");
  const auto* received_field = FindField(*progress_panel, "received");
  if (slot_field == nullptr || slot_name_field == nullptr || alias_field == nullptr ||
      seed_field == nullptr || world_field == nullptr || room_field == nullptr ||
      slot_data_name_field == nullptr || received_field == nullptr ||
      map_field == nullptr || mode_field == nullptr || goal_field == nullptr) {
    std::cerr << "tracker_alttp_bundle_required_field_missing\n";
    return 1;
  }
  if (slot_field->value("value", std::string{}) != "slot-1" ||
      slot_name_field->value("value", std::string{}) != "Link" ||
      alias_field->value("value", std::string{}) != "Jade" ||
      seed_field->value("value", std::string{}) != "seed-alttp-001" ||
      world_field->value("value", std::string{}) != "alttp-world-1" ||
      room_field->value("value", std::string{}) != "private-alttp-room" ||
      map_field->value("value", std::string{}) != "desert_palace" ||
      mode_field->value("value", std::string{}) != "open" ||
      goal_field->value("value", std::string{}) != "ganon" ||
      received_field->value("value", std::string{}) != "2" ||
      slot_data_name_field->value("value", std::string{}) != "UNKNOWN") {
    std::cerr << "tracker_alttp_bundle_panel_values_failed\n";
    return 1;
  }

  bool saw_desert_tab = false;
  bool saw_items_tab = false;
  for (const auto& tab : resolved.visible_tabs) {
    if (!tab.is_object()) {
      continue;
    }
    const auto tab_id = tab.value("id", std::string{});
    if (tab_id == "desert") {
      saw_desert_tab = true;
    }
    if (tab_id == "items") {
      saw_items_tab = true;
    }
  }
  if (!saw_desert_tab || !saw_items_tab) {
    std::cerr << "tracker_alttp_bundle_visible_tabs_missing\n";
    return 1;
  }

  const auto* bundle = runtime.Bundle();
  const auto* active_map = bundle == nullptr ? nullptr : bundle->FindMap(resolved.active_map_id);
  if (active_map == nullptr || !active_map->raster_image.has_value()) {
    std::cerr << "tracker_alttp_bundle_raster_missing\n";
    return 1;
  }

  OverlayCanvas canvas(960, 540);
  canvas.Clear({0, 0, 0, 0});
  RenderTrackerPanel(canvas,
                     runtime,
                     resolved,
                     TrackerPanelLayout{560, 0, 400, 540},
                     false,
                     "ALTTP");

  if (!CanvasContainsPixel(canvas, 156, 121, 82)) {
    std::cerr << "tracker_alttp_bundle_map_pixels_missing\n";
    return 1;
  }

  std::cout << "tracker_alttp_bundle_smoke_ok\n";
  return 0;
}
