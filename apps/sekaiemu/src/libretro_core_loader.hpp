#pragma once

#include "libretro_core_api.hpp"

#include <filesystem>
#include <string>

namespace sekaiemu::spike {

bool LoadLibretroCore(const std::filesystem::path& core_path,
                      void*& core_handle,
                      CoreApi& api,
                      std::string& error);

void UnloadLibretroCore(void*& core_handle);

}  // namespace sekaiemu::spike
