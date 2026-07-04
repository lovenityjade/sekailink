#include "legacy_bridge_commands.hpp"

#include "bridge_adapters/alttp_adapter.hpp"
#include "bridge_adapters/firered_adapter.hpp"
#include "bridge_adapters/ladx_adapter.hpp"
#include "bridge_adapters/oot_adapter.hpp"
#include "bridge_adapters/tloz_adapter.hpp"
#include "libretro_core_utils.hpp"

#include <cctype>
#include <sstream>
#include <vector>

namespace sekaiemu::spike {

namespace {

bool ParseLooseUnsigned(std::string_view text, std::uint32_t& out_value) {
  try {
    const auto lowered = Lowercase(text);
    std::size_t parsed = 0;
    unsigned long long value = 0;
    if (lowered.starts_with("0x")) {
      value = std::stoull(std::string(lowered), &parsed, 16);
    } else {
      value = std::stoull(std::string(text), &parsed, 10);
    }
    if (parsed != std::string(text).size() || value > 0xFFFFFFFFull) {
      return false;
    }
    out_value = static_cast<std::uint32_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

std::string TrimBridgeToken(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

std::vector<std::string> SplitBridgeLine(const std::string& line) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= line.size()) {
    const auto position = line.find('|', start);
    if (position == std::string::npos) {
      parts.push_back(TrimBridgeToken(line.substr(start)));
      break;
    }
    parts.push_back(TrimBridgeToken(line.substr(start, position - start)));
    start = position + 1;
  }
  return parts;
}

bridge_adapters::AdapterContext BuildAdapterContext(const LegacyBridgeCommandContext& context) {
  return bridge_adapters::AdapterContext{
      .find_receive_slot = [&context](std::string_view kind) { return context.find_receive_slot(kind); },
      .read_region = [&context](const WatchRegion& region) { return context.read_region(region); },
      .resolve_mutable = [&context](std::string_view domain, std::uint32_t start, std::uint32_t length) {
        return context.resolve_mutable(domain, start, length);
      },
      .read_slot_busy = [&context](const ReceiveSlot& slot) { return context.read_slot_busy(slot); },
      .write_slot_value = [&context](const ReceiveSlot& slot, std::uint32_t value) {
        return context.write_slot_value(slot, value);
      },
      .read_slot_value = [&context](const ReceiveSlot& slot) { return context.read_slot_value(slot); },
      .append_log = [&context](const std::string& line) { context.append_log(line); },
      .trace = [&context](const std::string& line) {
        if (context.trace) {
          context.trace(line);
        }
      },
  };
}

}  // namespace

void ProcessLegacyBridgeCommands(BridgeIpc& bridge_ipc,
                                 const LegacyBridgeCommandContext& context) {
  const auto commands = bridge_ipc.DrainCommands();
  for (std::size_t command_index = 0; command_index < commands.size(); ++command_index) {
    const std::string& line = commands[command_index];
    if (line.empty()) {
      continue;
    }
    const auto fields = SplitBridgeLine(line);
    if (fields.size() < 2) {
      context.append_log("inject_error|invalid_command|index=" + std::to_string(command_index));
      continue;
    }

    const auto adapter_context = BuildAdapterContext(context);
    const auto game_name = Lowercase(context.profile.game);
    if (game_name == "the legend of zelda" && Lowercase(fields[0]) == "item") {
      const auto code = context.resolve_injection_code(fields[0], fields[1]);
      if (!code.has_value()) {
        context.append_log("inject_error|unknown_code|" + fields[1]);
        continue;
      }
      std::string label = fields.size() >= 3 ? fields[2] : "";
      if (!bridge_adapters::InjectTlozItem(adapter_context, *code, label)) {
        return;
      }
      return;
    }

    if (game_name == "ocarina of time" && Lowercase(fields[0]) == "item") {
      const auto code = context.resolve_injection_code(fields[0], fields[1]);
      if (!code.has_value()) {
        context.append_log("inject_error|unknown_code|" + fields[1]);
        continue;
      }
      std::string label = fields.size() >= 3 ? fields[2] : "";
      if (!bridge_adapters::InjectOotItem(adapter_context, *code, label)) {
        return;
      }
      return;
    }

    const auto* slot = context.find_receive_slot(fields[0]);
    if (!slot) {
      context.append_log("inject_error|unknown_kind|" + fields[0]);
      continue;
    }
    const auto code = context.resolve_injection_code(fields[0], fields[1]);
    if (!code.has_value()) {
      context.append_log("inject_error|unknown_code|" + fields[1]);
      continue;
    }

    if (game_name == "a link to the past" &&
        context.find_receive_slot("progress") && context.find_receive_slot("item") &&
        context.find_receive_slot("player") && Lowercase(fields[0]) == "item") {
      std::uint32_t player_code = 0;
      std::string label;
      if (fields.size() >= 3) {
        std::uint32_t parsed_player = 0;
        if (ParseLooseUnsigned(fields[2], parsed_player)) {
          player_code = parsed_player & 0xFFu;
        } else {
          label = fields[2];
        }
      }
      if (fields.size() >= 4) {
        label = fields[3];
      }
      if (!bridge_adapters::InjectAlttpItem(adapter_context, *code, player_code, label)) {
        return;
      }
      return;
    }

    if (game_name == "pokemon firered" &&
        context.find_receive_slot("progress") && context.find_receive_slot("item") &&
        context.find_receive_slot("pending") && context.find_receive_slot("display") &&
        Lowercase(fields[0]) == "item") {
      bool should_display = true;
      std::string label;
      if (fields.size() >= 3) {
        std::uint32_t parsed_display = 0;
        if (ParseLooseUnsigned(fields[2], parsed_display)) {
          should_display = parsed_display != 0;
        } else {
          label = fields[2];
        }
      }
      if (fields.size() >= 4) {
        label = fields[3];
      }
      if (!bridge_adapters::InjectFireRedItem(adapter_context, *code, should_display, label)) {
        return;
      }
      return;
    }

    if (game_name == "links awakening dx beta" &&
        context.find_receive_slot("command") && context.find_receive_slot("item") &&
        context.find_receive_slot("sender_hi") && context.find_receive_slot("sender_lo") &&
        context.find_receive_slot("recv_index_hi") && context.find_receive_slot("recv_index_lo") &&
        context.find_receive_slot("mp_c") && context.find_receive_slot("mp_d") &&
        Lowercase(fields[0]) == "item") {
      std::uint32_t sender_code = 0;
      std::string label;
      if (fields.size() >= 3) {
        std::uint32_t parsed_sender = 0;
        if (ParseLooseUnsigned(fields[2], parsed_sender)) {
          sender_code = parsed_sender & 0xFFFFu;
        } else {
          label = fields[2];
        }
      }
      if (fields.size() >= 4) {
        label = fields[3];
      }
      if (!bridge_adapters::InjectLadxItem(adapter_context, *code, sender_code, label)) {
        return;
      }
      return;
    }

    if (context.read_slot_busy(*slot)) {
      return;
    }
    if (!context.write_slot_value(*slot, *code)) {
      context.append_log("inject_error|write_failed|index=" + std::to_string(command_index));
      continue;
    }

    std::ostringstream event;
    event << "inject|" << fields[0] << "|0x" << std::hex << std::uppercase << *code << std::dec;
    if (fields.size() >= 3 && !fields[2].empty()) {
      event << "|" << fields[2];
    }
    context.append_log(event.str());
    if (context.trace) {
      std::ostringstream trace;
      trace << "[sekaiemu] injected " << fields[0]
            << " value=0x" << std::hex << std::uppercase << *code << std::dec;
      if (fields.size() >= 3 && !fields[2].empty()) {
        trace << " label=" << fields[2];
      }
      context.trace(trace.str());
    }
    return;
  }
}

}  // namespace sekaiemu::spike
