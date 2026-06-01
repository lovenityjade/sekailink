#include "tracker_overlay_render_state.hpp"

#include "tracker_overlay_snapshot_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace sekaiemu::spike {
namespace {

using namespace tracker_overlay_snapshot_detail;

const nlohmann::json* SnapshotLastArrayEntry(const nlohmann::json& snapshot,
                                             std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto* value = JsonValueAtPath(snapshot, key);
    if (value != nullptr && value->is_array() && !value->empty()) {
      return &value->back();
    }
  }
  return nullptr;
}

std::string SnapshotMappedLabel(const nlohmann::json& snapshot,
                                std::int64_t canonical_id,
                                std::initializer_list<const char*> keys) {
  const std::string id_key = std::to_string(canonical_id);
  for (const char* key : keys) {
    const auto it = snapshot.find(key);
    if (it == snapshot.end() || !it->is_object()) {
      continue;
    }
    const auto label_it = it->find(id_key);
    if (label_it != it->end()) {
      const auto rendered = JsonScalarToText(*label_it);
      if (!rendered.empty()) {
        return rendered;
      }
    }
  }
  return {};
}

std::string SnapshotEventLabel(const nlohmann::json& snapshot,
                               std::initializer_list<const char*> array_keys,
                               std::initializer_list<const char*> label_keys,
                               std::initializer_list<const char*> id_keys,
                               std::initializer_list<const char*> mapping_keys) {
  const auto* entry = SnapshotLastArrayEntry(snapshot, array_keys);
  if (entry == nullptr) {
    return {};
  }
  if (entry->is_object()) {
    for (const char* key : label_keys) {
      if (const auto value = MetadataStringAt(*entry, key); !value.empty()) {
        return value;
      }
    }
    for (const char* key : id_keys) {
      if (const auto value = MetadataStringAt(*entry, key); !value.empty()) {
        try {
          if (const auto mapped = SnapshotMappedLabel(snapshot, std::stoll(value), mapping_keys);
              !mapped.empty()) {
            return mapped;
          }
        } catch (const std::exception&) {
        }
        return value;
      }
    }
    return JsonScalarToText(*entry);
  }
  if (entry->is_number_integer()) {
    if (const auto mapped =
            SnapshotMappedLabel(snapshot, entry->get<std::int64_t>(), mapping_keys);
        !mapped.empty()) {
      return mapped;
    }
  }
  return JsonScalarToText(*entry);
}

std::string SnapshotLastEntryString(const nlohmann::json& snapshot,
                                    std::initializer_list<const char*> array_keys,
                                    std::initializer_list<const char*> value_keys) {
  const auto* entry = SnapshotLastArrayEntry(snapshot, array_keys);
  if (entry == nullptr || !entry->is_object()) {
    return {};
  }
  for (const char* key : value_keys) {
    if (const auto value = MetadataStringAt(*entry, key); !value.empty()) {
      return value;
    }
  }
  return {};
}

std::string BoolToken(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isspace(ch) == 0) {
      out.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return out;
}

std::optional<bool> JsonBoolAtPath(const nlohmann::json& snapshot, std::string_view path) {
  const auto* value = JsonValueAtPath(snapshot, path);
  if (value == nullptr) {
    return std::nullopt;
  }
  if (value->is_boolean()) {
    return value->get<bool>();
  }
  if (value->is_number_integer()) {
    return value->get<std::int64_t>() != 0;
  }
  if (value->is_number_unsigned()) {
    return value->get<std::uint64_t>() != 0;
  }
  if (value->is_string()) {
    const auto token = BoolToken(value->get<std::string>());
    if (token == "1" || token == "true" || token == "yes" || token == "on" ||
        token == "connected") {
      return true;
    }
    if (token == "0" || token == "false" || token == "no" || token == "off" ||
        token == "disconnected") {
      return false;
    }
  }
  return std::nullopt;
}

std::optional<bool> SnapshotConnectionState(const TrackerRuntime& runtime) {
  const auto& snapshot = runtime.AuthoritativeState().snapshot;
  for (std::string_view path :
       {"connected", "ap_connected", "status.ap_connected", "status.connected",
        "room_metadata.connected"}) {
    if (const auto value = JsonBoolAtPath(snapshot, path); value.has_value()) {
      return value;
    }
  }
  return std::nullopt;
}

std::string LastReceivedTimestamp(const TrackerRuntime& runtime,
                                  const TrackerResolvedViewState& resolved) {
  for (auto it = resolved.recent_events.rbegin(); it != resolved.recent_events.rend(); ++it) {
    if (it->is_object() && it->value("event_type", std::string{}) == "item_received") {
      const auto timestamp = it->value("timestamp", std::string{});
      if (!timestamp.empty()) {
        return timestamp;
      }
    }
  }
  return SnapshotLastEntryString(runtime.AuthoritativeState().snapshot,
                                 {"received_items", "receivedItems", "items.received",
                                  "items.received_items", "item_state.received"},
                                 {"timestamp", "received_at", "receivedAt", "ts"});
}

std::string LastCheckedTimestamp(const TrackerRuntime& runtime,
                                 const TrackerResolvedViewState& resolved) {
  for (auto it = resolved.recent_events.rbegin(); it != resolved.recent_events.rend(); ++it) {
    if (it->is_object() && it->value("event_type", std::string{}) == "location_checked") {
      const auto timestamp = it->value("timestamp", std::string{});
      if (!timestamp.empty()) {
        return timestamp;
      }
    }
  }
  return SnapshotLastEntryString(runtime.AuthoritativeState().snapshot,
                                 {"checked_locations", "checkedLocations", "locations.checked",
                                  "location_state.checked"},
                                 {"timestamp", "checked_at", "checkedAt", "ts"});
}

}  // namespace

std::string SnapshotStringAt(const TrackerRuntime& runtime, std::string_view path) {
  return MetadataStringAt(runtime.AuthoritativeState().snapshot, path);
}

std::size_t SnapshotArraySize(const nlohmann::json& snapshot,
                              std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto* value = JsonValueAtPath(snapshot, key);
    if (value != nullptr && value->is_array()) {
      return value->size();
    }
  }
  return 0;
}

std::string LastReceivedLabel(const TrackerRuntime& runtime,
                              const TrackerResolvedViewState& resolved) {
  for (auto it = resolved.recent_events.rbegin(); it != resolved.recent_events.rend(); ++it) {
    if (it->is_object() && it->value("event_type", std::string{}) == "item_received") {
      const auto label = it->value("label", std::string{});
      if (!label.empty()) {
        return label;
      }
    }
  }
  return SnapshotEventLabel(runtime.AuthoritativeState().snapshot,
                            {"received_items", "receivedItems", "items.received",
                             "items.received_items", "item_state.received"},
                            {"item_name", "itemName", "mapped_value", "mappedValue", "label", "value"},
                            {"item_id", "itemId", "canonical_id", "canonicalId", "id"},
                            {"item_names"});
}

std::string LastReceivedFrom(const TrackerRuntime& runtime) {
  if (const auto* entry = SnapshotLastArrayEntry(
          runtime.AuthoritativeState().snapshot,
          {"received_items", "receivedItems", "items.received", "items.received_items",
           "item_state.received"});
      entry != nullptr && entry->is_object()) {
    for (const char* key :
         {"sender_alias", "senderAlias", "from_alias", "fromAlias", "sender_slot", "senderSlot"}) {
      if (const auto value = MetadataStringAt(*entry, key); !value.empty()) {
        return value;
      }
    }
  }
  return {};
}

std::string LastCheckedLabel(const TrackerRuntime& runtime,
                             const TrackerResolvedViewState& resolved) {
  for (auto it = resolved.recent_events.rbegin(); it != resolved.recent_events.rend(); ++it) {
    if (it->is_object() && it->value("event_type", std::string{}) == "location_checked") {
      const auto label = it->value("label", std::string{});
      if (!label.empty()) {
        return label;
      }
    }
  }
  return SnapshotEventLabel(runtime.AuthoritativeState().snapshot,
                            {"checked_locations", "checkedLocations", "locations.checked",
                             "location_state.checked"},
                            {"location_name", "locationName", "mapped_value", "mappedValue", "label",
                             "value"},
                            {"location_id", "locationId", "canonical_id", "canonicalId", "id"},
                            {"location_names", "location_mapping"});
}

std::vector<RecentRenderRow> BuildRecentRows(const TrackerRuntime& runtime,
                                             const TrackerResolvedViewState& resolved) {
  bool has_item_event = false;
  bool has_check_event = false;
  for (const auto& event : resolved.recent_events) {
    if (!event.is_object()) {
      continue;
    }
    has_item_event = has_item_event || event.value("event_type", std::string{}) == "item_received";
    has_check_event =
        has_check_event || event.value("event_type", std::string{}) == "location_checked";
  }

  std::vector<RecentRenderRow> rows;
  if (!has_item_event) {
    if (const auto label = LastReceivedLabel(runtime, resolved); !label.empty()) {
      const auto sender = LastReceivedFrom(runtime);
      rows.push_back(RecentRenderRow{"item_received",
                                     label,
                                     LastReceivedTimestamp(runtime, resolved),
                                     {},
                                     sender.empty() ? std::string{} : "FROM " + sender});
    }
  }
  if (!has_check_event) {
    if (const auto label = LastCheckedLabel(runtime, resolved); !label.empty()) {
      rows.push_back(RecentRenderRow{"location_checked",
                                     label,
                                     LastCheckedTimestamp(runtime, resolved),
                                     {},
                                     {}});
    }
  }

  for (const auto& event : resolved.recent_events) {
    if (!event.is_object()) {
      continue;
    }
    rows.push_back(RecentRenderRow{
        event.value("event_type", std::string{}),
        event.value("label", std::string{"UNKNOWN"}),
        event.value("timestamp", std::string{}),
        event.contains("canonical_id") && !event["canonical_id"].is_null()
            ? event["canonical_id"].dump()
            : std::string{},
        event.value("detail", std::string{}),
    });
  }
  return rows;
}

std::size_t CheckedCount(const TrackerRuntime& runtime) {
  const auto snapshot_count = std::max<std::size_t>(
      SnapshotArraySize(runtime.AuthoritativeState().snapshot,
                        {"checked_locations", "checkedLocations", "locations.checked",
                         "location_state.checked"}),
      runtime.AuthoritativeState().checked_locations.size());
  if (const auto* summary = JsonValueAtPath(runtime.AuthoritativeState().snapshot, "summary");
      summary != nullptr && summary->is_object()) {
    return std::max<std::size_t>(
        snapshot_count, static_cast<std::size_t>(JsonIntAtAnyKey(*summary, {"checked"}, 0)));
  }
  return snapshot_count;
}

std::size_t MissingCount(const TrackerRuntime& runtime) {
  const auto snapshot_count = std::max<std::size_t>(
      SnapshotArraySize(runtime.AuthoritativeState().snapshot,
                        {"missing_locations", "missingLocations", "locations.missing",
                         "location_state.missing"}),
      runtime.AuthoritativeState().missing_locations.size());
  if (const auto* summary = JsonValueAtPath(runtime.AuthoritativeState().snapshot, "summary");
      summary != nullptr && summary->is_object()) {
    const auto total = JsonIntAtAnyKey(*summary, {"total"}, 0);
    const auto checked = JsonIntAtAnyKey(*summary, {"checked"}, 0);
    if (total > 0) {
      return std::max<std::size_t>(
          snapshot_count,
          static_cast<std::size_t>(std::max<std::int64_t>(0, total - checked)));
    }
  }
  return snapshot_count;
}

std::size_t ReceivedCount(const TrackerRuntime& runtime) {
  const auto snapshot_count = std::max<std::size_t>(
      SnapshotArraySize(runtime.AuthoritativeState().snapshot,
                        {"received_items", "receivedItems", "items.received",
                         "items.received_items", "item_state.received"}),
      runtime.AuthoritativeState().received_items.size());
  if (const auto* summary = JsonValueAtPath(runtime.AuthoritativeState().snapshot, "summary");
      summary != nullptr && summary->is_object()) {
    return std::max<std::size_t>(
        snapshot_count, static_cast<std::size_t>(JsonIntAtAnyKey(*summary, {"received"}, 0)));
  }
  return snapshot_count;
}

std::string FormatPercent(std::size_t value, std::size_t maximum) {
  const std::size_t safe_maximum = std::max<std::size_t>(maximum, 1);
  return std::to_string((value * 100) / safe_maximum) + "%";
}

std::string BuildSessionHeadline(const TrackerRuntime& runtime,
                                 const TrackerResolvedViewState& resolved) {
  const auto& authoritative = runtime.AuthoritativeState();
  const std::string slot_name = SnapshotStringAt(runtime, "slot_name");
  const std::string player_alias = SnapshotStringAt(runtime, "player_alias");
  const std::string room_id = SnapshotStringAt(runtime, "room_id");

  std::vector<std::string> parts;
  if (!slot_name.empty()) {
    parts.push_back(slot_name);
  } else if (!authoritative.slot_id.empty()) {
    parts.push_back(authoritative.slot_id);
  }
  if (!player_alias.empty()) {
    parts.push_back(player_alias);
  }
  if (!room_id.empty()) {
    parts.push_back(room_id);
  }
  if (parts.empty() && !resolved.current_zone_id.empty()) {
    parts.push_back(resolved.current_zone_id);
  }
  if (parts.empty()) {
    return "WAITING FOR LIVE ROOM METADATA";
  }

  std::string headline;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index != 0) {
      headline += " / ";
    }
    headline += parts[index];
  }
  return headline;
}

std::vector<std::string> BuildMetadataChips(const TrackerRuntime& runtime,
                                            const TrackerResolvedViewState& resolved) {
  const auto& authoritative = runtime.AuthoritativeState();
  const auto& seed_metadata = resolved.seed_metadata;
  const std::string player_alias = SnapshotStringAt(runtime, "player_alias");
  const std::string slot_name = SnapshotStringAt(runtime, "slot_name");
  const std::string room_id = SnapshotStringAt(runtime, "room_id");

  std::vector<std::string> chips;
  if (const auto connected = SnapshotConnectionState(runtime); connected.has_value()) {
    chips.push_back(*connected ? "LINK ON" : "LINK OFF");
  }
  if (!slot_name.empty()) {
    chips.push_back("NAME " + TruncateText(slot_name, 12));
  }
  if (!player_alias.empty()) {
    chips.push_back("ALIAS " + TruncateText(player_alias, 12));
  }
  if (!room_id.empty()) {
    chips.push_back("ROOM " + TruncateText(room_id, 12));
  }
  chips.push_back(
      "SLOT " + TruncateText(authoritative.slot_id.empty() ? "UNKNOWN" : authoritative.slot_id, 12));
  chips.push_back(
      "SEED " + TruncateText(authoritative.seed_id.empty() ? "UNKNOWN" : authoritative.seed_id, 12));
  chips.push_back(
      "ZONE " + TruncateText(resolved.current_zone_id.empty() ? "UNKNOWN" : resolved.current_zone_id, 14));
  chips.push_back(
      "MAP " + TruncateText(resolved.active_map_id.empty() ? "DEFAULT" : resolved.active_map_id, 14));
  if (!authoritative.world_instance_id.empty()) {
    chips.push_back("WORLD " + TruncateText(authoritative.world_instance_id, 12));
  }
  if (const auto hash = MetadataStringAt(seed_metadata, "seed_hash"); !hash.empty()) {
    chips.push_back("HASH " + TruncateText(hash, 12));
  }
  if (const auto pack = MetadataStringAt(seed_metadata, "tracker_pack"); !pack.empty()) {
    chips.push_back("PACK " + TruncateText(pack, 12));
  }
  if (const auto variant = MetadataStringAt(seed_metadata, "tracker_variant"); !variant.empty()) {
    chips.push_back("VAR " + TruncateText(variant, 12));
  }
  if (const auto mode = MetadataStringAt(seed_metadata, "slot_data.mode"); !mode.empty()) {
    chips.push_back("MODE " + TruncateText(mode, 12));
  }
  if (const auto goal = MetadataStringAt(seed_metadata, "slot_data.goal"); !goal.empty()) {
    chips.push_back("GOAL " + TruncateText(goal, 12));
  }
  if (const auto difficulty = MetadataStringAt(seed_metadata, "slot_data.difficulty");
      !difficulty.empty()) {
    chips.push_back("DIFF " + TruncateText(difficulty, 12));
  }
  if (const auto weapons = MetadataStringAt(seed_metadata, "slot_data.weapons"); !weapons.empty()) {
    chips.push_back("WEAP " + TruncateText(weapons, 12));
  }
  return chips;
}

}  // namespace sekaiemu::spike
