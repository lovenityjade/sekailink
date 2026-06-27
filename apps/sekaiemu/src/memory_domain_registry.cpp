#include "memory_domain_registry.hpp"

#include "host_io_utils.hpp"
#include "libretro_core_utils.hpp"

#include <algorithm>

namespace sekaiemu::spike {

namespace {

std::string NormalizeMemoryDomain(std::string_view memory_domain) {
  std::string out = Lowercase(memory_domain);
  for (char& ch : out) {
    if (ch == ' ' || ch == '-' || ch == '.') {
      ch = '_';
    }
  }
  return out;
}

}  // namespace

void MemoryDomainRegistry::SetMemoryMaps(const retro_memory_map* map) {
  memory_descriptors_.clear();
  if (map && map->descriptors && map->num_descriptors > 0) {
    memory_descriptors_.assign(map->descriptors, map->descriptors + map->num_descriptors);
  }
}

const std::vector<retro_memory_descriptor>& MemoryDomainRegistry::Descriptors() const {
  return memory_descriptors_;
}

const std::uint8_t* MemoryDomainRegistry::Resolve(const CoreApi& core,
                                                  std::string_view memory_domain,
                                                  std::uint32_t start,
                                                  std::uint32_t length) const {
  if (!core.retro_get_memory_data || !core.retro_get_memory_size) {
    return nullptr;
  }

  const auto region_name = NormalizeMemoryDomain(memory_domain);
  if (region_name == "system_ram" || region_name == "ram" || region_name == "wram" ||
      region_name == "save_ram" || region_name == "battery_ram" || region_name == "sram" ||
      region_name == "cart_ram" || region_name == "cartram" ||
      region_name == "video_ram" || region_name == "vram") {
    unsigned region_id = RETRO_MEMORY_SYSTEM_RAM;
    if (region_name == "save_ram" || region_name == "battery_ram" || region_name == "sram" ||
        region_name == "cart_ram" || region_name == "cartram") {
      region_id = RETRO_MEMORY_SAVE_RAM;
    } else if (region_name == "video_ram" || region_name == "vram") {
      region_id = RETRO_MEMORY_VIDEO_RAM;
    }
    auto* base = static_cast<const std::uint8_t*>(core.retro_get_memory_data(region_id));
    const std::size_t size = core.retro_get_memory_size(region_id);
    if (!base) {
      return nullptr;
    }
    if (start + length <= size) {
      return base + start;
    }
  }

  if (region_name == "gba_system_bus") {
    auto* base = static_cast<const std::uint8_t*>(core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    if (!base) {
      return nullptr;
    }

    constexpr std::uint32_t kEwramBase = 0x02000000u;
    constexpr std::uint32_t kEwramSize = 0x00040000u;
    constexpr std::uint32_t kIwramBase = 0x03000000u;
    constexpr std::uint32_t kIwramSize = 0x00008000u;
    constexpr std::uint32_t kCombinedSize = kEwramSize + kIwramSize;

    if (start >= kEwramBase && start + length <= kEwramBase + kEwramSize) {
      return base + (start - kEwramBase);
    }
    if (start >= kIwramBase && start + length <= kIwramBase + kIwramSize) {
      return base + kEwramSize + (start - kIwramBase);
    }
    if (start + length <= kCombinedSize) {
      return base + start;
    }
  }

  auto resolve_via_descriptor = [&](const retro_memory_descriptor& descriptor) -> const std::uint8_t* {
    if (!descriptor.ptr) {
      return nullptr;
    }

    const std::uint64_t address = start;
    const std::uint64_t end = address + length;
    if (end < address) {
      return nullptr;
    }

    const std::uint64_t desc_start = descriptor.start;
    const std::uint64_t desc_select = descriptor.select;
    const std::uint64_t desc_disconnect = descriptor.disconnect;
    const std::uint64_t desc_len = descriptor.len;
    const std::uint64_t desc_offset = descriptor.offset;

    auto map_one = [&](std::uint64_t in_address, std::uint64_t& out_physical) -> bool {
      if ((in_address & desc_select) != desc_start) {
        return false;
      }

      if (in_address < desc_start) {
        return false;
      }

      std::uint64_t relative = in_address - desc_start;

      if (desc_disconnect != 0) {
        std::uint64_t compressed = 0;
        std::uint64_t out_bit = 1;
        for (std::uint64_t in_bit = 1; in_bit != 0; in_bit <<= 1) {
          if (desc_disconnect & in_bit) {
            continue;
          }
          if (relative & in_bit) {
            compressed |= out_bit;
          }
          out_bit <<= 1;
        }
        relative = compressed;
      }

      if (desc_len != 0) {
        relative %= desc_len;
      }

      out_physical = desc_offset + relative;
      return true;
    };

    std::uint64_t first = 0;
    std::uint64_t last = 0;
    if (!map_one(address, first) || !map_one(end - 1, last)) {
      return nullptr;
    }

    if (last < first || (last - first + 1) != length) {
      return nullptr;
    }

    return static_cast<const std::uint8_t*>(descriptor.ptr) + first;
  };

  if (!memory_descriptors_.empty()) {
    for (const auto& descriptor : memory_descriptors_) {
      const std::string descriptor_space =
          descriptor.addrspace ? NormalizeMemoryDomain(descriptor.addrspace) : std::string();

      const bool domain_matches =
          region_name == "bus" ||
          region_name == "n64_system_bus" ||
          region_name == "system_bus" ||
          (!region_name.empty() && descriptor_space == region_name);

      if (!domain_matches) {
        continue;
      }

      if (const auto* resolved = resolve_via_descriptor(descriptor)) {
        return resolved;
      }
    }

    if (region_name == "bus" || region_name == "n64_system_bus" || region_name == "system_bus") {
      for (const auto& descriptor : memory_descriptors_) {
        if (const auto* resolved = resolve_via_descriptor(descriptor)) {
          return resolved;
        }
      }
    }
  }

  return nullptr;
}

std::uint8_t* MemoryDomainRegistry::ResolveMutable(CoreApi& core,
                                                   std::string_view memory_domain,
                                                   std::uint32_t start,
                                                   std::uint32_t length) const {
  return const_cast<std::uint8_t*>(Resolve(core, memory_domain, start, length));
}

}  // namespace sekaiemu::spike
