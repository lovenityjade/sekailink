#include "tracker_overlay_snapshot_helpers.hpp"

namespace sekaiemu::spike::tracker_overlay_snapshot_detail {

void AppendMetadataEntriesAt(nlohmann::json::array_t& output,
                             const nlohmann::json& root,
                             std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    const auto* value = JsonValueAtPath(root, path);
    if (value == nullptr) {
      continue;
    }
    if (value->is_array()) {
      for (const auto& entry : *value) {
        output.push_back(entry);
      }
      continue;
    }
    if (!value->is_object()) {
      continue;
    }
    for (auto it = value->begin(); it != value->end(); ++it) {
      if (!it.value().is_object()) {
        continue;
      }
      auto entry = it.value();
      if (!entry.contains("id")) {
        entry["id"] = it.key();
      }
      output.push_back(std::move(entry));
    }
  }
}

void AddStateIdsFromArray(const nlohmann::json& snapshot,
                          std::unordered_set<std::string>& ids,
                          std::initializer_list<const char*> paths,
                          std::initializer_list<const char*> id_keys) {
  for (const char* path : paths) {
    const auto* value = JsonValueAtPath(snapshot, path);
    if (value == nullptr || !value->is_array()) {
      continue;
    }
    for (const auto& entry : *value) {
      if (entry.is_object()) {
        const auto id = JsonStringAtAnyKey(entry, id_keys);
        if (!id.empty()) {
          ids.insert(id);
        }
      } else if (const auto id = JsonScalarToText(entry); !id.empty()) {
        ids.insert(id);
      }
    }
  }
}

const nlohmann::json* SnapshotArrayAtAnyPath(const nlohmann::json& snapshot,
                                             std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    const auto* value = JsonValueAtPath(snapshot, path);
    if (value != nullptr && value->is_array()) {
      return value;
    }
  }
  return nullptr;
}

std::int64_t JsonIntAtAnyKey(const nlohmann::json& root,
                             std::initializer_list<const char*> keys,
                             std::int64_t fallback) {
  if (!root.is_object()) {
    return fallback;
  }
  for (const char* key : keys) {
    const auto it = root.find(key);
    if (it == root.end()) {
      continue;
    }
    if (it->is_number_integer()) {
      return it->get<std::int64_t>();
    }
    if (it->is_number_unsigned()) {
      return static_cast<std::int64_t>(it->get<std::uint64_t>());
    }
    if (it->is_string()) {
      try {
        return std::stoll(it->get<std::string>());
      } catch (const std::exception&) {
      }
    }
  }
  return fallback;
}

bool JsonBoolAtAnyKey(const nlohmann::json& root,
                      std::initializer_list<const char*> keys,
                      bool fallback) {
  if (!root.is_object()) {
    return fallback;
  }
  for (const char* key : keys) {
    const auto it = root.find(key);
    if (it == root.end()) {
      continue;
    }
    if (it->is_boolean()) {
      return it->get<bool>();
    }
    if (it->is_number_integer()) {
      return it->get<std::int64_t>() != 0;
    }
    if (it->is_string()) {
      const auto text = it->get<std::string>();
      if (text == "true" || text == "1" || text == "yes" || text == "on") {
        return true;
      }
      if (text == "false" || text == "0" || text == "no" || text == "off") {
        return false;
      }
    }
  }
  return fallback;
}

}  // namespace sekaiemu::spike::tracker_overlay_snapshot_detail
