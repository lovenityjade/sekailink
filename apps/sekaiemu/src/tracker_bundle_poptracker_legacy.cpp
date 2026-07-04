#include "tracker_bundle_poptracker_legacy.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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

std::string PopTrackerMapId(std::string_view name) {
  std::string out = "poptracker_";
  for (const char ch : name) {
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    } else {
      out.push_back('_');
    }
  }
  while (out.find("__") != std::string::npos) {
    out.erase(out.find("__"), 1);
  }
  if (!out.empty() && out.back() == '_') {
    out.pop_back();
  }
  return out;
}

std::string PackMapName(const nlohmann::json& value) {
  return JsonString(value,
                    {"poptracker_map_name",
                     "pack_map_name",
                     "packMapName",
                     "poptracker_map",
                     "poptrackerMap",
                     "pack_map",
                     "packMap"});
}

}  // namespace

void ApplyPopTrackerAdaptedMaps(nlohmann::json& raw,
                                const std::filesystem::path& bundle_root) {
  const auto maps_path = bundle_root / "poptracker-adapted" / "maps" / "maps.json";
  if (!std::filesystem::exists(maps_path)) {
    return;
  }
  const auto pop_maps = nlohmann::json::parse(ReadTextFile(maps_path));
  if (!pop_maps.is_array()) {
    return;
  }

  std::unordered_map<std::string, std::string> image_by_name;
  for (const auto& entry : pop_maps) {
    const auto name = JsonString(entry, {"name", "id"});
    const auto image = JsonString(entry, {"img", "image"});
    if (!name.empty() && !image.empty()) {
      image_by_name[name] = "poptracker-adapted/" + image;
    }
  }

  std::unordered_map<std::string, std::string> poptracker_name_to_map_id;
  std::unordered_set<std::string> map_ids;
  if (raw["maps"].is_array()) {
    for (auto& map : raw["maps"]) {
      const auto id = JsonString(map, {"id", "map_id", "mapId"});
      if (!id.empty()) {
        map_ids.insert(id);
      }
      const auto pack_map_name = PackMapName(map);
      if (id.empty() || pack_map_name.empty()) {
        continue;
      }
      const auto image = image_by_name.find(pack_map_name);
      if (image == image_by_name.end()) {
        continue;
      }
      map["image"] = image->second;
      map["art_origin"] = "poptracker-adapted-pack";
      map["poptracker_map_name"] = pack_map_name;
      poptracker_name_to_map_id[pack_map_name] = id;
    }
  }

  for (const auto& [name, image] : image_by_name) {
    if (poptracker_name_to_map_id.contains(name)) {
      continue;
    }
    const auto map_id = PopTrackerMapId(name);
    if (map_ids.contains(map_id)) {
      continue;
    }
    poptracker_name_to_map_id[name] = map_id;
    map_ids.insert(map_id);
    raw["maps"].push_back({
        {"id", map_id},
        {"label", name},
        {"image", image},
        {"art_origin", "poptracker-adapted-pack"},
        {"poptracker_map_name", name},
    });
  }
  if (raw["tabs"].is_array()) {
    for (auto& tab : raw["tabs"]) {
      const auto pack_map_name = PackMapName(tab);
      if (pack_map_name.empty()) {
        continue;
      }
      const auto map_id = poptracker_name_to_map_id.find(pack_map_name);
      if (map_id != poptracker_name_to_map_id.end()) {
        tab["map"] = map_id->second;
      }
    }
  }
}

}  // namespace sekaiemu::spike
