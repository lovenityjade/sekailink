#include "tracker_overlay_render_state.hpp"

#include "tracker_overlay_pack_metadata.hpp"
#include "tracker_overlay_snapshot_helpers.hpp"

#include <algorithm>
#include <unordered_map>

namespace sekaiemu::spike {

using namespace tracker_overlay_state_detail;
using namespace tracker_overlay_snapshot_detail;

namespace {

bool JsonHasAnyKey(const nlohmann::json& root,
                   std::initializer_list<const char*> keys) {
  if (!root.is_object()) {
    return false;
  }
  for (const char* key : keys) {
    if (root.contains(key)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<BundlePinSegmentRenderMetadata> ParsePinSegments(const nlohmann::json& entry,
                                                             std::string_view fallback_color) {
  std::vector<BundlePinSegmentRenderMetadata> segments;
  const auto segment_it = entry.find("segments");
  if (segment_it == entry.end() || !segment_it->is_array()) {
    return segments;
  }
  for (const auto& segment_entry : *segment_it) {
    if (!segment_entry.is_object()) {
      continue;
    }
    BundlePinSegmentRenderMetadata segment;
    segment.section_id = JsonStringAtAnyKey(segment_entry, {"section_id", "sectionId", "id"});
    segment.label = JsonStringAtAnyKey(segment_entry, {"label", "name", "title"});
    segment.color = JsonStringAtAnyKey(segment_entry, {"color", "state_color", "stateColor"});
    if (segment.color.empty()) {
      segment.color = std::string(fallback_color);
    }
    segment.checked_count = JsonIntAtAnyKey(segment_entry, {"checked_count", "checkedCount"}, 0);
    segment.total_count = JsonIntAtAnyKey(segment_entry, {"total_count", "totalCount", "count"}, 0);
    segment.checked = JsonBoolAtAnyKey(segment_entry, {"checked", "cleared"}, false) ||
                      (segment.total_count > 0 && segment.checked_count >= segment.total_count) ||
                      segment.color == "black";
    segment.mixed = JsonBoolAtAnyKey(segment_entry, {"mixed"}, false) ||
                    (segment.checked_count > 0 && segment.total_count > segment.checked_count);
    if (const auto checks = segment_entry.find("checks"); checks != segment_entry.end() && checks->is_array()) {
      for (const auto& check_entry : *checks) {
        if (!check_entry.is_object()) {
          continue;
        }
        BundlePinSegmentRenderMetadata::Check check;
        check.location_id = JsonStringAtAnyKey(check_entry, {"location_id", "locationId", "id"});
        if (check.location_id.empty()) {
          if (const auto numeric_id = JsonNumberAtAnyKey(check_entry, {"location_id", "locationId", "id"});
              numeric_id.has_value()) {
            check.location_id = std::to_string(static_cast<std::uint64_t>(*numeric_id));
          }
        }
        check.label = JsonStringAtAnyKey(check_entry, {"label", "name", "title"});
        check.checked = JsonBoolAtAnyKey(check_entry, {"checked", "cleared"}, false);
        if (!check.location_id.empty()) {
          segment.checks.push_back(std::move(check));
        }
      }
    }
    if (segment.total_count <= 0) {
      segment.total_count = 1;
    }
    segments.push_back(std::move(segment));
  }
  return segments;
}

std::unordered_set<std::string> ReceivedItemIds(const TrackerRuntime& runtime) {
  std::unordered_set<std::string> ids;
  for (const auto id : runtime.AuthoritativeState().received_items) {
    ids.insert(std::to_string(id));
  }
  for (const auto id : runtime.ObservedState().locally_received_items) {
    ids.insert(std::to_string(id));
  }
  AddStateIdsFromArray(runtime.AuthoritativeState().snapshot,
                       ids,
                       {"received_items", "receivedItems", "items.received",
                        "items.received_items", "item_state.received"},
                       {"item_id", "itemId", "canonical_id", "canonicalId", "id"});
  return ids;
}

std::unordered_set<std::string> CheckedLocationIds(const TrackerRuntime& runtime) {
  std::unordered_set<std::string> ids;
  for (const auto id : runtime.AuthoritativeState().checked_locations) {
    ids.insert(std::to_string(id));
  }
  for (const auto id : runtime.ObservedState().locally_checked_locations) {
    ids.insert(std::to_string(id));
  }
  AddStateIdsFromArray(runtime.AuthoritativeState().snapshot,
                       ids,
                       {"checked_locations", "checkedLocations", "locations.checked",
                        "location_state.checked"},
                       {"location_id", "locationId", "canonical_id", "canonicalId", "id"});
  return ids;
}

std::vector<BundleItemRenderMetadata> BuildBundleItems(const TrackerRuntime& runtime) {
  const auto* bundle = runtime.Bundle();
  const auto semantic_bindings = bundle != nullptr ? LoadSemanticItemBindings(*bundle)
                                                   : std::vector<SemanticItemBinding>{};
  if (const auto* snapshot_items = SnapshotArrayAtAnyPath(
          runtime.AuthoritativeState().snapshot, {"items", "tracker.items", "tracker_state.items"});
      snapshot_items != nullptr) {
    std::vector<BundleItemRenderMetadata> items;
    std::unordered_map<std::string, SemanticItemBinding> bindings_by_slot;
    for (const auto& binding : semantic_bindings) {
      bindings_by_slot[binding.slot_id] = binding;
    }
    for (const auto& entry : *snapshot_items) {
      if (!entry.is_object()) {
        continue;
      }
      BundleItemRenderMetadata item;
      item.id = JsonStringAtAnyKey(entry, {"id", "item_id", "itemId", "code", "name"});
      item.code = JsonStringAtAnyKey(entry, {"code", "primary_code", "primaryCode"});
      item.pack_visual_code = JsonStringAtAnyKey(
          entry, {"pack_visual_code", "packVisualCode", "visual_code", "visualCode"});
      item.label = JsonStringAtAnyKey(entry, {"label", "name", "title"});
      item.abbreviation =
          JsonStringAtAnyKey(entry, {"abbr", "abbreviation", "short", "short_label", "shortLabel"});
      item.icon = JsonStringAtAnyKey(entry, {"icon", "image", "image_path", "imagePath"});
      item.stage = JsonIntAtAnyKey(entry, {"stage_index", "stageIndex", "visual_stage", "visualStage",
                                           "stage", "level", "value"},
                                   0);
      item.count = JsonIntAtAnyKey(entry, {"count", "amount", "qty"}, 0);
      item.has_explicit_state = JsonHasAnyKey(entry, {"acquired", "active", "owned", "checked"});
      item.acquired = item.has_explicit_state
                          ? JsonBoolAtAnyKey(entry, {"acquired", "active", "owned", "checked"}, false)
                          : (item.stage > 0 || item.count > 0);
      if (const auto it = bindings_by_slot.find(item.id); it != bindings_by_slot.end()) {
        if (item.icon.empty() && bundle != nullptr) {
          item.icon = ResolveSemanticItemIcon(*bundle, it->second.icon_key, item.stage);
        }
      }
      if (item.id.empty()) {
        continue;
      }
      if (item.label.empty()) {
        item.label = item.id;
      }
      if (item.abbreviation.empty()) {
        item.abbreviation = item.label.substr(0, std::min<std::size_t>(2, item.label.size()));
      }
      items.push_back(std::move(item));
    }
    if (!items.empty()) {
      return items;
    }
  }

  if (bundle == nullptr) {
    return {};
  }

  nlohmann::json::array_t entries;
  AppendMetadataEntriesAt(entries,
                          bundle->raw,
                          {"item_icon_metadata", "itemIconMetadata", "item_icons", "itemIcons",
                           "item_metadata", "itemMetadata", "items", "tracker_flow.items",
                           "trackerFlow.items", "tracker-flow.items"});

  std::vector<BundleItemRenderMetadata> items;
  for (const auto& entry : entries) {
    if (!entry.is_object()) {
      continue;
    }
    BundleItemRenderMetadata item;
    item.id =
        JsonStringAtAnyKey(entry, {"id", "item_id", "itemId", "canonical_id", "canonicalId"});
    item.code = JsonStringAtAnyKey(entry, {"code", "primary_code", "primaryCode"});
    item.pack_visual_code = JsonStringAtAnyKey(
        entry, {"pack_visual_code", "packVisualCode", "visual_code", "visualCode"});
    item.label = JsonStringAtAnyKey(entry, {"label", "name", "title"});
    item.abbreviation =
        JsonStringAtAnyKey(entry, {"abbr", "abbreviation", "short", "short_label", "shortLabel"});
    item.icon = JsonStringAtAnyKey(entry, {"icon", "image", "image_path", "imagePath"});
    if (item.id.empty()) {
      continue;
    }
    if (item.label.empty()) {
      item.label = item.id;
    }
    if (item.abbreviation.empty()) {
      item.abbreviation = item.label.substr(0, std::min<std::size_t>(2, item.label.size()));
    }
    items.push_back(std::move(item));
  }
  return items;
}

std::vector<BundlePinRenderMetadata> BuildBundlePins(const TrackerRuntime& runtime,
                                                     const TrackerResolvedViewState& resolved) {
  if (const auto* snapshot_pins = SnapshotArrayAtAnyPath(
          runtime.AuthoritativeState().snapshot,
          {"pins_detailed", "pinsDetailed", "tracker.pins_detailed", "tracker.pinsDetailed",
           "tracker_state.pins_detailed", "pins", "map.pins", "tracker_state.pins"});
      snapshot_pins != nullptr) {
    std::vector<BundlePinRenderMetadata> pins;
    const auto* bundle = runtime.Bundle();
    const auto bundle_placements =
        bundle != nullptr ? PopTrackerGroupPlacements(*bundle)
                          : std::unordered_map<std::string, PinPlacement>{};
    for (const auto& entry : *snapshot_pins) {
      if (!entry.is_object()) {
        continue;
      }
      BundlePinRenderMetadata pin;
      pin.id = JsonStringAtAnyKey(entry, {"id", "pin_id", "pinId", "canonical_id", "canonicalId"});
      pin.location_id = JsonStringAtAnyKey(entry, {"location_id", "locationId", "check_id",
                                                   "checkId", "canonical_location_id",
                                                   "canonicalLocationId"});
      pin.group_id = JsonStringAtAnyKey(entry, {"group_id", "groupId"});
      pin.map_id = JsonStringAtAnyKey(entry, {"map_id", "mapId", "map"});
      pin.pack_map = JsonStringAtAnyKey(entry, {"pack_map", "packMap", "poptracker_map", "poptrackerMap"});
      pin.map_asset = JsonStringAtAnyKey(entry, {"map_asset", "mapAsset", "image", "map_image", "mapImage"});
      pin.label = JsonStringAtAnyKey(entry, {"label", "name", "title"});
      pin.color =
          JsonStringAtAnyKey(entry, {"color", "state_color", "stateColor", "pin_color", "pinColor"});
      pin.segments = ParsePinSegments(entry, pin.color);
      pin.checked = JsonBoolAtAnyKey(entry, {"checked", "cleared", "acquired"}, false) ||
                    pin.color == "black";
      pin.has_explicit_checked = entry.contains("checked") || entry.contains("cleared") ||
                                 entry.contains("acquired") || !pin.color.empty();
      pin.size = JsonNumberAtAnyKey(entry, {"size", "radius", "pin_size", "pinSize"}).value_or(0.0);
      const auto x = JsonNumberAtAnyKey(entry, {"x", "left", "map_x", "mapX"});
      const auto y = JsonNumberAtAnyKey(entry, {"y", "top", "map_y", "mapY"});
      pin.has_position = x.has_value() && y.has_value();
      if (pin.has_position) {
        pin.x = *x;
        pin.y = *y;
      } else {
        const auto placement_it = bundle_placements.find(pin.id);
        if (placement_it != bundle_placements.end()) {
          pin.has_position = true;
          pin.x = placement_it->second.x;
          pin.y = placement_it->second.y;
          if (pin.map_id.empty()) {
            pin.map_id = placement_it->second.map_id;
          }
        }
      }
      if (pin.location_id.empty()) {
        pin.location_id = pin.id;
      }
      if (pin.id.empty()) {
        pin.id = pin.location_id;
      }
      if (pin.label.empty()) {
        pin.label = pin.id;
      }
      if (pin.id.empty() || !pin.has_position) {
        continue;
      }
      pins.push_back(std::move(pin));
    }
    if (!pins.empty()) {
      return pins;
    }
  }

  const auto* bundle = runtime.Bundle();
  if (bundle == nullptr) {
    return {};
  }

  nlohmann::json::array_t entries;
  AppendMetadataEntriesAt(entries,
                          bundle->raw,
                          {"map_pin_metadata", "mapPinMetadata", "map_pins", "mapPins",
                           "location_pin_metadata", "locationPinMetadata", "location_pins",
                           "locationPins", "pins", "checks", "locations", "tracker_flow.pins",
                           "trackerFlow.pins", "tracker-flow.pins"});
  if (const auto* active_map = ResolveActiveMap(runtime, resolved); active_map != nullptr) {
    AppendMetadataEntriesAt(entries,
                            active_map->raw,
                            {"map_pin_metadata", "mapPinMetadata", "map_pins", "mapPins",
                             "location_pins", "locationPins", "pins", "checks", "locations"});
  }

  std::vector<BundlePinRenderMetadata> pins;
  for (const auto& entry : entries) {
    if (!entry.is_object()) {
      continue;
    }
    BundlePinRenderMetadata pin;
    pin.id = JsonStringAtAnyKey(entry, {"id", "pin_id", "pinId", "canonical_id", "canonicalId"});
    pin.location_id = JsonStringAtAnyKey(entry, {"location_id", "locationId", "check_id",
                                                 "checkId", "canonical_location_id",
                                                 "canonicalLocationId"});
    pin.group_id = JsonStringAtAnyKey(entry, {"group_id", "groupId"});
    pin.map_id = JsonStringAtAnyKey(entry, {"map_id", "mapId", "map"});
    pin.pack_map = JsonStringAtAnyKey(entry, {"pack_map", "packMap", "poptracker_map", "poptrackerMap"});
    pin.map_asset = JsonStringAtAnyKey(entry, {"map_asset", "mapAsset", "image", "map_image", "mapImage"});
    pin.label = JsonStringAtAnyKey(entry, {"label", "name", "title"});
    pin.size = JsonNumberAtAnyKey(entry, {"size", "radius", "pin_size", "pinSize"}).value_or(0.0);
    const auto x = JsonNumberAtAnyKey(entry, {"x", "left", "map_x", "mapX"});
    const auto y = JsonNumberAtAnyKey(entry, {"y", "top", "map_y", "mapY"});
    pin.has_position = x.has_value() && y.has_value();
    if (pin.has_position) {
      pin.x = *x;
      pin.y = *y;
    }
    if (pin.location_id.empty()) {
      pin.location_id = pin.id;
    }
    if (pin.id.empty()) {
      pin.id = pin.location_id;
    }
    if (pin.label.empty()) {
      pin.label = pin.id;
    }
    if (pin.id.empty() || !pin.has_position) {
      continue;
    }
    if (!pin.map_id.empty() && pin.map_id != resolved.active_map_id) {
      continue;
    }
    pins.push_back(std::move(pin));
  }
  return pins;
}

}  // namespace sekaiemu::spike
