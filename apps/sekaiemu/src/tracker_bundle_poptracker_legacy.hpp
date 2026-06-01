#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {

// Transitional visual fallback for legacy Sekaiemu bundles. BETA-3 PopTracker
// logic remains owned by SKLMI; do not extend this into runtime logic.
void ApplyPopTrackerAdaptedMaps(nlohmann::json& raw,
                                const std::filesystem::path& bundle_root);

}  // namespace sekaiemu::spike
