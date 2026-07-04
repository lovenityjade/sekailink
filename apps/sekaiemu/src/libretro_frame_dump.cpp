#include "libretro_frame_dump.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

namespace sekaiemu::spike {
namespace {

std::filesystem::path ResolveFrameDumpPath(const std::filesystem::path& configured_path,
                                           const std::filesystem::path& save_directory,
                                           std::uint64_t frame_counter) {
  if (!configured_path.empty()) {
    return configured_path;
  }
  std::ostringstream filename;
  filename << "frame-" << std::setw(8) << std::setfill('0') << frame_counter << ".ppm";
  return save_directory / "dumps" / filename.str();
}

}  // namespace

bool WriteFramePpm(const std::filesystem::path& path,
                   const void* data,
                   unsigned width,
                   unsigned height,
                   std::size_t pitch,
                   retro_pixel_format pixel_format,
                   std::string& error) {
  if (!data || width == 0 || height == 0) {
    error = "No frame is available for screenshot.";
    return false;
  }

  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "Failed to create screenshot directory: " + ec.message();
    return false;
  }

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    error = "Failed to open screenshot file for writing.";
    return false;
  }

  output << "P6\n" << width << " " << height << "\n255\n";
  const auto* source = static_cast<const std::uint8_t*>(data);
  for (unsigned y = 0; y < height; ++y) {
    const auto* row = source + static_cast<std::size_t>(y) * pitch;
    for (unsigned x = 0; x < width; ++x) {
      std::uint8_t rgb[3]{0, 0, 0};
      switch (pixel_format) {
        case RETRO_PIXEL_FORMAT_RGB565: {
          const auto pixel = static_cast<std::uint16_t>(
              row[x * 2u] | (static_cast<std::uint16_t>(row[x * 2u + 1u]) << 8u));
          rgb[0] = static_cast<std::uint8_t>(((pixel >> 11u) & 0x1fu) * 255u / 31u);
          rgb[1] = static_cast<std::uint8_t>(((pixel >> 5u) & 0x3fu) * 255u / 63u);
          rgb[2] = static_cast<std::uint8_t>((pixel & 0x1fu) * 255u / 31u);
          break;
        }
        case RETRO_PIXEL_FORMAT_0RGB1555: {
          const auto pixel = static_cast<std::uint16_t>(
              row[x * 2u] | (static_cast<std::uint16_t>(row[x * 2u + 1u]) << 8u));
          rgb[0] = static_cast<std::uint8_t>(((pixel >> 10u) & 0x1fu) * 255u / 31u);
          rgb[1] = static_cast<std::uint8_t>(((pixel >> 5u) & 0x1fu) * 255u / 31u);
          rgb[2] = static_cast<std::uint8_t>((pixel & 0x1fu) * 255u / 31u);
          break;
        }
        case RETRO_PIXEL_FORMAT_XRGB8888:
        default:
          rgb[0] = row[x * 4u + 2u];
          rgb[1] = row[x * 4u + 1u];
          rgb[2] = row[x * 4u + 0u];
          break;
      }
      output.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    }
  }
  if (!output) {
    error = "Failed to write screenshot file.";
    return false;
  }
  return true;
}

void MaybeDumpFrame(const void* data,
                    unsigned width,
                    unsigned height,
                    std::size_t pitch,
                    retro_pixel_format pixel_format,
                    const std::filesystem::path& configured_path,
                    const std::filesystem::path& save_directory,
                    std::uint64_t frame_counter,
                    std::uint64_t dump_at_frame,
                    bool& dump_written) {
  if (dump_written || dump_at_frame == 0 ||
      frame_counter < dump_at_frame || !data || width == 0 || height == 0) {
    return;
  }

  const auto path = ResolveFrameDumpPath(configured_path, save_directory, frame_counter);
  std::string error;
  if (!WriteFramePpm(path, data, width, height, pitch, pixel_format, error)) {
    std::cerr << "[sekaiemu] frame dump failed: " << error << "\n";
    dump_written = true;
    return;
  }
  dump_written = true;
  std::cerr << "[sekaiemu] frame dumped: " << path
            << " frame=" << frame_counter
            << " size=" << width << "x" << height << "\n";
}

}  // namespace sekaiemu::spike
