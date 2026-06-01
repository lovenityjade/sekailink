#include "firered_adapter.hpp"

#include <iomanip>
#include <sstream>

namespace sekaiemu::spike::bridge_adapters {

bool InjectFireRedItem(const AdapterContext& context,
                       std::uint32_t item_code,
                       bool should_display,
                       const std::string& label) {
  const WatchRegion main_state_region{"gba_system_bus", 0x03003078, 1};
  const auto overworld_state = context.read_region(main_state_region);
  if (overworld_state.empty() || overworld_state[0] != 1) {
    return false;
  }

  const WatchRegion sb1_ptr_region{"gba_system_bus", 0x03004F58, 4};
  const auto sb1_ptr_bytes = context.read_region(sb1_ptr_region);
  if (sb1_ptr_bytes.size() != 4) {
    context.append_log("inject_error|firered_save_block1_unreadable");
    return false;
  }

  const std::uint32_t sb1_address =
      static_cast<std::uint32_t>(sb1_ptr_bytes[0]) |
      (static_cast<std::uint32_t>(sb1_ptr_bytes[1]) << 8) |
      (static_cast<std::uint32_t>(sb1_ptr_bytes[2]) << 16) |
      (static_cast<std::uint32_t>(sb1_ptr_bytes[3]) << 24);
  if (sb1_address < 0x02000000u || sb1_address >= 0x02040000u) {
    context.append_log("inject_error|firered_save_block1_invalid");
    return false;
  }

  const WatchRegion received_count_region{"gba_system_bus", sb1_address + 0x3DE8u, 2};
  const auto received_count = context.read_region(received_count_region);
  if (received_count.size() != 2) {
    context.append_log("inject_error|firered_count_unreadable");
    return false;
  }

  const auto* item_slot = context.find_receive_slot("item");
  const auto* progress_slot = context.find_receive_slot("progress");
  const auto* pending_slot = context.find_receive_slot("pending");
  const auto* display_slot = context.find_receive_slot("display");
  if (!item_slot || !progress_slot || !pending_slot || !display_slot) {
    context.append_log("inject_error|firered_missing_slots");
    return false;
  }

  const auto pending_value = context.read_slot_value(*pending_slot);
  if (!pending_value.has_value()) {
    context.append_log("inject_error|firered_pending_unreadable");
    return false;
  }
  if ((*pending_value & 0xFFu) != 0) {
    return false;
  }

  const std::uint32_t next_progress =
      (static_cast<std::uint32_t>(received_count[0]) |
       (static_cast<std::uint32_t>(received_count[1]) << 8)) + 1u;

  if (!context.write_slot_value(*item_slot, item_code & 0xFFFFu) ||
      !context.write_slot_value(*progress_slot, next_progress & 0xFFFFu) ||
      !context.write_slot_value(*pending_slot, 1u) ||
      !context.write_slot_value(*display_slot, should_display ? 1u : 0u)) {
    context.append_log("inject_error|firered_write_failed");
    return false;
  }

  std::ostringstream event;
  event << "inject|item|0x" << std::hex << std::uppercase << item_code
        << std::dec << "|progress=" << (next_progress & 0xFFFFu)
        << "|display=" << (should_display ? 1 : 0);
  if (!label.empty()) {
    event << "|" << label;
  }
  context.append_log(event.str());

  std::ostringstream trace;
  trace << "[sekaiemu] injected FireRed item"
        << " value=0x" << std::hex << std::uppercase << item_code
        << std::dec << " progress=" << (next_progress & 0xFFFFu)
        << " display=" << (should_display ? 1 : 0);
  if (!label.empty()) {
    trace << " label=" << label;
  }
  context.trace(trace.str());
  return true;
}

}  // namespace sekaiemu::spike::bridge_adapters
