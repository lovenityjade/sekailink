#pragma once

#include "libretro_core_api.hpp"
#include "save_state_manager.hpp"
#include "tracker_runtime.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {

bool SaveStateSlotNow(SaveStateManager& save_state_manager,
                      CoreApi& core,
                      const TrackerRuntime& tracker_runtime,
                      std::string_view game_name,
                      std::uint64_t frame_counter,
                      int slot,
                      std::optional<int>& pending_screenshot_slot,
                      nlohmann::json& pending_metadata);

bool LoadStateSlotNow(SaveStateManager& save_state_manager, CoreApi& core, int slot);

}  // namespace sekaiemu::spike
