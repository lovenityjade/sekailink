#include "tracker_overlay_pack_metadata.hpp"

#include "tracker_overlay_render_state.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <tuple>

namespace sekaiemu::spike::tracker_overlay_state_detail {
namespace {

namespace fs = std::filesystem;

std::string ReadTextFile(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_overlay_file_open_failed:" + path.string());
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::string LowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string CanonicalName(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isalnum(ch) != 0) {
      out.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return out;
}

const std::unordered_map<std::string, fs::path>& PopTrackerItemImageIndex(const TrackerBundle& bundle) {
  static std::unordered_map<std::string, std::unordered_map<std::string, fs::path>> cache;
  const auto cache_key = bundle.bundle_root.string();
  const auto found = cache.find(cache_key);
  if (found != cache.end()) {
    return found->second;
  }

  auto& index = cache[cache_key];
  const fs::path items_root = bundle.bundle_root / "poptracker-adapted" / "images" / "items";
  std::error_code ec;
  if (fs::exists(items_root, ec)) {
    for (const auto& entry : fs::directory_iterator(items_root, ec)) {
      if (ec || !entry.is_regular_file()) {
        continue;
      }
      const auto canonical = CanonicalName(entry.path().stem().string());
      if (!canonical.empty()) {
        index.emplace(canonical, entry.path().lexically_relative(bundle.bundle_root));
      }
    }
  }
  return index;
}

std::string ResolvePopTrackerMapId(const TrackerBundle& bundle, std::string_view poptracker_name) {
  const auto canonical = CanonicalName(poptracker_name);
  for (const auto& map : bundle.maps) {
    if (CanonicalName(JsonStringAtAnyKey(map.raw, {"poptracker_map_name"})) == canonical) {
      return map.id;
    }
  }
  return {};
}

void CollectMapLocations(const nlohmann::json& node,
                         std::vector<std::tuple<std::string, double, double>>& out) {
  if (!node.is_object()) {
    return;
  }
  if (const auto* locations = JsonValueAtPath(node, "map_locations");
      locations != nullptr && locations->is_array()) {
    for (const auto& entry : *locations) {
      if (!entry.is_object()) {
        continue;
      }
      const auto map_name = JsonStringAtAnyKey(entry, {"map", "map_id", "mapId"});
      const auto x = JsonNumberAtAnyKey(entry, {"x", "left", "map_x", "mapX"});
      const auto y = JsonNumberAtAnyKey(entry, {"y", "top", "map_y", "mapY"});
      if (!map_name.empty() && x.has_value() && y.has_value()) {
        out.emplace_back(map_name, *x, *y);
      }
    }
  }
  if (const auto* children = JsonValueAtPath(node, "children");
      children != nullptr && children->is_array()) {
    for (const auto& child : *children) {
      CollectMapLocations(child, out);
    }
  }
}

}  // namespace

std::vector<SemanticItemBinding> LoadSemanticItemBindings(const TrackerBundle& bundle) {
  std::vector<SemanticItemBinding> bindings;
  const auto* root = JsonValueAtPath(bundle.raw, "item_icon_metadata.slot_icon_bindings");
  if (root == nullptr || !root->is_array()) {
    return bindings;
  }
  for (const auto& entry : *root) {
    if (!entry.is_object()) {
      continue;
    }
    SemanticItemBinding binding;
    binding.slot_id = JsonStringAtAnyKey(entry, {"slot_id", "slotId", "id"});
    binding.icon_key = JsonStringAtAnyKey(entry, {"icon_key", "iconKey", "icon"});
    binding.render_hint = JsonStringAtAnyKey(entry, {"render_hint", "renderHint", "hint"});
    if (!binding.slot_id.empty()) {
      bindings.push_back(std::move(binding));
    }
  }
  return bindings;
}

std::string ResolveSemanticItemIcon(const TrackerBundle& bundle,
                                    const std::string& icon_key,
                                    std::int64_t stage) {
  const auto& index = PopTrackerItemImageIndex(bundle);
  if (index.empty()) {
    return {};
  }

  const auto find_candidate = [&](std::initializer_list<std::string_view> names) -> std::string {
    for (const auto name : names) {
      const auto it = index.find(CanonicalName(name));
      if (it != index.end()) {
        return it->second.generic_string();
      }
    }
    return {};
  };

  const auto canonical = CanonicalName(icon_key);
  if (canonical.find("itembow") != std::string::npos) {
    return find_candidate({stage >= 2 ? "Bow+Arrows" : "Bow"});
  }
  if (canonical.find("itemsword") != std::string::npos) {
    const auto sword_stage = std::clamp<std::int64_t>(stage, 0, 4);
    if (sword_stage <= 0) {
      return find_candidate({"sworddisabled"});
    }
    return find_candidate({"sword" + std::to_string(sword_stage)});
  }
  if (canonical.find("itemshield") != std::string::npos) {
    if (stage >= 3) return find_candidate({"Mirror_Shield"});
    if (stage >= 2) return find_candidate({"Shield_lvl2"});
    return find_candidate({"Shield_lvl1"});
  }
  if (canonical.find("itemmail") != std::string::npos) {
    if (stage >= 3) return find_candidate({"Red_Mail"});
    if (stage >= 2) return find_candidate({"Blue_Mail"});
    return find_candidate({"Green_Mail"});
  }
  if (canonical.find("itemglove") != std::string::npos) {
    if (stage >= 2) return find_candidate({"Titans_Mitt", "Titans_Mitts"});
    return find_candidate({"Power_Glove"});
  }
  if (canonical.find("itemboomerang") != std::string::npos) {
    return find_candidate({"Red_Boomerang"});
  }

  const auto leaf = icon_key.rfind('.') == std::string::npos ? icon_key : icon_key.substr(icon_key.rfind('.') + 1);
  const std::vector<std::string> candidates = {
      leaf,
      leaf + ".png",
      LowercaseCopy(leaf),
      "Magic_" + leaf,
      "Cane_of_" + leaf,
      "Book_of_" + leaf,
  };
  for (const auto& candidate : candidates) {
    const auto it = index.find(CanonicalName(candidate));
    if (it != index.end()) {
      return it->second.generic_string();
    }
  }

  for (const auto& [name, path] : index) {
    if (name.find(CanonicalName(leaf)) != std::string::npos) {
      return path.generic_string();
    }
  }
  return {};
}

const std::unordered_map<std::string, PinPlacement>& PopTrackerGroupPlacements(const TrackerBundle& bundle) {
  static std::unordered_map<std::string, std::unordered_map<std::string, PinPlacement>> cache;
  const auto cache_key = bundle.bundle_root.string();
  const auto found = cache.find(cache_key);
  if (found != cache.end()) {
    return found->second;
  }

  auto& placements = cache[cache_key];
  std::unordered_map<std::string, std::vector<std::tuple<std::string, double, double>>> by_group_name;
  std::unordered_map<std::string, std::vector<std::tuple<std::string, double, double>>> by_section_name;

  const fs::path locations_root = bundle.bundle_root / "poptracker-adapted" / "locations";
  std::error_code ec;
  if (fs::exists(locations_root, ec)) {
    for (const auto& entry : fs::directory_iterator(locations_root, ec)) {
      if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") {
        continue;
      }
      nlohmann::json raw;
      try {
        raw = nlohmann::json::parse(ReadTextFile(entry.path()));
      } catch (const std::exception&) {
        continue;
      }
      if (!raw.is_array()) {
        continue;
      }
      for (const auto& group : raw) {
        if (!group.is_object()) {
          continue;
        }
        const auto group_name = JsonStringAtAnyKey(group, {"name", "label", "title"});
        std::vector<std::tuple<std::string, double, double>> coords;
        CollectMapLocations(group, coords);
        if (!group_name.empty() && !coords.empty()) {
          auto& bucket = by_group_name[CanonicalName(group_name)];
          bucket.insert(bucket.end(), coords.begin(), coords.end());
        }
        const auto* children = JsonValueAtPath(group, "children");
        if (children == nullptr || !children->is_array()) {
          continue;
        }
        for (const auto& child : *children) {
          const auto child_name = JsonStringAtAnyKey(child, {"name", "label", "title"});
          std::vector<std::tuple<std::string, double, double>> child_coords;
          CollectMapLocations(child, child_coords);
          if (!child_name.empty() && !child_coords.empty()) {
            auto& bucket = by_section_name[CanonicalName(child_name)];
            bucket.insert(bucket.end(), child_coords.begin(), child_coords.end());
          }
        }
      }
    }
  }

  const auto* check_groups = JsonValueAtPath(bundle.raw, "surface_inventory.check_groups");
  if (check_groups == nullptr || !check_groups->is_array()) {
    return placements;
  }

  for (const auto& group : *check_groups) {
    if (!group.is_object()) {
      continue;
    }
    const auto group_id = JsonStringAtAnyKey(group, {"group_id", "groupId", "id"});
    if (group_id.empty()) {
      continue;
    }
    std::vector<std::tuple<std::string, double, double>> coords;
    if (const auto source_group_name =
            JsonStringAtAnyKey(group, {"source_group_name", "sourceGroupName"});
        !source_group_name.empty()) {
      if (const auto it = by_group_name.find(CanonicalName(source_group_name)); it != by_group_name.end()) {
        coords = it->second;
      }
    }
    if (coords.empty()) {
      const auto* examples = JsonValueAtPath(group, "source_examples");
      if (examples != nullptr && examples->is_array()) {
        for (const auto& example : *examples) {
          if (!example.is_string()) {
            continue;
          }
          const auto it = by_section_name.find(CanonicalName(example.get<std::string>()));
          if (it == by_section_name.end()) {
            continue;
          }
          coords.insert(coords.end(), it->second.begin(), it->second.end());
        }
      }
    }
    if (coords.empty()) {
      continue;
    }

    std::unordered_map<std::string, std::size_t> map_votes;
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (const auto& [map_name, x, y] : coords) {
      ++map_votes[map_name];
      sum_x += x;
      sum_y += y;
    }
    const auto dominant =
        std::max_element(map_votes.begin(), map_votes.end(),
                         [](const auto& left, const auto& right) { return left.second < right.second; });
    if (dominant == map_votes.end()) {
      continue;
    }
    const auto map_id = ResolvePopTrackerMapId(bundle, dominant->first);
    if (map_id.empty()) {
      continue;
    }
    placements[group_id] = PinPlacement{
        .map_id = map_id,
        .x = sum_x / static_cast<double>(coords.size()),
        .y = sum_y / static_cast<double>(coords.size()),
    };
  }
  return placements;
}

}  // namespace sekaiemu::spike::tracker_overlay_state_detail
