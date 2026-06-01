#pragma once

#include "adapter_context.hpp"

namespace sekaiemu::spike::bridge_adapters {

bool InjectAlttpItem(const AdapterContext& context,
                     std::uint32_t item_code,
                     std::uint32_t player_code,
                     const std::string& label);

}  // namespace sekaiemu::spike::bridge_adapters
