#pragma once

#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <string>
#include <vector>

namespace sekaiemu::spike {

enum class TrackerPackHitTargetKind {
  Item,
  Pin,
};

struct TrackerPackHitCheck {
  std::string location_id;
  std::string label;
  bool checked = false;
};

struct TrackerPackHitTarget {
  TrackerPackHitTargetKind kind = TrackerPackHitTargetKind::Item;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  std::string code;
  std::string pin_id;
  std::string location_id;
  std::string group_id;
  std::string label;
  std::vector<TrackerPackHitCheck> checks;
};

std::vector<TrackerPackHitTarget> BuildPackLayoutHitTargets(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    int x,
    int y,
    int width,
    int height,
    const TrackerOverlayAssetResolver* asset_resolver = nullptr);

const TrackerPackHitTarget* FindPackLayoutHitTarget(
    const std::vector<TrackerPackHitTarget>& targets,
    int point_x,
    int point_y);

}  // namespace sekaiemu::spike
