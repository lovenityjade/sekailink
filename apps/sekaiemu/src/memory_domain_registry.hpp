#pragma once

#include "libretro_core_api.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

class MemoryDomainRegistry {
 public:
  void SetMemoryMaps(const retro_memory_map* map);

  const std::vector<retro_memory_descriptor>& Descriptors() const;

  const std::uint8_t* Resolve(const CoreApi& core,
                              std::string_view memory_domain,
                              std::uint32_t start,
                              std::uint32_t length) const;

  std::uint8_t* ResolveMutable(CoreApi& core,
                               std::string_view memory_domain,
                               std::uint32_t start,
                               std::uint32_t length) const;

 private:
  std::vector<retro_memory_descriptor> memory_descriptors_;
};

}  // namespace sekaiemu::spike
