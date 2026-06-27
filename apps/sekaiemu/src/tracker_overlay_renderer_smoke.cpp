#include "overlay_canvas.hpp"
#include "tracker_pack_layout_engine.hpp"
#include "tracker_pack_layout_renderer.hpp"
#include "tracker_overlay_render_state.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_overlay_style.hpp"
#include "tracker_runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

void WriteText(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
}

std::string MakeSolidPpm(int width,
                         int height,
                         int red,
                         int green,
                         int blue,
                         int center_red,
                         int center_green,
                         int center_blue) {
  std::ostringstream out;
  out << "P3\n" << width << ' ' << height << "\n255\n";
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const bool center = x >= width / 3 && x < (width * 2) / 3 &&
                          y >= height / 3 && y < (height * 2) / 3;
      out << (center ? center_red : red) << ' '
          << (center ? center_green : green) << ' '
          << (center ? center_blue : blue) << '\n';
    }
  }
  return out.str();
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

int main() {
  namespace fs = std::filesystem;
  using namespace sekaiemu::spike;
  using sekaiemu::spike::tracker_pack_layout_detail::ShouldSuppressPackLayoutNode;

  const auto root = fs::temp_directory_path() / "tracker-overlay-renderer-smoke";
  fs::remove_all(root);
  fs::create_directories(root / "bundle" / "maps");

  WriteText(root / "bundle" / "maps" / "overworld.ppm", MakeSolidPpm(12, 8, 12, 80, 128, 252, 220, 72));

  WriteText(root / "bundle" / "manifest.json", R"JSON({
    "tracker_contract_version": "1",
    "linkedworld_id": "earthbound-test",
    "display_name": "EarthBound Tracker",
    "default_view_id": "main",
    "default_tab_id": "inventory",
    "default_map_id": "overworld",
    "maps": [
      {"id": "overworld", "label": "Overworld", "image": "maps/overworld.ppm"}
    ],
    "tabs": [
      {"id": "inventory", "label": "Inventory", "view_id": "main", "map_id": "overworld"}
    ],
    "item_icon_metadata": [
      {"id": 301, "label": "Sound Stone", "abbr": "SS", "icon": "icons/sound-stone.png"},
      {"id": 302, "label": "Receiver Phone", "abbr": "RP"}
    ],
    "map_pin_metadata": [
      {"id": "gift", "location_id": 101, "label": "Gift", "map_id": "overworld", "x": 0.25, "y": 0.35},
      {"id": "shop", "location_id": 102, "label": "Shop", "map_id": "overworld", "x": 0.75, "y": 0.62}
    ],
    "info_panels": [
      {
        "id": "session",
        "label": "Session",
        "fields": [
          {"id": "slot", "label": "Slot", "source": "session", "key": "slot_id"},
          {"id": "seed", "label": "Seed", "source": "session", "key": "seed_id"}
        ]
      }
    ],
    "views": [
      {"id": "main", "label": "Main", "tabs": ["inventory"]}
    ]
  })JSON");

  TrackerRuntime runtime;
  runtime.LoadBundle(TrackerBundle::LoadFromDirectory(root / "bundle"));
  runtime.ApplyServerSnapshot({
      {"world_instance_id", "world-1"},
      {"linkedworld_id", "earthbound-test"},
      {"slot_id", "slot-1"},
      {"seed_id", "seed-1"},
      {"seed_metadata", {{"mode", "standard"}}},
      {"checked_locations", nlohmann::json::array({101})},
      {"missing_locations", nlohmann::json::array({102})},
      {"received_items", nlohmann::json::array({301})},
  });
  runtime.ApplyRuntimeContext({{"zone_id", "overworld"}});
  runtime.ApplyLocalToggle("boss_ready", true);
  runtime.ApplySklmiEvent({
      {"event_type", "item_received"},
      {"item_id", 301},
      {"label", "Sound Stone"},
      {"timestamp", "00:00:34"},
  });

  OverlayCanvas canvas(760, 360);
  canvas.Clear({0, 0, 0, 0});
  RenderTrackerPanel(canvas,
                     runtime,
                     runtime.ResolvedViewState(),
                     TrackerPanelLayout{400, 0, 360, 360},
                     false,
                     "SMOKE");

  if (!CanvasHasVisiblePixels(canvas)) {
    std::cerr << "tracker_overlay_renderer_canvas_empty\n";
    return 1;
  }
  if (!CanvasContainsPixel(canvas, 252, 220, 72)) {
    std::cerr << "tracker_overlay_renderer_map_pixels_missing\n";
    return 1;
  }
  if (!CanvasContainsPixel(canvas, 86, 214, 142)) {
    std::cerr << "tracker_overlay_renderer_received_item_badge_missing\n";
    return 1;
  }
  if (!CanvasContainsPixel(canvas, 48, 58, 72)) {
    std::cerr << "tracker_overlay_renderer_pending_item_badge_missing\n";
    return 1;
  }
  if (!CanvasContainsPixel(canvas, 255, 88, 88)) {
    std::cerr << "tracker_overlay_renderer_open_pin_missing\n";
    return 1;
  }

  TrackerRuntime snapshot_alias_runtime;
  snapshot_alias_runtime.LoadBundle(TrackerBundle::LoadFromDirectory(root / "bundle"));
  snapshot_alias_runtime.ApplyServerSnapshot({
      {"linkedworld_id", "earthbound-test"},
      {"slot", "1"},
      {"seed", "13680877444271862070"},
      {"slot_name", "Jade-ALTTP"},
      {"connected", true},
      {"ap_connected", true},
  });
  const auto snapshot_alias_resolved = snapshot_alias_runtime.ResolvedViewState();
  if (BuildSessionHeadline(snapshot_alias_runtime, snapshot_alias_resolved) != "Jade-ALTTP") {
    std::cerr << "tracker_overlay_renderer_snapshot_alias_headline_failed\n";
    return 1;
  }
  bool saw_link_on = false;
  bool saw_slot = false;
  bool saw_seed = false;
  for (const auto& chip : BuildMetadataChips(snapshot_alias_runtime, snapshot_alias_resolved)) {
    saw_link_on = saw_link_on || chip == "LINK ON";
    saw_slot = saw_slot || chip == "SLOT 1";
    saw_seed = saw_seed || chip == "SEED 136808774...";
  }
  if (!saw_link_on || !saw_slot || !saw_seed) {
    std::cerr << "tracker_overlay_renderer_snapshot_alias_chips_failed\n";
    return 1;
  }
  if (!ShouldSuppressPackLayoutNode({{"type", "group"}, {"header", "Settings"}}) ||
      !ShouldSuppressPackLayoutNode({{"type", "layout"}, {"key", "setting_grid"}}) ||
      !ShouldSuppressPackLayoutNode({{"type", "group"}, {"header", "Pulltree Drops"}}) ||
      !ShouldSuppressPackLayoutNode({{"type", "group"}, {"header", "Hoarder + Stun Drops"}}) ||
      !ShouldSuppressPackLayoutNode({{"type", "map"}, {"maps", nlohmann::json::array({"er_legend"})}}) ||
      ShouldSuppressPackLayoutNode({{"type", "group"}, {"header", "Items"}})) {
    std::cerr << "tracker_overlay_renderer_pack_layout_suppression_failed\n";
    return 1;
  }
  if (ResolveMapCoordinate(55.0, 0, 2007, 2007) != 55 ||
      ResolveMapCoordinate(45.0, 0, 2007, 2007) != 45 ||
      ResolveMapCoordinate(0.55, 0, 2007, 2007) != 1103 ||
      ResolveMapCoordinate(55.0, 0, 201, 0) != 110) {
    std::cerr << "tracker_overlay_renderer_coordinate_mode_failed\n";
    return 1;
  }

  const auto snapshot_layout_root = root / "snapshot-layout-bundle";
  fs::create_directories(snapshot_layout_root);
  WriteText(snapshot_layout_root / "manifest.json", R"JSON({
    "tracker_contract_version": "1",
    "linkedworld_id": "snapshot-layout-test",
    "display_name": "Snapshot Layout Tracker",
    "default_view_id": "main",
    "default_tab_id": "items",
    "default_map_id": "world",
    "maps": [{"id": "world", "label": "World"}],
    "tabs": [{"id": "items", "label": "Items", "view_id": "main", "map_id": "world"}],
    "views": [{"id": "main", "label": "Main", "tabs": ["items"]}]
  })JSON");

  TrackerRuntime snapshot_layout_runtime;
  snapshot_layout_runtime.LoadBundle(TrackerBundle::LoadFromDirectory(snapshot_layout_root));
  snapshot_layout_runtime.ApplyServerSnapshot({
      {"schema", "sekailink.tracker.snapshot.v1"},
      {"linkedworld_id", "snapshot-layout-test"},
      {"slot_id", "slot-snapshot-layout"},
      {"seed_id", "seed-snapshot-layout"},
      {"active_tab", "items"},
      {"active_map", "world"},
      {"pack_item_visuals",
       nlohmann::json::array({
           {
               {"primary_code", "snapshot_item"},
               {"name", "Snapshot Item"},
               {"type", "toggle"},
               {"image", ""},
               {"disabled_image", ""},
               {"aliases", nlohmann::json::array({"snapshot_item"})},
               {"stages", nlohmann::json::array()},
               {"images", nlohmann::json::array()},
           },
       })},
      {"items",
       nlohmann::json::array({
           {
               {"id", "snapshot_item"},
               {"code", "snapshot_item"},
               {"pack_visual_code", "snapshot_item"},
               {"label", "Snapshot Item"},
               {"acquired", true},
           },
       })},
      {"pack_layouts",
       nlohmann::json::array({
           {
               {"file", "snapshot-layout.json"},
               {"layout_ids", nlohmann::json::array({"tracker_default"})},
               {"json",
                {
                    {"tracker_default",
                     {
                         {"type", "itemgrid"},
                         {"item_size", 22},
                         {"rows", nlohmann::json::array({nlohmann::json::array({"snapshot_item"})})},
                     }},
                }},
           },
       })},
  });
  OverlayCanvas snapshot_layout_canvas(160, 120);
  snapshot_layout_canvas.Clear({0, 0, 0, 0});
  if (!RenderPackDrivenTrackerBody(snapshot_layout_canvas,
                                   snapshot_layout_runtime,
                                   snapshot_layout_runtime.ResolvedViewState(),
                                   0,
                                   0,
                                   160,
                                   120,
                                   nullptr)) {
    std::cerr << "tracker_overlay_renderer_snapshot_layout_not_used\n";
    return 1;
  }
  if (!CanvasHasVisiblePixels(snapshot_layout_canvas)) {
    std::cerr << "tracker_overlay_renderer_snapshot_layout_empty\n";
    return 1;
  }

  TrackerRuntime snapshot_only_layout_runtime;
  snapshot_only_layout_runtime.ApplyServerSnapshot({
      {"schema", "sekailink.tracker.snapshot.v1"},
      {"linkedworld_id", "snapshot-only-layout-test"},
      {"active_tab", "items"},
      {"active_map", "world"},
      {"pack_item_visuals",
       nlohmann::json::array({
           {
               {"primary_code", "snapshot_item"},
               {"name", "Snapshot Item"},
               {"type", "toggle"},
               {"aliases", nlohmann::json::array({"snapshot_item"})},
           },
       })},
      {"items",
       nlohmann::json::array({
           {
               {"id", "snapshot_item"},
               {"code", "snapshot_item"},
               {"pack_visual_code", "snapshot_item"},
               {"label", "Snapshot Item"},
               {"acquired", true},
           },
       })},
      {"pack_layouts",
       nlohmann::json::array({
           {
               {"file", "snapshot-only-layout.json"},
               {"layout_ids", nlohmann::json::array({"tracker_default"})},
               {"json",
                {
                    {"tracker_default",
                     {
                         {"type", "itemgrid"},
                         {"item_size", 22},
                         {"rows", nlohmann::json::array({nlohmann::json::array({"snapshot_item"})})},
                     }},
                }},
           },
       })},
  });
  OverlayCanvas snapshot_only_layout_canvas(160, 120);
  snapshot_only_layout_canvas.Clear({0, 0, 0, 0});
  if (!RenderPackDrivenTrackerBody(snapshot_only_layout_canvas,
                                   snapshot_only_layout_runtime,
                                   snapshot_only_layout_runtime.ResolvedViewState(),
                                   0,
                                   0,
                                   160,
                                   120,
                                   nullptr)) {
    std::cerr << "tracker_overlay_renderer_snapshot_only_layout_not_used\n";
    return 1;
  }
  if (!CanvasHasVisiblePixels(snapshot_only_layout_canvas)) {
    std::cerr << "tracker_overlay_renderer_snapshot_only_layout_empty\n";
    return 1;
  }

  std::cout << "tracker_overlay_renderer_smoke_ok\n";
  return 0;
}
