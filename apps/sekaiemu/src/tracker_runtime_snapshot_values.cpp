#include "tracker_runtime_snapshot_values.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace sekaiemu::spike::tracker_runtime_values {
namespace {

std::string TrimCopy(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::optional<std::size_t> JsonArrayIndexFromToken(std::string_view token) {
  if (token.empty()) {
    return std::nullopt;
  }
  std::string_view numeric = token;
  if (token.front() == '[' && token.back() == ']' && token.size() > 2) {
    numeric = token.substr(1, token.size() - 2);
  }
  if (!std::all_of(numeric.begin(), numeric.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::stoul(std::string(numeric)));
}

std::string SnapshotMappedLabel(const nlohmann::json& snapshot,
                                std::int64_t canonical_id,
                                std::initializer_list<const char*> mapping_keys) {
  const std::string id_key = std::to_string(canonical_id);
  for (const char* key : mapping_keys) {
    const auto it = snapshot.find(key);
    if (it == snapshot.end() || !it->is_object()) {
      continue;
    }
    const auto label_it = it->find(id_key);
    if (label_it != it->end()) {
      const auto rendered = JsonScalarToString(*label_it);
      if (!rendered.empty()) {
        return rendered;
      }
    }
  }
  return {};
}

}  // namespace

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

std::string JsonScalarToString(const nlohmann::json& value) {
  if (value.is_string()) {
    return TrimCopy(value.get<std::string>());
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? "ON" : "OFF";
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<std::int64_t>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<std::uint64_t>());
  }
  if (value.is_number_float()) {
    std::ostringstream out;
    out << value.get<double>();
    return out.str();
  }
  if (value.is_null()) {
    return {};
  }
  return value.dump();
}

const nlohmann::json* JsonValueAtPath(const nlohmann::json& root, std::string_view path) {
  if (path.empty()) {
    return &root;
  }
  const nlohmann::json* current = &root;
  std::size_t start = 0;
  while (start < path.size()) {
    const std::size_t dot = path.find('.', start);
    const std::string_view token =
        path.substr(start, dot == std::string_view::npos ? path.size() - start : dot - start);
    if (current->is_array()) {
      const auto index = JsonArrayIndexFromToken(token);
      if (!index.has_value() || *index >= current->size()) {
        return nullptr;
      }
      current = &(*current)[*index];
    } else if (current->is_object()) {
      const std::string key(token);
      const auto it = current->find(key);
      if (it == current->end()) {
        return nullptr;
      }
      current = &*it;
    } else {
      return nullptr;
    }
    if (dot == std::string_view::npos) {
      return current;
    }
    start = dot + 1;
  }
  return current;
}

std::size_t SnapshotArraySize(const nlohmann::json& snapshot, std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto* value = JsonValueAtPath(snapshot, key);
    if (value != nullptr && value->is_array()) {
      return value->size();
    }
  }
  return 0;
}

const nlohmann::json* SnapshotLastArrayEntry(const nlohmann::json& snapshot,
                                             std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto* value = JsonValueAtPath(snapshot, key);
    if (value == nullptr || !value->is_array() || value->empty()) {
      continue;
    }
    return &value->back();
  }
  return nullptr;
}

std::string SnapshotEntryLabel(const nlohmann::json& snapshot,
                               std::initializer_list<const char*> array_keys,
                               std::initializer_list<const char*> label_keys,
                               std::initializer_list<const char*> id_keys,
                               std::initializer_list<const char*> mapping_keys) {
  const auto* entry = SnapshotLastArrayEntry(snapshot, array_keys);
  if (entry == nullptr) {
    return {};
  }
  if (entry->is_object()) {
    const auto label = JsonString(*entry, label_keys);
    if (!label.empty()) {
      return label;
    }
    if (const auto id = JsonString(*entry, id_keys); !id.empty()) {
      try {
        const auto mapped = SnapshotMappedLabel(snapshot, std::stoll(id), mapping_keys);
        if (!mapped.empty()) {
          return mapped;
        }
      } catch (const std::exception&) {
      }
      return id;
    }
    return JsonScalarToString(*entry);
  }
  if (entry->is_number_integer()) {
    const auto canonical_id = entry->get<std::int64_t>();
    if (const auto mapped = SnapshotMappedLabel(snapshot, canonical_id, mapping_keys); !mapped.empty()) {
      return mapped;
    }
  }
  return JsonScalarToString(*entry);
}

std::string SnapshotEntryString(const nlohmann::json& snapshot,
                                std::initializer_list<const char*> array_keys,
                                std::initializer_list<const char*> value_keys) {
  const auto* entry = SnapshotLastArrayEntry(snapshot, array_keys);
  if (entry == nullptr || !entry->is_object()) {
    return {};
  }
  return JsonString(*entry, value_keys);
}

}  // namespace sekaiemu::spike::tracker_runtime_values
