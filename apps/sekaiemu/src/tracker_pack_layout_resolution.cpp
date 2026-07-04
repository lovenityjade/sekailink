#include "tracker_pack_layout_engine.hpp"

#include "tracker_overlay_snapshot_helpers.hpp"
#include "tracker_pack_layout_json.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace sekaiemu::spike::tracker_pack_layout_detail {
namespace {

using tracker_overlay_snapshot_detail::SnapshotArrayAtAnyPath;

const nlohmann::json* SnapshotPins(const TrackerRuntime& runtime) {
  return SnapshotArrayAtAnyPath(
      runtime.AuthoritativeState().snapshot,
      {"pins_detailed", "pinsDetailed", "tracker.pins_detailed", "tracker.pinsDetailed",
       "tracker_state.pins_detailed", "pins", "map.pins", "tracker_state.pins"});
}

std::string SnapshotPackMapName(const nlohmann::json& pin) {
  return JsonStringAtAnyKey(pin, {"pack_map", "packMap", "poptracker_map", "poptrackerMap"});
}

template <typename Value>
std::string MostVotedString(const std::unordered_map<std::string, Value>& votes) {
  const auto winner =
      std::max_element(votes.begin(), votes.end(), [](const auto& left, const auto& right) {
        return left.second < right.second;
      });
  return winner == votes.end() ? std::string{} : winner->first;
}

}  // namespace

bool ResolvePackMapId(const TrackerBundle& bundle, std::string_view pack_name, std::string* out_map_id) {
  const auto wanted = CanonicalToken(pack_name);
  for (const auto& map : bundle.maps) {
    const auto raw_name = JsonStringAtAnyKey(map.raw, {"poptracker_map_name", "pack_map_name", "packMapName"});
    if (!raw_name.empty() && CanonicalToken(raw_name) == wanted) {
      *out_map_id = map.id;
      return true;
    }
    if (CanonicalToken(map.label) == wanted || CanonicalToken(map.id) == wanted) {
      *out_map_id = map.id;
      return true;
    }
  }
  return false;
}

bool ResolveContextPackMapId(const PackStateContext& context,
                             std::string_view pack_name,
                             std::string* out_map_id) {
  const auto wanted = CanonicalToken(pack_name);
  if (wanted.empty()) {
    return false;
  }
  const auto* pins = SnapshotPins(context.runtime);
  if (pins != nullptr && pins->is_array()) {
    std::unordered_map<std::string, int> votes;
    for (const auto& pin : *pins) {
      if (!pin.is_object() || CanonicalToken(SnapshotPackMapName(pin)) != wanted) {
        continue;
      }
      const auto map_id = JsonStringAtAnyKey(pin, {"map_id", "mapId", "map"});
      if (!map_id.empty()) {
        ++votes[map_id];
      }
    }
    const auto winner = MostVotedString(votes);
    if (!winner.empty()) {
      *out_map_id = winner;
      return true;
    }
  }
  return context.bundle != nullptr && ResolvePackMapId(*context.bundle, pack_name, out_map_id);
}

std::string ResolveContextPackMapAsset(const PackStateContext& context,
                                       std::string_view map_id,
                                       std::string_view pack_name) {
  const auto wanted = CanonicalToken(pack_name);
  std::unordered_map<std::string, int> votes;
  if (const auto* pins = SnapshotPins(context.runtime); pins != nullptr && pins->is_array()) {
    for (const auto& pin : *pins) {
      if (!pin.is_object()) {
        continue;
      }
      if (!wanted.empty() && CanonicalToken(SnapshotPackMapName(pin)) != wanted) {
        continue;
      }
      if (!map_id.empty() && JsonStringAtAnyKey(pin, {"map_id", "mapId", "map"}) != map_id) {
        continue;
      }
      auto asset = JsonStringAtAnyKey(pin, {"map_asset", "mapAsset", "image", "map_image", "mapImage"});
      if (!asset.empty()) {
        ++votes[NormalizePackAssetPath(asset)];
      }
    }
  }
  if (const auto asset = MostVotedString(votes); !asset.empty()) {
    return asset;
  }

  const auto* pack_maps = JsonValueAtPath(context.runtime.AuthoritativeState().snapshot, "pack_maps");
  if (pack_maps == nullptr || !pack_maps->is_array() || wanted.empty()) {
    return {};
  }
  for (const auto& map : *pack_maps) {
    if (!map.is_object() || CanonicalToken(JsonStringAtAnyKey(map, {"name", "id"})) != wanted) {
      continue;
    }
    auto asset = JsonStringAtAnyKey(map, {"image", "asset", "map_asset", "mapAsset"});
    return asset.empty() ? std::string{} : NormalizePackAssetPath(asset);
  }
  return {};
}

std::string ResolveDominantContextPackMapName(const PackStateContext& context,
                                              std::string_view map_id) {
  std::unordered_map<std::string, int> votes;
  if (const auto* pins = SnapshotPins(context.runtime); pins != nullptr && pins->is_array()) {
    for (const auto& pin : *pins) {
      if (!pin.is_object()) {
        continue;
      }
      if (!map_id.empty() && JsonStringAtAnyKey(pin, {"map_id", "mapId", "map"}) != map_id) {
        continue;
      }
      const auto pack_map = SnapshotPackMapName(pin);
      if (!pack_map.empty()) {
        ++votes[pack_map];
      }
    }
  }
  return MostVotedString(votes);
}

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
