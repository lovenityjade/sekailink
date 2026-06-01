#include "libretro_memory_tools.hpp"

#include "host_io_utils.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <system_error>

namespace sekaiemu::spike {
namespace {

void LogMemoryRegion(const CoreApi& core, const char* label, unsigned id) {
  if (!core.retro_get_memory_data || !core.retro_get_memory_size) {
    return;
  }

  void* data = core.retro_get_memory_data(id);
  const std::size_t size = core.retro_get_memory_size(id);
  std::cerr << "[sekaiemu-libretro-spike] memory[" << label << "]"
            << " ptr=" << data
            << " size=" << size;
  if (data && size > 0) {
    std::cerr << " preview=" << HexPreview(static_cast<const std::uint8_t*>(data), std::min<std::size_t>(size, 16));
  }
  std::cerr << "\n";
}

bool WriteMemoryRegionDump(const CoreApi& core,
                           const std::filesystem::path& prefix,
                           const char* label,
                           unsigned id) {
  if (!core.retro_get_memory_data || !core.retro_get_memory_size) {
    return false;
  }

  void* data = core.retro_get_memory_data(id);
  const std::size_t size = core.retro_get_memory_size(id);
  if (!data || size == 0) {
    return false;
  }

  const auto output_path = prefix.string() + "_" + label + ".bin";
  std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    std::cerr << "[sekaiemu-libretro-spike] dump failed: could not open "
              << output_path << "\n";
    return false;
  }

  stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  if (!stream) {
    std::cerr << "[sekaiemu-libretro-spike] dump failed: could not write "
              << output_path << "\n";
    return false;
  }
  return true;
}

}  // namespace

void ProbeMemoryRegions(const CoreApi& core, const MemoryDomainRegistry& memory_domains) {
  LogMemoryRegion(core, "system_ram", RETRO_MEMORY_SYSTEM_RAM);
  LogMemoryRegion(core, "save_ram", RETRO_MEMORY_SAVE_RAM);
  LogMemoryRegion(core, "video_ram", RETRO_MEMORY_VIDEO_RAM);

  if (!memory_domains.Descriptors().empty()) {
    const std::size_t preview_count = std::min<std::size_t>(memory_domains.Descriptors().size(), 4);
    for (std::size_t index = 0; index < preview_count; ++index) {
      const auto& descriptor = memory_domains.Descriptors()[index];
      std::cerr << "[sekaiemu-libretro-spike] mmap[" << index << "]"
                << " start=0x" << std::hex << descriptor.start
                << " select=0x" << descriptor.select
                << " disconnect=0x" << descriptor.disconnect
                << " len=0x" << descriptor.len
                << " offset=0x" << descriptor.offset
                << " flags=0x" << descriptor.flags
                << std::dec << "\n";
    }
  }
}

void DumpMemorySnapshot(const CoreApi& core,
                        const std::filesystem::path& save_directory,
                        std::size_t& dump_counter) {
  const auto dump_root = save_directory / "dumps";
  std::error_code ec;
  std::filesystem::create_directories(dump_root, ec);
  if (ec) {
    std::cerr << "[sekaiemu-libretro-spike] dump failed: could not create "
              << dump_root << ": " << ec.message() << "\n";
    return;
  }

  const auto dump_id = ++dump_counter;
  const auto prefix = dump_root / ("snapshot_" + std::to_string(dump_id));
  bool wrote_anything = false;
  wrote_anything |= WriteMemoryRegionDump(core, prefix, "system_ram", RETRO_MEMORY_SYSTEM_RAM);
  wrote_anything |= WriteMemoryRegionDump(core, prefix, "save_ram", RETRO_MEMORY_SAVE_RAM);
  wrote_anything |= WriteMemoryRegionDump(core, prefix, "video_ram", RETRO_MEMORY_VIDEO_RAM);

  if (wrote_anything) {
    std::cerr << "[sekaiemu-libretro-spike] memory snapshot written: "
              << prefix << "_*.bin\n";
  } else {
    std::cerr << "[sekaiemu-libretro-spike] memory snapshot skipped: no readable regions\n";
  }
}

}  // namespace sekaiemu::spike
