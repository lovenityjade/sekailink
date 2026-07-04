#include "ladx_adapter.hpp"

#include <iomanip>
#include <sstream>

namespace sekaiemu::spike::bridge_adapters {

bool InjectLadxItem(const AdapterContext& context,
                    std::uint32_t item_code,
                    std::uint32_t sender_code,
                    const std::string& label) {
  auto read_byte = [&](std::uint32_t address, std::uint8_t& out_value) -> bool {
    const WatchRegion region{"system_ram", address, 1};
    const auto bytes = context.read_region(region);
    if (bytes.size() != 1) {
      return false;
    }
    out_value = bytes[0];
    return true;
  };

  const auto* command_slot = context.find_receive_slot("command");
  const auto* item_slot = context.find_receive_slot("item");
  const auto* sender_hi_slot = context.find_receive_slot("sender_hi");
  const auto* sender_lo_slot = context.find_receive_slot("sender_lo");
  const auto* recv_index_hi_slot = context.find_receive_slot("recv_index_hi");
  const auto* recv_index_lo_slot = context.find_receive_slot("recv_index_lo");
  const auto* mp_c_slot = context.find_receive_slot("mp_c");
  const auto* mp_d_slot = context.find_receive_slot("mp_d");
  if (!command_slot || !item_slot || !sender_hi_slot || !sender_lo_slot ||
      !recv_index_hi_slot || !recv_index_lo_slot || !mp_c_slot || !mp_d_slot) {
    context.append_log("inject_error|ladx_missing_slots");
    return false;
  }

  std::uint8_t gameplay_type = 0;
  std::uint8_t safety0 = 0;
  std::uint8_t safety1 = 0;
  std::uint8_t safety2 = 0;
  std::uint8_t safety3 = 0;
  if (!read_byte(0xDB95u, gameplay_type) ||
      !read_byte(0xC0FBu, safety0) ||
      !read_byte(0xC0FCu, safety1) ||
      !read_byte(0xC0Fdu, safety2) ||
      !read_byte(0xC0FEu, safety3)) {
    context.append_log("inject_error|ladx_state_unreadable");
    return false;
  }

  const bool gameplay_ok =
      (gameplay_type >= 0x06u && gameplay_type <= 0x1Au) || gameplay_type == 0x01u;
  const bool safety_ok = safety0 == 0 && safety1 == 0 && safety2 == 0 && safety3 == 0;
  if (!gameplay_ok || !safety_ok) {
    return false;
  }

  const auto command_value = context.read_slot_value(*command_slot);
  const auto recv_index_hi = context.read_slot_value(*recv_index_hi_slot);
  const auto recv_index_lo = context.read_slot_value(*recv_index_lo_slot);
  if (!command_value.has_value() || !recv_index_hi.has_value() || !recv_index_lo.has_value()) {
    context.append_log("inject_error|ladx_recv_index_unreadable");
    return false;
  }

  if ((*command_value & 0xFFu) != 0) {
    return false;
  }

  const std::uint16_t recv_index = static_cast<std::uint16_t>(
      ((*recv_index_hi & 0xFFu) << 8) | (*recv_index_lo & 0xFFu));
  const std::uint8_t sender_hi = static_cast<std::uint8_t>((sender_code >> 8) & 0xFFu);
  const std::uint8_t sender_lo = static_cast<std::uint8_t>(sender_code & 0xFFu);

  if (!context.write_slot_value(*item_slot, item_code & 0xFFu) ||
      !context.write_slot_value(*sender_hi_slot, sender_hi) ||
      !context.write_slot_value(*sender_lo_slot, sender_lo) ||
      !context.write_slot_value(*mp_c_slot, static_cast<std::uint8_t>((recv_index >> 8) & 0xFFu)) ||
      !context.write_slot_value(*mp_d_slot, static_cast<std::uint8_t>(recv_index & 0xFFu)) ||
      !context.write_slot_value(*command_slot, 0x83u)) {
    context.append_log("inject_error|ladx_write_failed");
    return false;
  }

  std::ostringstream event;
  event << "inject|item|0x" << std::hex << std::uppercase << item_code
        << std::dec << "|sender=" << sender_code
        << "|recv_index=" << recv_index;
  if (!label.empty()) {
    event << "|" << label;
  }
  context.append_log(event.str());

  std::ostringstream trace;
  trace << "[sekaiemu] injected LADX item"
        << " value=0x" << std::hex << std::uppercase << item_code
        << std::dec << " sender=" << sender_code
        << " recv_index=" << recv_index;
  if (!label.empty()) {
    trace << " label=" << label;
  }
  context.trace(trace.str());
  return true;
}

}  // namespace sekaiemu::spike::bridge_adapters
