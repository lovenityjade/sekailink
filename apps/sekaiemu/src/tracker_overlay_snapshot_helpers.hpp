#pragma once

#include "tracker_overlay_render_state.hpp"

#include <cstdint>
#include <string>
#include <unordered_set>

namespace sekaiemu::spike::tracker_overlay_snapshot_detail {

void AppendMetadataEntriesAt(nlohmann::json::array_t& output,
                             const nlohmann::json& root,
                             std::initializer_list<const char*> paths);
void AddStateIdsFromArray(const nlohmann::json& snapshot,
                          std::unordered_set<std::string>& ids,
                          std::initializer_list<const char*> paths,
                          std::initializer_list<const char*> id_keys);
const nlohmann::json* SnapshotArrayAtAnyPath(const nlohmann::json& snapshot,
                                             std::initializer_list<const char*> paths);
std::int64_t JsonIntAtAnyKey(const nlohmann::json& root,
                             std::initializer_list<const char*> keys,
                             std::int64_t fallback = 0);
bool JsonBoolAtAnyKey(const nlohmann::json& root,
                      std::initializer_list<const char*> keys,
                      bool fallback = false);

}  // namespace sekaiemu::spike::tracker_overlay_snapshot_detail
