#pragma once

#include "adapter_context.hpp"

namespace sekaiemu::spike::bridge_adapters {

bool InjectFireRedItem(const AdapterContext& context,
                       std::uint32_t item_code,
                       bool should_display,
                       const std::string& label);

}  // namespace sekaiemu::spike::bridge_adapters
