#include "tracker_runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

namespace fs = std::filesystem;
using sekaiemu::spike::TrackerBundle;
using sekaiemu::spike::TrackerDisplayMode;
using sekaiemu::spike::TrackerRuntime;

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

void WriteText(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("write_failed:" + path.string());
  }
  output << text;
}

void WriteBinary(const fs::path& path, const std::vector<std::uint8_t>& data) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("write_failed:" + path.string());
  }
  output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void AddFileToZip(archive* writer, const fs::path& root, const fs::path& path) {
  const auto relative = fs::relative(path, root).generic_string();
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("zip_input_open_failed:" + path.string());
  }
  const auto size = static_cast<std::int64_t>(fs::file_size(path));
  archive_entry* entry = archive_entry_new();
  archive_entry_set_pathname(entry, relative.c_str());
  archive_entry_set_size(entry, size);
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  if (archive_write_header(writer, entry) != ARCHIVE_OK) {
    const std::string error =
        archive_error_string(writer) != nullptr ? archive_error_string(writer) : "archive_write_header_failed";
    archive_entry_free(entry);
    throw std::runtime_error("zip_header_failed:" + error);
  }

  std::vector<char> buffer(64 * 1024);
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto read_size = input.gcount();
    if (read_size > 0 && archive_write_data(writer, buffer.data(), read_size) < 0) {
      const std::string error =
          archive_error_string(writer) != nullptr ? archive_error_string(writer) : "archive_write_data_failed";
      archive_entry_free(entry);
      throw std::runtime_error("zip_data_failed:" + error);
    }
  }
  archive_entry_free(entry);
}

void WriteZipFromDirectory(const fs::path& archive_path, const fs::path& root) {
  archive* writer = archive_write_new();
  if (writer == nullptr) {
    throw std::runtime_error("zip_writer_init_failed");
  }
  archive_write_add_filter_none(writer);
  archive_write_set_format_zip(writer);
  const auto archive_path_u8 = archive_path.u8string();
  const std::string archive_path_text(archive_path_u8.begin(), archive_path_u8.end());
  if (archive_write_open_filename(writer, archive_path_text.c_str()) != ARCHIVE_OK) {
    const std::string error =
        archive_error_string(writer) != nullptr ? archive_error_string(writer) : "archive_write_open_failed";
    archive_write_free(writer);
    throw std::runtime_error("zip_open_failed:" + error);
  }
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      AddFileToZip(writer, root, entry.path());
    }
  }
  archive_write_close(writer);
  archive_write_free(writer);
}

std::string MakeSolidPpm(int width, int height, int red, int green, int blue, int center_red, int center_green, int center_blue) {
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

const std::vector<std::uint8_t> kSmokePng = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
    0x01, 0x03, 0x00, 0x00, 0x00, 0x48, 0x78, 0x9f, 0x67, 0x00, 0x00, 0x00,
    0x06, 0x50, 0x4c, 0x54, 0x45, 0x11, 0x22, 0x33, 0xff, 0xff, 0xff, 0x34,
    0xcc, 0x65, 0xc8, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41, 0x54, 0x08,
    0xd7, 0x63, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x27,
    0x34, 0x27, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82};

const std::vector<std::uint8_t> kSmokeJpg = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02, 0x02, 0x03,
    0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06,
    0x06, 0x05, 0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a,
    0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e, 0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d,
    0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13, 0x12, 0x10,
    0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x03, 0x03,
    0x03, 0x04, 0x03, 0x04, 0x08, 0x04, 0x04, 0x08, 0x10, 0x0b, 0x09, 0x0b,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x02, 0x00, 0x02, 0x03,
    0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xc4, 0x00, 0x14, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11,
    0x00, 0x3f, 0x00, 0x2a, 0x3d, 0x0a, 0x7f, 0xff, 0xd9};

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc >= 2) {
      const TrackerBundle external_bundle = TrackerBundle::LoadFromPath(argv[1]);
      if (external_bundle.linkedworld_id.empty() || external_bundle.maps.empty() ||
          external_bundle.tabs.empty()) {
        throw std::runtime_error("external_bundle_load_failed");
      }
      std::cout << "tracker_runtime_external_bundle_ok\n";
      return 0;
    }

    const fs::path root = fs::temp_directory_path() / "sekaiemu-libretro-tracker-runtime-smoke";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "bundle" / "maps");
    WriteText(root / "bundle" / "maps" / "overworld.ppm", MakeSolidPpm(12, 8, 16, 96, 24, 240, 232, 80));
    WriteText(root / "bundle" / "maps" / "dungeon.ppm", MakeSolidPpm(12, 8, 24, 32, 112, 255, 96, 64));
    WriteBinary(root / "bundle" / "poptracker-adapted" / "images" / "maps" / "linkedworld-static.png", kSmokePng);
    WriteBinary(root / "bundle" / "poptracker-adapted" / "images" / "items" / "linkedworld-static.jpg", kSmokeJpg);
    WriteText(root / "bundle" / "manifest.json", R"JSON({
  "tracker_contract_version": "1",
  "linkedworld_id": "earthbound",
  "display_name": "EarthBound",
  "default_view_id": "main",
  "default_tab_id": "items",
  "default_map_id": "overworld",
  "views": [
    {"id": "main", "label": "Main", "tabs": ["items", "map", "dungeon"]}
  ],
  "tabs": [
    {"id": "items", "label": "Items", "view": "main"},
    {"id": "map", "label": "Map", "view": "main", "map": "overworld", "zones": ["zone.overworld"]},
    {"id": "dungeon", "label": "Dungeon", "view": "main", "map": "dungeon", "zones": ["zone.dungeon"]}
  ],
  "maps": [
    {"id": "overworld", "label": "Overworld", "image": "maps/overworld.ppm"},
    {"id": "dungeon", "label": "Dungeon", "image": "maps/dungeon.ppm", "image_transparent": true},
    {"id": "linkedworld-png", "label": "LinkedWorld PNG", "image": "poptracker-adapted/images/maps/linkedworld-static.png"},
    {"id": "linkedworld-jpg", "label": "LinkedWorld JPG", "image": "/poptracker-adapted/images/items/linkedworld-static.jpg"}
  ],
  "zone_bindings": {
    "zone.overworld": "overworld",
    "zone.dungeon": "dungeon"
  },
  "info_panels": [
    {
      "id": "session",
      "label": "Session",
      "fields": [
        {"id": "slot", "label": "Slot", "source": "session", "key": "slot_id"},
        {"id": "zone", "label": "Zone", "source": "session", "key": "current_zone_id"}
      ]
    },
    {
      "id": "seed",
      "label": "Seed",
      "fields": [
        {"id": "mode", "label": "Mode", "source": "seed", "key": "mode"},
        {"id": "difficulty", "label": "Diff", "source": "seed", "key": "difficulty"}
      ]
    },
    {
      "id": "live",
      "label": "Live",
      "fields": [
        {"id": "progress", "label": "Progress", "source": "session", "key": "check_progress"},
        {"id": "received", "label": "Received", "source": "session", "key": "received_count"},
        {"id": "last-item", "label": "Last Item", "source": "session", "key": "last_received_label"},
        {"id": "last-check", "label": "Last Check", "source": "session", "key": "last_check_label"},
        {"id": "last-item-at", "label": "Item At", "source": "session", "key": "last_received_timestamp", "hide_when_empty": true},
        {"id": "last-check-at", "label": "Check At", "source": "session", "key": "last_check_timestamp", "hide_when_empty": true},
        {"id": "empty-optional", "label": "Optional", "source": "snapshot", "key": "missing_field", "hide_when_empty": true}
      ]
    },
    {
      "id": "snapshot",
      "label": "Snapshot",
      "fields": [
        {"id": "alias", "label": "Alias", "source": "snapshot", "key": "slot_alias"}
      ]
    },
    {
      "id": "optional",
      "label": "Optional",
      "fields": [
        {"id": "driver", "label": "Driver", "source": "snapshot", "key": "room_metadata.driver_instance_id", "hide_when_empty": true}
      ]
    }
  ],
  "seed_option_bindings": [
    {"option": "mode", "equals": "dungeon", "disable_tabs": ["items"]}
  ]
})JSON");

    const TrackerBundle bundle = TrackerBundle::LoadFromPath(root / "bundle");
    if (bundle.linkedworld_id != "earthbound" || bundle.views.size() != 1 ||
        bundle.tabs.size() != 3 || bundle.maps.size() != 4) {
      throw std::runtime_error("bundle_parse_failed");
    }
    const auto* png_map = bundle.FindMap("linkedworld-png");
    const auto* jpg_map = bundle.FindMap("linkedworld-jpg");
    if (png_map == nullptr || !png_map->raster_image.has_value() ||
        png_map->raster_image->width != 2 || png_map->raster_image->height != 2) {
      throw std::runtime_error("bundle_png_decode_failed:" +
                               (png_map == nullptr ? std::string("missing") : png_map->raster_load_error));
    }
    if (jpg_map == nullptr || !jpg_map->raster_image.has_value() ||
        jpg_map->raster_image->width != 2 || jpg_map->raster_image->height != 2) {
      throw std::runtime_error("bundle_jpg_decode_failed:" +
                               (jpg_map == nullptr ? std::string("missing") : jpg_map->raster_load_error));
    }

    const auto archive_path = root / "bundle.zip";
    WriteZipFromDirectory(archive_path, root / "bundle");
    const TrackerBundle archive_bundle = TrackerBundle::LoadFromPath(archive_path);
    const auto* archive_png_map = archive_bundle.FindMap("linkedworld-png");
    if (archive_bundle.linkedworld_id != "earthbound" || archive_png_map == nullptr ||
        !archive_png_map->raster_image.has_value() || archive_png_map->raster_image->width != 2 ||
        archive_png_map->raster_image->height != 2) {
      throw std::runtime_error("bundle_zip_load_failed");
    }

    TrackerRuntime runtime;
    runtime.LoadBundle(bundle);
    runtime.ApplyServerSnapshot({
        {"world_instance_id", "world-1"},
        {"linkedworld_id", "earthbound"},
        {"slot_id", "slot-7"},
        {"seed_id", "seed-42"},
        {"slot_alias", "Tracy"},
        {"checked_locations",
         nlohmann::json::array({
             nlohmann::json{{"location_id", 101}, {"location_name", "Tracy Gift"}},
             nlohmann::json{{"location_id", 102}, {"location_name", "Burger Shop"}, {"timestamp", "00:00:52"}},
         })},
        {"missing_locations", nlohmann::json::array({103, 104, 105})},
        {"received_items",
         nlohmann::json::array({
             nlohmann::json{{"item_id", 301}, {"item_name", "Sound Stone"}},
             nlohmann::json{{"item_id", 301}, {"item_name", "Sound Stone"}, {"timestamp", "00:00:34"}},
         })},
        {"seed_metadata", {{"mode", "standard"}, {"difficulty", "normal"}}},
    });
    runtime.ApplyRuntimeContext({{"zone_id", "zone.overworld"}});
    auto resolved = runtime.ResolvedViewState();
    if (resolved.active_map_id != "overworld") {
      throw std::runtime_error("zone_binding_failed");
    }
    const auto* session_panel = FindPanel(resolved.info_panels, "session");
    const auto* seed_panel = FindPanel(resolved.info_panels, "seed");
    const auto* live_panel = FindPanel(resolved.info_panels, "live");
    const auto* snapshot_panel = FindPanel(resolved.info_panels, "snapshot");
    if (session_panel == nullptr || seed_panel == nullptr || live_panel == nullptr ||
        snapshot_panel == nullptr) {
      throw std::runtime_error("info_panel_parse_failed");
    }
    if (const auto* slot_field = FindField(*session_panel, "slot");
        slot_field == nullptr || slot_field->value("value", std::string{}) != "slot-7") {
      throw std::runtime_error("session_field_resolution_failed");
    }
    if (const auto* mode_field = FindField(*seed_panel, "mode");
        mode_field == nullptr || mode_field->value("value", std::string{}) != "standard") {
      throw std::runtime_error("seed_field_resolution_failed");
    }
    if (const auto* progress_field = FindField(*live_panel, "progress");
        progress_field == nullptr || progress_field->value("value", std::string{}) != "2/5") {
      throw std::runtime_error("progress_field_resolution_failed");
    }
    if (const auto* received_field = FindField(*live_panel, "received");
        received_field == nullptr || received_field->value("value", std::string{}) != "2") {
      throw std::runtime_error("received_count_resolution_failed");
    }
    if (const auto* last_item_field = FindField(*live_panel, "last-item");
        last_item_field == nullptr || last_item_field->value("value", std::string{}) != "Sound Stone") {
      throw std::runtime_error("last_item_field_resolution_failed");
    }
    if (const auto* last_check_field = FindField(*live_panel, "last-check");
        last_check_field == nullptr || last_check_field->value("value", std::string{}) != "Burger Shop") {
      throw std::runtime_error("last_check_field_resolution_failed");
    }
    if (const auto* item_time_field = FindField(*live_panel, "last-item-at");
        item_time_field == nullptr || item_time_field->value("value", std::string{}) != "00:00:34") {
      throw std::runtime_error("last_item_timestamp_resolution_failed");
    }
    if (const auto* check_time_field = FindField(*live_panel, "last-check-at");
        check_time_field == nullptr || check_time_field->value("value", std::string{}) != "00:00:52") {
      throw std::runtime_error("last_check_timestamp_resolution_failed");
    }
    if (FindField(*live_panel, "empty-optional") != nullptr) {
      throw std::runtime_error("hide_when_empty_field_failed");
    }
    if (FindPanel(resolved.info_panels, "optional") != nullptr) {
      throw std::runtime_error("hide_when_empty_panel_failed");
    }
    if (const auto* alias_field = FindField(*snapshot_panel, "alias");
        alias_field == nullptr || alias_field->value("value", std::string{}) != "Tracy") {
      throw std::runtime_error("snapshot_field_resolution_failed");
    }

    runtime.ApplySklmiEvent({
        {"event_type", "location_checked"},
        {"location_id", 101},
        {"label", "Tracy Check"},
        {"timestamp", "00:01:00"},
    });
    resolved = runtime.ResolvedViewState();
    if (resolved.recent_events.size() != 1 || !resolved.recent_events.front().is_object() ||
        resolved.recent_events.front().value("label", std::string{}) != "Tracy Check") {
      throw std::runtime_error("recent_event_capture_failed");
    }

    const auto mutation_before_local_ui = runtime.MutationSerial();
    runtime.ApplyLocalToggle("boss-key", true);
    runtime.SetDisplayMode(TrackerDisplayMode::ToggleScreen);
    runtime.TogglePrimaryScreen();
    runtime.SetManualMap("dungeon");
    if (runtime.MutationSerial() <= mutation_before_local_ui) {
      throw std::runtime_error("mutation_serial_local_ui_failed");
    }
    resolved = runtime.ResolvedViewState();
    if (resolved.active_map_id != "dungeon" || resolved.show_tracker_screen) {
      throw std::runtime_error("override_or_toggle_screen_failed");
    }

    const fs::path state_path = root / "tracker-state.json";
    runtime.SavePersistedState(state_path);
    const auto persisted = TrackerRuntime::ReadPersistedState(state_path);
    TrackerRuntime recovered;
    recovered.LoadBundle(bundle);
    recovered.ApplyServerSnapshot({
        {"world_instance_id", "world-1"},
        {"linkedworld_id", "earthbound"},
        {"slot_id", "slot-7"},
        {"seed_id", "seed-42"},
        {"seed_metadata", {{"mode", "dungeon"}}},
    });
    recovered.LoadPersistedState(persisted);
    recovered.SetAutoMapFollow(true);
    recovered.ApplyRuntimeContext({{"zone_id", "zone.dungeon"}});
    if (recovered.MutationSerial() == 0) {
      throw std::runtime_error("mutation_serial_recovery_failed");
    }
    resolved = recovered.ResolvedViewState();
    if (resolved.active_map_id != "dungeon") {
      throw std::runtime_error("state_recovery_or_zone_follow_failed");
    }
    if (resolved.toggles.value("boss-key", false) != true) {
      throw std::runtime_error("toggle_state_missing");
    }
    for (const auto& tab : resolved.visible_tabs) {
      if (tab.value("id", std::string{}) == "items") {
        throw std::runtime_error("seed_option_binding_failed");
      }
    }

    TrackerRuntime linkedworld_style;
    linkedworld_style.LoadBundle(bundle);
    linkedworld_style.ApplyServerSnapshot({
        {"serverSnapshot",
         {
             {"worldInstanceId", "world-2"},
             {"linkedworldId", "earthbound"},
             {"slotId", "slot-8"},
             {"seedId", "seed-99"},
             {"seedSettings", {{"mode", "dungeon"}, {"difficulty", "hard"}}},
             {"slotData", {{"name", "linkedworld-style"}}},
             {"locations",
              {
                  {"checked",
                   nlohmann::json::array({
                       nlohmann::json{{"id", "201"}, {"label", "Nested Check"}},
                   })},
                  {"missing", nlohmann::json::array({"202", "203"})},
              }},
             {"items",
              {
                  {"received",
                   nlohmann::json::array({
                       nlohmann::json{{"id", "401"}, {"label", "Nested Badge"}, {"senderAlias", "Paula"}},
                   })},
              }},
             {"trackerState",
              {
                  {"activeMapId", "dungeon"},
                  {"autoFollowMap", true},
              }},
         }},
    });
    resolved = linkedworld_style.ResolvedViewState();
    if (resolved.active_map_id != "dungeon" || resolved.active_tab_id != "dungeon") {
      throw std::runtime_error("linkedworld_hint_autotab_failed");
    }
    const auto* linked_seed_panel = FindPanel(resolved.info_panels, "seed");
    const auto* linked_live_panel = FindPanel(resolved.info_panels, "live");
    if (linked_seed_panel == nullptr || linked_live_panel == nullptr) {
      throw std::runtime_error("linkedworld_panel_resolution_failed");
    }
    if (const auto* mode_field = FindField(*linked_seed_panel, "mode");
        mode_field == nullptr || mode_field->value("value", std::string{}) != "dungeon") {
      throw std::runtime_error("linkedworld_seed_settings_failed");
    }
    if (const auto* received_field = FindField(*linked_live_panel, "received");
        received_field == nullptr || received_field->value("value", std::string{}) != "1") {
      throw std::runtime_error("linkedworld_received_count_failed");
    }
    if (const auto* last_item_field = FindField(*linked_live_panel, "last-item");
        last_item_field == nullptr || last_item_field->value("value", std::string{}) != "Nested Badge") {
      throw std::runtime_error("linkedworld_received_label_failed");
    }
    if (const auto* progress_field = FindField(*linked_live_panel, "progress");
        progress_field == nullptr || progress_field->value("value", std::string{}) != "1/3") {
      throw std::runtime_error("linkedworld_nested_progress_failed");
    }

    std::cout << "tracker_runtime_smoke_ok\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[tracker-runtime-smoke] " << ex.what() << '\n';
    return 1;
  }
}
