#include "alttp_adapter.hpp"

#include <iomanip>
#include <sstream>

namespace sekaiemu::spike::bridge_adapters {

bool InjectAlttpItem(const AdapterContext& context,
                     std::uint32_t item_code,
                     std::uint32_t player_code,
                     const std::string& label) {
  const auto* progress_slot = context.find_receive_slot("progress");
  const auto* item_slot = context.find_receive_slot("item");
  const auto* player_slot = context.find_receive_slot("player");
  if (!progress_slot || !item_slot || !player_slot) {
    context.append_log("inject_error|alttp_missing_slots");
    return false;
  }

  if (context.read_slot_busy(*item_slot)) {
    return false;
  }

  const auto current_progress = context.read_slot_value(*progress_slot);
  if (!current_progress.has_value()) {
    context.append_log("inject_error|alttp_progress_unreadable");
    return false;
  }

  const std::uint32_t next_progress = (*current_progress + 1u) & 0xFFFFu;
  if (!context.write_slot_value(*progress_slot, next_progress) ||
      !context.write_slot_value(*item_slot, item_code & 0xFFu) ||
      !context.write_slot_value(*player_slot, player_code & 0xFFu)) {
    context.append_log("inject_error|alttp_write_failed");
    return false;
  }

  std::ostringstream event;
  event << "inject|item|0x" << std::hex << std::uppercase << item_code
        << "|player=" << std::dec << (player_code & 0xFFu)
        << "|progress=" << next_progress;
  if (!label.empty()) {
    event << "|" << label;
  }
  context.append_log(event.str());

  std::ostringstream trace;
  trace << "[sekaiemu] injected ALTTP item"
        << " value=0x" << std::hex << std::uppercase << item_code
        << std::dec << " player=" << (player_code & 0xFFu)
        << " progress=" << next_progress;
  if (!label.empty()) {
    trace << " label=" << label;
  }
  context.trace(trace.str());
  return true;
}

}  // namespace sekaiemu::spike::bridge_adapters
