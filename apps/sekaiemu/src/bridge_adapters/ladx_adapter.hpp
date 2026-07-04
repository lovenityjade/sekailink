#pragma once

#include "adapter_context.hpp"

namespace sekaiemu::spike::bridge_adapters {

bool InjectLadxItem(const AdapterContext& context,
                    std::uint32_t item_code,
                    std::uint32_t sender_code,
                    const std::string& label);

}  // namespace sekaiemu::spike::bridge_adapters
