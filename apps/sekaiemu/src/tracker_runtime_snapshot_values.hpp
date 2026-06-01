#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike::tracker_runtime_values {

std::string JsonString(const nlohmann::json& value,
                       std::initializer_list<const char*> keys);
std::string JsonScalarToString(const nlohmann::json& value);
const nlohmann::json* JsonValueAtPath(const nlohmann::json& root, std::string_view path);

std::size_t SnapshotArraySize(const nlohmann::json& snapshot,
                              std::initializer_list<const char*> keys);
const nlohmann::json* SnapshotLastArrayEntry(const nlohmann::json& snapshot,
                                             std::initializer_list<const char*> keys);
std::string SnapshotEntryLabel(const nlohmann::json& snapshot,
                               std::initializer_list<const char*> array_keys,
                               std::initializer_list<const char*> label_keys,
                               std::initializer_list<const char*> id_keys,
                               std::initializer_list<const char*> mapping_keys);
std::string SnapshotEntryString(const nlohmann::json& snapshot,
                                std::initializer_list<const char*> array_keys,
                                std::initializer_list<const char*> value_keys);

}  // namespace sekaiemu::spike::tracker_runtime_values
