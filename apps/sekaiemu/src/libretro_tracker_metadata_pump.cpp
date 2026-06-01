#include "libretro_tracker_metadata_pump.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

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

std::unordered_map<std::string, std::string> ParseKeyValueDetail(std::string_view detail) {
  std::unordered_map<std::string, std::string> fields;
  std::istringstream input{std::string(detail)};
  std::string token;
  while (input >> token) {
    const auto delimiter = token.find('=');
    if (delimiter == std::string::npos || delimiter == 0) {
      continue;
    }
    auto key = TrimCopy(token.substr(0, delimiter));
    auto value = TrimCopy(token.substr(delimiter + 1));
    if (key.empty() || value.empty()) {
      continue;
    }
    fields[std::move(key)] = std::move(value);
  }
  return fields;
}

void AssignRoomMetadataValue(nlohmann::json& room_metadata,
                             const std::string& key,
                             const std::string& value) {
  if (key.empty() || value.empty()) {
    return;
  }
  try {
    room_metadata[key] = nlohmann::json::parse(value);
  } catch (const std::exception&) {
    room_metadata[key] = value;
  }
}

}  // namespace

bool PumpTrackerSnapshot(const std::filesystem::path& snapshot_path,
                         TrackerSnapshotReader& snapshot_reader,
                         TrackerRuntime& tracker_runtime) {
  if (snapshot_path.empty()) {
    return false;
  }
  const auto poll = snapshot_reader.Poll(snapshot_path);
  if (!poll.changed) {
    return false;
  }
  if (!poll.ok) {
    std::cerr << "[sekaiemu-libretro-spike] tracker snapshot ignored: "
              << poll.error << "\n";
    return false;
  }

  const auto previous = tracker_runtime.AuthoritativeState().snapshot;
  tracker_runtime.ApplyServerSnapshot(poll.snapshot);
  if (previous != poll.snapshot) {
    std::cerr << "[sekaiemu-libretro-spike] tracker snapshot updated:"
              << " path=" << snapshot_path
              << " revision=" << poll.snapshot.value("revision", 0)
              << " schema=" << poll.snapshot.value("schema", std::string{"unknown"})
              << "\n";
  }
  return true;
}

bool PumpTrackerRoomMetadata(const std::filesystem::path& room_state_path,
                             std::optional<std::filesystem::file_time_type>& last_write_time,
                             TrackerRuntime& tracker_runtime) {
  if (room_state_path.empty()) {
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::exists(room_state_path, ec) || ec) {
    return false;
  }

  const auto write_time = std::filesystem::last_write_time(room_state_path, ec);
  if (ec) {
    return false;
  }
  if (last_write_time.has_value() && *last_write_time == write_time) {
    return false;
  }

  std::ifstream input(room_state_path, std::ios::binary);
  if (!input) {
    return false;
  }

  nlohmann::json seed_metadata = nlohmann::json::object();
  nlohmann::json room_metadata = nlohmann::json::object();
  std::string world_id;
  std::string linkedworld_id;
  std::string seed_id;
  std::string slot_id;
  std::string slot_name;
  std::string player_alias;
  std::string room_id;
  std::string room_name;
  std::string room_type;
  std::string session_id;

  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind("meta|", 0) != 0) {
      continue;
    }
    const auto delimiter = line.find('|', 5);
    if (delimiter == std::string::npos) {
      continue;
    }
    const auto key = TrimCopy(line.substr(5, delimiter - 5));
    const auto value = TrimCopy(line.substr(delimiter + 1));
    if (key.empty()) {
      continue;
    }

    if (key == "world_id" || key == "world_instance_id") {
      world_id = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "linkedworld_id") {
      linkedworld_id = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "seed_id") {
      seed_id = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "room_slot_id" || key == "slot_id") {
      slot_id = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "slot_name") {
      slot_name = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "player_alias") {
      player_alias = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "room_id") {
      room_id = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "room_name") {
      room_name = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "room_type") {
      room_type = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "session_id") {
      session_id = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "seed_hash" || key == "tracker_pack" || key == "tracker_variant") {
      seed_metadata[key] = value;
      AssignRoomMetadataValue(room_metadata, key, value);
    } else if (key == "slot_data") {
      try {
        const auto parsed = nlohmann::json::parse(value);
        if (parsed.is_object()) {
          seed_metadata["slot_data"] = parsed;
          room_metadata["slot_data"] = parsed;
          if (slot_id.empty()) {
            const auto slot = parsed.find("slot");
            if (slot != parsed.end()) {
              if (slot->is_string()) {
                slot_id = slot->get<std::string>();
              } else if (slot->is_number_integer()) {
                slot_id = std::to_string(slot->get<std::int64_t>());
              }
            }
          }
          if (slot_name.empty()) {
            const auto name = parsed.find("name");
            if (name != parsed.end() && name->is_string()) {
              slot_name = name->get<std::string>();
            }
          }
        } else {
          seed_metadata["slot_data"] = value;
          room_metadata["slot_data"] = value;
        }
      } catch (const std::exception&) {
        seed_metadata["slot_data_raw"] = value;
        room_metadata["slot_data_raw"] = value;
      }
    } else {
      AssignRoomMetadataValue(room_metadata, key, value);
    }
  }

  nlohmann::json snapshot = nlohmann::json::object();
  if (!world_id.empty()) {
    snapshot["world_instance_id"] = world_id;
  }
  if (!linkedworld_id.empty()) {
    snapshot["linkedworld_id"] = linkedworld_id;
  }
  if (!seed_id.empty()) {
    snapshot["seed_id"] = seed_id;
  }
  if (!slot_id.empty()) {
    snapshot["slot_id"] = slot_id;
  }
  if (!slot_name.empty()) {
    snapshot["slot_name"] = slot_name;
  }
  if (!player_alias.empty()) {
    snapshot["player_alias"] = player_alias;
  }
  if (!room_id.empty()) {
    snapshot["room_id"] = room_id;
  }
  if (!room_name.empty()) {
    snapshot["room_name"] = room_name;
  }
  if (!room_type.empty()) {
    snapshot["room_type"] = room_type;
  }
  if (!session_id.empty()) {
    snapshot["session_id"] = session_id;
  }
  if (!seed_metadata.empty()) {
    snapshot["seed_metadata"] = seed_metadata;
  }
  if (!room_metadata.empty()) {
    snapshot["room_metadata"] = room_metadata;
  }
  if (!snapshot.empty()) {
    const auto previous = tracker_runtime.AuthoritativeState().snapshot;
    tracker_runtime.ApplyServerSnapshot(snapshot);
    if (previous != snapshot) {
      std::cerr << "[sekaiemu-libretro-spike] tracker room metadata updated:"
                << " room_state=" << room_state_path
                << " linkedworld=" << (linkedworld_id.empty() ? "-" : linkedworld_id)
                << " room=" << (room_id.empty() ? "-" : room_id)
                << " slot=" << (slot_id.empty() ? "-" : slot_id)
                << " slot_name=" << (slot_name.empty() ? "-" : slot_name)
                << " player_alias=" << (player_alias.empty() ? "-" : player_alias)
                << " seed=" << (seed_id.empty() ? "-" : seed_id)
                << "\n";
    }
  }

  last_write_time = write_time;
  return !snapshot.empty();
}

bool PumpTrackerTraceEvents(const std::filesystem::path& trace_path,
                            std::uintmax_t& trace_offset,
                            std::unordered_map<std::string, std::string>& item_label_by_key,
                            TrackerRuntime& tracker_runtime) {
  if (trace_path.empty() || !std::filesystem::exists(trace_path)) {
    return false;
  }

  const auto file_size = std::filesystem::file_size(trace_path);
  if (trace_offset > file_size) {
    trace_offset = 0;
  }

  std::ifstream input(trace_path, std::ios::binary);
  if (!input) {
    return false;
  }
  input.seekg(static_cast<std::streamoff>(trace_offset), std::ios::beg);
  bool changed = false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      const auto record = nlohmann::json::parse(line);
      if (!record.is_object()) {
        continue;
      }
      const auto record_type = record.value("record_type", std::string());
      if (record_type == "event") {
        auto tracker_event = record;
        const auto event_type = tracker_event.value("event_type", std::string());
        if (event_type == "item_received") {
          const auto has_label =
              tracker_event.contains("label") || tracker_event.contains("item_name") ||
              tracker_event.contains("itemName") || tracker_event.contains("mapped_value") ||
              tracker_event.contains("mappedValue");
          std::vector<std::string> lookup_keys;
          if (const auto key = tracker_event.value("key", std::string{}); !key.empty()) {
            lookup_keys.push_back(key);
          }
          if (tracker_event.contains("canonical_id") && tracker_event["canonical_id"].is_number_integer()) {
            lookup_keys.push_back(std::to_string(tracker_event["canonical_id"].get<std::int64_t>()));
          }
          if (tracker_event.contains("item_id") && tracker_event["item_id"].is_number_integer()) {
            lookup_keys.push_back(std::to_string(tracker_event["item_id"].get<std::int64_t>()));
          }
          for (const auto& key : lookup_keys) {
            const auto label_it = item_label_by_key.find(key);
            if (label_it == item_label_by_key.end() || label_it->second.empty()) {
              continue;
            }
            if (!has_label) {
              tracker_event["item_name"] = label_it->second;
            }
            tracker_event["label"] = label_it->second;
            break;
          }
        }
        tracker_runtime.ApplySklmiEvent(tracker_event);
        if (event_type == "slot_connected") {
          nlohmann::json snapshot = nlohmann::json::object();
          const auto linkedworld_id = tracker_event.value("linkedworld_id", std::string{});
          const auto driver_instance_id = tracker_event.value("driver_instance_id", std::string{});
          const auto core_profile = tracker_event.value("core_profile", std::string{});
          if (!linkedworld_id.empty()) {
            snapshot["linkedworld_id"] = linkedworld_id;
          }
          if (!driver_instance_id.empty() || !core_profile.empty()) {
            nlohmann::json room_metadata = nlohmann::json::object();
            if (!driver_instance_id.empty()) {
              room_metadata["driver_instance_id"] = driver_instance_id;
            }
            if (!core_profile.empty()) {
              room_metadata["core_profile"] = core_profile;
            }
            snapshot["room_metadata"] = std::move(room_metadata);
          }
          if (!snapshot.empty()) {
            tracker_runtime.ApplyServerSnapshot(snapshot);
          }
        }
        changed = true;
        continue;
      }
      if (record_type == "trace") {
        const auto event = record.value("event", std::string{});
        const auto detail = record.value("detail", std::string{});
        nlohmann::json snapshot = nlohmann::json::object();
        if (event == "room_item_pending") {
          const auto fields = ParseKeyValueDetail(detail);
          const auto item_name_it = fields.find("item_name");
          if (item_name_it != fields.end() && !item_name_it->second.empty() &&
              item_name_it->second != "None") {
            for (const char* key_name : {"event_key", "mapped_value", "ap_item_id", "delivery_id"}) {
              const auto key_it = fields.find(key_name);
              if (key_it != fields.end() && !key_it->second.empty()) {
                item_label_by_key[key_it->second] = item_name_it->second;
              }
            }
          }
        } else if (event == "room_client_ready") {
          const auto fields = ParseKeyValueDetail(detail);
          if (const auto it = fields.find("session_name"); it != fields.end()) {
            snapshot["room_id"] = it->second;
            snapshot["room_name"] = it->second;
          }
          if (const auto it = fields.find("slot_id"); it != fields.end()) {
            snapshot["slot_id"] = it->second;
          }
        } else if (event == "room_metadata_ready") {
          const auto fields = ParseKeyValueDetail(detail);
          if (const auto it = fields.find("world_id"); it != fields.end() &&
              it->second != "missing") {
            snapshot["world_instance_id"] = it->second;
          }
          if (const auto it = fields.find("seed_id"); it != fields.end() &&
              it->second != "missing") {
            snapshot["seed_id"] = it->second;
          }
          nlohmann::json seed_metadata = nlohmann::json::object();
          if (const auto it = fields.find("seed_hash"); it != fields.end() &&
              it->second != "missing") {
            seed_metadata["seed_hash"] = it->second;
          }
          if (const auto it = fields.find("slot_data"); it != fields.end() &&
              it->second.rfind("present(", 0) == 0) {
            seed_metadata["slot_data_present"] = true;
          }
          if (!seed_metadata.empty()) {
            snapshot["seed_metadata"] = std::move(seed_metadata);
          }
        }
        if (!snapshot.empty()) {
          tracker_runtime.ApplyServerSnapshot(snapshot);
          changed = true;
        }
        continue;
      }
    } catch (const std::exception&) {
    }
  }
  const auto position = input.tellg();
  if (position >= 0) {
    trace_offset = static_cast<std::uintmax_t>(position);
  } else {
    trace_offset = file_size;
  }
  return changed;
}

}  // namespace sekaiemu::spike
