#pragma once

#include "libretro_core_api.hpp"
#include "memory_domain_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace sekaiemu::spike {

void ProbeMemoryRegions(const CoreApi& core, const MemoryDomainRegistry& memory_domains);
void DumpMemorySnapshot(const CoreApi& core,
                        const std::filesystem::path& save_directory,
                        std::size_t& dump_counter);

}  // namespace sekaiemu::spike
