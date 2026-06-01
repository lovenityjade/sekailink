#pragma once

#include "tracker_pack_layout_model.hpp"

#include <nlohmann/json.hpp>

namespace sekaiemu::spike::tracker_pack_layout_detail {

void LoadSnapshotVisualDefinitions(PackStateContext& context);
bool LoadSnapshotLayoutDefinitions(const nlohmann::json& snapshot, PackLayoutDocument& document);
bool SnapshotProvidesPackVisuals(const TrackerRuntime& runtime);
const PackLayoutDocument& PackLayoutForBundle(const TrackerBundle& bundle, bool include_visual_fallbacks);

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
