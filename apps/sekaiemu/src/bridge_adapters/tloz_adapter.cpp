#include "tloz_adapter.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sekaiemu::spike::bridge_adapters {

bool InjectTlozItem(const AdapterContext& context,
                    std::uint32_t item_code,
                    const std::string& label) {
  const WatchRegion game_mode_region{"system_ram", 0x0012u, 1};
  const auto game_mode = context.read_region(game_mode_region);
  if (game_mode.size() != 1 || game_mode[0] != 0x05u) {
    return false;
  }

  auto write_byte = [&](std::uint32_t offset, std::uint8_t value) -> bool {
    auto* target = context.resolve_mutable("system_ram", offset, 1);
    if (!target) {
      return false;
    }
    target[0] = value;
    return true;
  };

  bool ok = false;
  if (item_code == 0x1Au) {
    const WatchRegion hearts_region{"system_ram", 0x066Fu, 1};
    const auto heart_byte = context.read_region(hearts_region);
    if (heart_byte.size() != 1) {
      context.append_log("inject_error|tloz_heart_unreadable");
      return false;
    }

    const std::uint8_t current = heart_byte[0];
    const std::uint8_t containers =
        static_cast<std::uint8_t>(std::min<unsigned>(((current >> 4) & 0x0Fu) + 1u, 0x0Fu));
    const std::uint8_t hearts =
        static_cast<std::uint8_t>(std::min<unsigned>((current & 0x0Fu) + 1u, 0x0Fu));
    ok = write_byte(0x066Fu, static_cast<std::uint8_t>((containers << 4) | hearts));
  } else if (item_code == 0x19u) {
    const WatchRegion key_region{"system_ram", 0x066Eu, 1};
    const auto key_byte = context.read_region(key_region);
    if (key_byte.size() != 1) {
      context.append_log("inject_error|tloz_key_unreadable");
      return false;
    }

    const std::uint8_t next_keys =
        static_cast<std::uint8_t>(std::min<unsigned>(key_byte[0] + 1u, 0xFFu));
    ok = write_byte(0x066Eu, next_keys);
  } else {
    context.append_log("inject_error|tloz_unsupported_item");
    return false;
  }

  if (!ok ||
      !write_byte(0x0505u, static_cast<std::uint8_t>(item_code & 0xFFu)) ||
      !write_byte(0x0506u, 128u) ||
      !write_byte(0x0602u, 4u)) {
    context.append_log("inject_error|tloz_write_failed");
    return false;
  }

  std::ostringstream event;
  event << "inject|item|0x" << std::hex << std::uppercase << item_code << std::dec;
  if (!label.empty()) {
    event << "|" << label;
  }
  context.append_log(event.str());

  std::ostringstream trace;
  trace << "[sekaiemu] injected TLOZ item"
        << " value=0x" << std::hex << std::uppercase << item_code << std::dec;
  if (!label.empty()) {
    trace << " label=" << label;
  }
  context.trace(trace.str());
  return true;
}

}  // namespace sekaiemu::spike::bridge_adapters
