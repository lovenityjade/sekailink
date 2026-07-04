#include "oot_adapter.hpp"

#include <initializer_list>
#include <iomanip>
#include <sstream>

namespace sekaiemu::spike::bridge_adapters {

bool InjectOotItem(const AdapterContext& context,
                   std::uint32_t item_code,
                   const std::string& label) {
  auto read_u16 = [&](std::initializer_list<std::uint32_t> addresses,
                      std::uint16_t& out_value,
                      std::uint32_t& resolved_address) -> bool {
    for (const auto address : addresses) {
      for (const char* domain : {"n64_system_bus", "system_ram"}) {
        const WatchRegion region{domain, address, 2};
        const auto bytes = context.read_region(region);
        if (bytes.size() != 2) {
          continue;
        }
        out_value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8) |
                                               static_cast<std::uint16_t>(bytes[1]));
        resolved_address = address;
        return true;
      }
    }
    return false;
  };

  auto write_u16 = [&](std::initializer_list<std::uint32_t> addresses,
                       std::uint16_t value) -> bool {
    for (const auto address : addresses) {
      for (const char* domain : {"n64_system_bus", "system_ram"}) {
        auto* target = context.resolve_mutable(domain, address, 2);
        if (!target) {
          continue;
        }
        target[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        target[1] = static_cast<std::uint8_t>(value & 0xFFu);
        return true;
      }
    }
    return false;
  };

  bool ok = false;
  if (item_code == 0x3Bu) {
    std::uint16_t equipment = 0;
    std::uint32_t resolved_address = 0;
    if (!read_u16({0x11A66Cu, 0x8011A66Cu}, equipment, resolved_address)) {
      context.append_log("inject_error|oot_equipment_unreadable");
      return false;
    }
    equipment |= 0x0001u;
    ok = write_u16({resolved_address, 0x11A66Cu, 0x8011A66Cu}, equipment);
  } else {
    context.append_log("inject_error|oot_unsupported_item");
    return false;
  }

  if (!ok) {
    context.append_log("inject_error|oot_write_failed");
    return false;
  }

  std::ostringstream event;
  event << "inject|item|0x" << std::hex << std::uppercase << item_code << std::dec;
  if (!label.empty()) {
    event << "|" << label;
  }
  context.append_log(event.str());

  std::ostringstream trace;
  trace << "[sekaiemu] injected OOT item"
        << " value=0x" << std::hex << std::uppercase << item_code << std::dec;
  if (!label.empty()) {
    trace << " label=" << label;
  }
  context.trace(trace.str());
  return true;
}

}  // namespace sekaiemu::spike::bridge_adapters
