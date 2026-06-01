#pragma once

#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {

class HostTrackerAssetResolver final : public TrackerOverlayAssetResolver {
 public:
  void SetRoots(std::vector<std::filesystem::path> roots);
  bool HasRoots() const;

  std::optional<TrackerOverlayResolvedAsset> ResolveTrackerAsset(
      std::string_view bundle_relative_path) const override;

 private:
  std::vector<std::filesystem::path> roots_;
  mutable std::unordered_map<std::string, TrackerRasterImage> cache_;
  mutable std::unordered_set<std::string> failed_;
};

}  // namespace sekaiemu::spike
