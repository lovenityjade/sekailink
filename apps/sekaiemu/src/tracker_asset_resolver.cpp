#include "tracker_asset_resolver.hpp"

#include <exception>
#include <system_error>
#include <utility>

namespace sekaiemu::spike {

void HostTrackerAssetResolver::SetRoots(std::vector<std::filesystem::path> roots) {
  roots_.clear();
  for (auto& root : roots) {
    if (!root.empty()) {
      roots_.push_back(std::move(root));
    }
  }
  cache_.clear();
  failed_.clear();
}

bool HostTrackerAssetResolver::HasRoots() const {
  return !roots_.empty();
}

std::optional<TrackerOverlayResolvedAsset> HostTrackerAssetResolver::ResolveTrackerAsset(
    std::string_view bundle_relative_path) const {
  if (bundle_relative_path.empty() || roots_.empty()) {
    return std::nullopt;
  }
  const std::string key(bundle_relative_path);
  if (const auto cached = cache_.find(key); cached != cache_.end()) {
    return TrackerOverlayResolvedAsset{
        cached->second.width,
        cached->second.height,
        cached->second.rgba_pixels.data(),
    };
  }
  if (failed_.contains(key)) {
    return std::nullopt;
  }

  std::filesystem::path relative_path(key);
  while (!relative_path.empty() && relative_path.is_absolute()) {
    relative_path = relative_path.relative_path();
  }
  for (const auto& root : roots_) {
    const auto full_path = root / relative_path;
    std::error_code ec;
    if (!std::filesystem::exists(full_path, ec) || ec) {
      continue;
    }
    try {
      auto image = LoadTrackerRasterAsset(full_path);
      const auto [it, inserted] = cache_.emplace(key, std::move(image));
      (void)inserted;
      return TrackerOverlayResolvedAsset{
          it->second.width,
          it->second.height,
          it->second.rgba_pixels.data(),
      };
    } catch (const std::exception&) {
      continue;
    }
  }

  failed_.insert(key);
  return std::nullopt;
}

}  // namespace sekaiemu::spike
