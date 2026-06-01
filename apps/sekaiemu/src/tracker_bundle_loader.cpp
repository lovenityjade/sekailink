#include "tracker_runtime.hpp"

#include "tracker_bundle_archive.hpp"
#include "tracker_bundle_poptracker_legacy.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sekaiemu::spike {
namespace {

std::string TrimCopy(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_file_open_failed:" + path.string());
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::string JsonString(const nlohmann::json& value,
                       std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto it = value.find(key);
    if (it != value.end() && it->is_string()) {
      const auto parsed = TrimCopy(it->get<std::string>());
      if (!parsed.empty()) {
        return parsed;
      }
    }
  }
  return {};
}

std::vector<std::string> JsonStringArray(const nlohmann::json& value,
                                         std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto it = value.find(key);
    if (it == value.end() || !it->is_array()) {
      continue;
    }
    std::vector<std::string> out;
    for (const auto& entry : *it) {
      if (!entry.is_string()) {
        continue;
      }
      const auto parsed = TrimCopy(entry.get<std::string>());
      if (!parsed.empty()) {
        out.push_back(parsed);
      }
    }
    return out;
  }
  return {};
}

std::filesystem::path ResolveBundleAssetPath(const std::filesystem::path& bundle_root,
                                             std::string_view asset_path) {
  while (!asset_path.empty() && (asset_path.front() == '/' || asset_path.front() == '\\')) {
    asset_path.remove_prefix(1);
  }
  return bundle_root / std::filesystem::path(std::string(asset_path));
}

std::optional<nlohmann::json> LoadOptionalJsonRef(const std::filesystem::path& bundle_root,
                                                  const nlohmann::json& raw,
                                                  const char* ref_key) {
  const auto ref = JsonString(raw, {ref_key});
  if (ref.empty()) {
    return std::nullopt;
  }
  const auto path = ResolveBundleAssetPath(bundle_root, ref);
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  return nlohmann::json::parse(ReadTextFile(path));
}

void MergeReferencedJson(nlohmann::json& raw,
                         const std::filesystem::path& bundle_root,
                         const char* ref_key,
                         const char* target_key) {
  if (raw.contains(target_key)) {
    return;
  }
  if (auto referenced = LoadOptionalJsonRef(bundle_root, raw, ref_key); referenced.has_value()) {
    raw[target_key] = std::move(*referenced);
  }
}

TrackerMapDefinition ParseMapDefinition(const nlohmann::json& raw) {
  TrackerMapDefinition map;
  map.raw = raw;
  map.id = JsonString(raw, {"id", "map_id", "mapId"});
  map.label = JsonString(raw, {"label", "name", "title"});
  map.image = JsonString(raw, {"image", "image_path", "imagePath"});
  map.image_transparent = raw.value("image_transparent", raw.value("imageTransparent", false));
  map.zones = JsonStringArray(raw, {"zones", "zone_ids", "zoneIds"});
  if (map.id.empty()) {
    throw std::runtime_error("tracker_map_missing_id");
  }
  return map;
}

TrackerTabDefinition ParseTabDefinition(const nlohmann::json& raw) {
  TrackerTabDefinition tab;
  tab.raw = raw;
  tab.id = JsonString(raw, {"id", "tab_id", "tabId"});
  tab.label = JsonString(raw, {"label", "name", "title"});
  tab.view_id = JsonString(raw, {"view", "view_id", "viewId"});
  tab.map_id = JsonString(raw, {"map", "map_id", "mapId"});
  tab.zones = JsonStringArray(raw, {"zones", "zone_ids", "zoneIds"});
  if (tab.id.empty()) {
    throw std::runtime_error("tracker_tab_missing_id");
  }
  return tab;
}

TrackerViewDefinition ParseViewDefinition(const nlohmann::json& raw) {
  TrackerViewDefinition view;
  view.raw = raw;
  view.id = JsonString(raw, {"id", "view_id", "viewId"});
  view.label = JsonString(raw, {"label", "name", "title"});
  view.tab_ids = JsonStringArray(raw, {"tabs", "tab_ids", "tabIds"});
  if (view.id.empty()) {
    throw std::runtime_error("tracker_view_missing_id");
  }
  return view;
}

TrackerInfoFieldDefinition ParseInfoFieldDefinition(const nlohmann::json& raw) {
  TrackerInfoFieldDefinition field;
  field.raw = raw;
  field.id = JsonString(raw, {"id", "field_id", "fieldId"});
  field.label = JsonString(raw, {"label", "name", "title"});
  field.source = JsonString(raw, {"source", "from"});
  field.key = JsonString(raw, {"key", "path"});
  field.fallback = JsonString(raw, {"fallback", "default"});
  field.hide_when_empty =
      raw.value("hide_when_empty",
                raw.value("hideWhenEmpty", raw.value("omit_if_empty", raw.value("omitIfEmpty", false))));
  if (field.id.empty()) {
    field.id = field.key;
  }
  if (field.label.empty()) {
    field.label = field.id;
  }
  if (field.source.empty()) {
    field.source = "session";
  }
  return field;
}

TrackerInfoPanelDefinition ParseInfoPanelDefinition(const nlohmann::json& raw) {
  TrackerInfoPanelDefinition panel;
  panel.raw = raw;
  panel.id = JsonString(raw, {"id", "panel_id", "panelId"});
  panel.label = JsonString(raw, {"label", "name", "title"});
  if (panel.id.empty()) {
    throw std::runtime_error("tracker_info_panel_missing_id");
  }
  if (panel.label.empty()) {
    panel.label = panel.id;
  }
  if (const auto surface = JsonString(raw, {"surface", "placement", "area"}); !surface.empty()) {
    panel.surface = surface;
  }
  panel.priority = raw.value("priority", raw.value("order", 100));
  for (const auto& field : raw.value("fields", nlohmann::json::array())) {
    panel.fields.push_back(ParseInfoFieldDefinition(field));
  }
  return panel;
}

}  // namespace

TrackerBundle TrackerBundle::LoadFromPath(const std::filesystem::path& bundle_path) {
  if (std::filesystem::is_directory(bundle_path)) {
    return LoadFromDirectory(bundle_path);
  }
  if (std::filesystem::is_regular_file(bundle_path) && IsZipArchivePath(bundle_path)) {
    return LoadFromDirectory(MaterializeTrackerArchive(bundle_path));
  }
  throw std::runtime_error("tracker_bundle_path_unsupported:" + bundle_path.string());
}

TrackerBundle TrackerBundle::LoadFromDirectory(const std::filesystem::path& bundle_root) {
  const auto manifest_path = bundle_root / "manifest.json";
  auto raw = nlohmann::json::parse(ReadTextFile(manifest_path));
  MergeReferencedJson(raw, bundle_root, "surface_inventory_ref", "surface_inventory");
  MergeReferencedJson(raw, bundle_root, "location_groups_ref", "location_groups");
  MergeReferencedJson(raw, bundle_root, "item_slots_ref", "item_slots");
  MergeReferencedJson(raw, bundle_root, "dungeon_progress_ref", "dungeon_progress");
  MergeReferencedJson(raw, bundle_root, "room_metadata_ref", "room_metadata");
  MergeReferencedJson(raw, bundle_root, "slot_data_ref", "slot_data");
  MergeReferencedJson(raw, bundle_root, "tracker_flow_ref", "tracker_flow");
  MergeReferencedJson(raw, bundle_root, "item_icon_metadata_ref", "item_icon_metadata");
  MergeReferencedJson(raw, bundle_root, "map_pin_metadata_ref", "map_pin_metadata");
  MergeReferencedJson(raw, bundle_root, "settings_metadata_ref", "settings_metadata");
  MergeReferencedJson(raw, bundle_root, "autotabbing_hints_ref", "autotabbing_hints");
  MergeReferencedJson(raw, bundle_root, "poptracker_adaptation_ref", "poptracker_adaptation");
  ApplyPopTrackerAdaptedMaps(raw, bundle_root);
  TrackerBundle bundle;
  bundle.raw = raw;
  bundle.contract_version =
      JsonString(raw, {"tracker_contract_version", "contract_version", "contractVersion"});
  if (bundle.contract_version.empty()) {
    bundle.contract_version = "1";
  }
  bundle.linkedworld_id = JsonString(raw, {"linkedworld_id", "linkedworldId"});
  bundle.display_name = JsonString(raw, {"display_name", "displayName"});
  bundle.default_view_id = JsonString(raw, {"default_view_id", "defaultViewId"});
  bundle.default_tab_id = JsonString(raw, {"default_tab_id", "defaultTabId"});
  bundle.default_map_id = JsonString(raw, {"default_map_id", "defaultMapId"});
  bundle.bundle_root = bundle_root;

  for (const auto& raw_map : raw.value("maps", nlohmann::json::array())) {
    auto map = ParseMapDefinition(raw_map);
    if (!map.image.empty()) {
      map.resolved_image_path = ResolveBundleAssetPath(bundle_root, map.image);
      if (std::filesystem::exists(map.resolved_image_path)) {
        try {
          map.raster_image = LoadTrackerRasterAsset(map.resolved_image_path, map.image_transparent);
          map.raster_load_error.clear();
        } catch (const std::exception& exception) {
          // Keep tracker startup resilient when a bundle ships an unsupported
          // raster format. The overlay can still render textual state while the
          // bundle is corrected or re-exported to a supported image format.
          map.raster_image.reset();
          map.raster_load_error = exception.what();
        }
      } else {
        map.raster_load_error = "tracker_image_missing:" + map.resolved_image_path.string();
      }
    }
    bundle.maps.push_back(std::move(map));
  }
  for (const auto& raw_tab : raw.value("tabs", nlohmann::json::array())) {
    bundle.tabs.push_back(ParseTabDefinition(raw_tab));
  }
  for (const auto& raw_view : raw.value("views", nlohmann::json::array())) {
    bundle.views.push_back(ParseViewDefinition(raw_view));
  }
  for (const auto& raw_panel : raw.value("info_panels", raw.value("infoPanels", nlohmann::json::array()))) {
    bundle.info_panels.push_back(ParseInfoPanelDefinition(raw_panel));
  }

  const auto zone_bindings =
      raw.value("zone_bindings", raw.value("zoneBindings", nlohmann::json::object()));
  if (zone_bindings.is_object()) {
    for (auto it = zone_bindings.begin(); it != zone_bindings.end(); ++it) {
      if (it.value().is_string()) {
        bundle.zone_map_bindings[it.key()] = it.value().get<std::string>();
      }
    }
  }

  bundle.seed_option_bindings =
      raw.value("seed_option_bindings", raw.value("seedOptionBindings", nlohmann::json::array()));

  if (bundle.default_view_id.empty() && !bundle.views.empty()) {
    bundle.default_view_id = bundle.views.front().id;
  }
  if (bundle.default_tab_id.empty() && !bundle.tabs.empty()) {
    bundle.default_tab_id = bundle.tabs.front().id;
  }
  if (bundle.default_map_id.empty() && !bundle.maps.empty()) {
    bundle.default_map_id = bundle.maps.front().id;
  }
  return bundle;
}

const TrackerMapDefinition* TrackerBundle::FindMap(std::string_view id) const {
  const auto it =
      std::find_if(maps.begin(), maps.end(), [&](const auto& map) { return map.id == id; });
  return it == maps.end() ? nullptr : &*it;
}

const TrackerTabDefinition* TrackerBundle::FindTab(std::string_view id) const {
  const auto it =
      std::find_if(tabs.begin(), tabs.end(), [&](const auto& tab) { return tab.id == id; });
  return it == tabs.end() ? nullptr : &*it;
}

const TrackerViewDefinition* TrackerBundle::FindView(std::string_view id) const {
  const auto it =
      std::find_if(views.begin(), views.end(), [&](const auto& view) { return view.id == id; });
  return it == views.end() ? nullptr : &*it;
}

}  // namespace sekaiemu::spike
