#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include <libretro.h>

namespace sekaiemu::spike {

bool WriteFramePpm(const std::filesystem::path& path,
                   const void* data,
                   unsigned width,
                   unsigned height,
                   std::size_t pitch,
                   retro_pixel_format pixel_format,
                   std::string& error);

void MaybeDumpFrame(const void* data,
                    unsigned width,
                    unsigned height,
                    std::size_t pitch,
                    retro_pixel_format pixel_format,
                    const std::filesystem::path& configured_path,
                    const std::filesystem::path& save_directory,
                    std::uint64_t frame_counter,
                    std::uint64_t dump_at_frame,
                    bool& dump_written);

}  // namespace sekaiemu::spike
