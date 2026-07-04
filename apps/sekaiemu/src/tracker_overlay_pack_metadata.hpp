#pragma once

#include "tracker_runtime.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike::tracker_overlay_state_detail {

struct SemanticItemBinding {
  std::string slot_id;
  std::string icon_key;
  std::string render_hint;
};

struct PinPlacement {
  std::string map_id;
  double x = 0.0;
  double y = 0.0;
};

std::vector<SemanticItemBinding> LoadSemanticItemBindings(const TrackerBundle& bundle);
std::string ResolveSemanticItemIcon(const TrackerBundle& bundle,
                                    const std::string& icon_key,
                                    std::int64_t stage);
const std::unordered_map<std::string, PinPlacement>& PopTrackerGroupPlacements(const TrackerBundle& bundle);

}  // namespace sekaiemu::spike::tracker_overlay_state_detail
