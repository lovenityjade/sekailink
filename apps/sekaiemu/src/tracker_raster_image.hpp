#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace sekaiemu::spike {

struct TrackerRasterImage {
  unsigned width = 0;
  unsigned height = 0;
  std::vector<std::uint8_t> rgba_pixels;
};

TrackerRasterImage LoadTrackerRasterAsset(const std::filesystem::path& path,
                                          bool image_transparent = false);

}  // namespace sekaiemu::spike
