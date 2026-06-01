#include "launch_options.hpp"

#include <charconv>
#include <sstream>

namespace sekaiemu::spike {

namespace {

bool IsFlag(const std::string& argument) {
  return argument.rfind("--", 0) == 0;
}

std::optional<std::uint16_t> ParsePort(const std::string& text) {
  std::uint64_t value = 0;
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value, 10);
  if (result.ec != std::errc{} || result.ptr != end || value > 65535) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(value);
}

std::optional<std::uint64_t> ParseUint64(const std::string& text) {
  std::uint64_t value = 0;
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value, 10);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

std::string BuildUsageText(const std::string& executable_name) {
  std::ostringstream text;
  text
      << "Usage:\n"
      << "  " << executable_name
      << " [--probe-only] [--profile <profile.txt>] [--system-dir <dir>] [--save-dir <dir>] [--log-dir <dir>] [--memory-socket <path>] [--sklmi-runtime <path>] [--sklmi-manifest-dir <dir>] [--ap-host <host>] [--ap-port <port>] [--ap-path <path>] [--ap-game <game>] [--ap-slot-name <slot>] [--ap-password <password>] [--ap-uuid <uuid>] [--ap-tags <tags>] [--tracker-required] [--tracker-pack <path>] [--tracker-variant <name>] [--tracker-snapshot <path>] [--tracker-command-log <path>] [--tracker-assets-root <path>] [--tracker-bundle <path>] [--tracker-state <path>] [--chat-inbox <path>] [--chat-outbox <path>] [--input-script <path>] [--input-script-quit-after-end] [--load-state-on-start] [--save-state-at-frame <frame>] [--quit-after-frame <frame>] [--dump-frame-at-frame <frame>] [--dump-frame-path <path.ppm>] <core.so> <game.rom>\n"
      << "  " << executable_name
      << " [--probe-only] --core <core.so> --game <game.rom> [--system-dir <dir>] [--save-dir <dir>] [--profile <profile.txt>] [--log-dir <dir>] [--memory-socket <path>] [--sklmi-runtime <path>] [--sklmi-manifest-dir <dir>] [--ap-host <host>] [--ap-port <port>] [--ap-path <path>] [--ap-game <game>] [--ap-slot-name <slot>] [--ap-password <password>] [--ap-uuid <uuid>] [--ap-tags <tags>] [--tracker-required] [--tracker-pack <path>] [--tracker-variant <name>] [--tracker-snapshot <path>] [--tracker-command-log <path>] [--tracker-assets-root <path>] [--tracker-bundle <path>] [--tracker-state <path>] [--chat-inbox <path>] [--chat-outbox <path>] [--input-script <path>] [--input-script-quit-after-end] [--load-state-on-start] [--save-state-at-frame <frame>] [--quit-after-frame <frame>] [--dump-frame-at-frame <frame>] [--dump-frame-path <path.ppm>]\n"
      << "  " << executable_name
      << " [--probe-only] --core <core.so> --patch <patch.aplttp> --base-rom <rom.sfc> [--system-dir <dir>] [--save-dir <dir>] [--profile <profile.txt>] [--log-dir <dir>] [--memory-socket <path>] [--sklmi-runtime <path>] [--sklmi-manifest-dir <dir>] [--ap-host <host>] [--ap-port <port>] [--ap-path <path>] [--ap-game <game>] [--ap-slot-name <slot>] [--ap-password <password>] [--ap-uuid <uuid>] [--ap-tags <tags>] [--tracker-required] [--tracker-pack <path>] [--tracker-variant <name>] [--tracker-snapshot <path>] [--tracker-command-log <path>] [--tracker-assets-root <path>] [--tracker-bundle <path>] [--tracker-state <path>] [--chat-inbox <path>] [--chat-outbox <path>] [--input-script <path>] [--input-script-quit-after-end] [--load-state-on-start] [--save-state-at-frame <frame>] [--quit-after-frame <frame>] [--dump-frame-at-frame <frame>] [--dump-frame-path <path.ppm>]\n\n"
      << "Examples:\n"
      << "  " << executable_name << " ./bsnes_mercury_libretro.so ./EarthBound.sfc ./system ./saves\n"
      << "  " << executable_name << " --probe-only ./mgba_libretro.so ./Pokemon.gba ./system ./saves\n"
      << "  " << executable_name << " --profile ./profiles/earthbound-starter.profile ./bsnes.so ./EarthBound.sfc ./system ./saves\n"
      << "  " << executable_name << " --core ./mupen64plus_next_libretro.so --game ./OOT.z64 --save-dir ./saves --log-dir ./logs\n"
      << "  " << executable_name << " --core ./bsnes.so --patch ./AP_Seed.aplttp --base-rom ./Zelda.sfc --save-dir ./saves\n";
  return text.str();
}

CliParseResult ParseLaunchArgs(const std::vector<std::string>& args,
                               const std::filesystem::path& current_directory) {
  CliParseResult result;
  result.request.system_directory = current_directory;
  result.request.save_directory = current_directory;

  std::vector<std::string> positional;
  for (std::size_t index = 0; index < args.size(); ++index) {
    const auto& argument = args[index];
    if (argument == "--help" || argument == "-h") {
      result.ok = false;
      result.exit_code = 0;
      result.show_usage = true;
      return result;
    }
    if (argument == "--probe-only") {
      result.request.probe_only = true;
      continue;
    }
    if (argument == "--input-script-quit-after-end") {
      result.request.input_script_quit_after_end = true;
      continue;
    }
    if (argument == "--load-state-on-start") {
      result.request.load_state_on_start = true;
      continue;
    }
    if (argument == "--tracker-required") {
      result.request.tracker_required = true;
      continue;
    }
    if (argument == "--profile" || argument == "--core" || argument == "--game" ||
        argument == "--patch" || argument == "--base-rom" ||
        argument == "--system-dir" || argument == "--save-dir" || argument == "--log-dir" ||
        argument == "--memory-socket" || argument == "--sklmi-runtime" ||
        argument == "--sklmi-manifest-dir" || argument == "--ap-host" ||
        argument == "--ap-port" || argument == "--ap-path" || argument == "--ap-game" ||
        argument == "--ap-slot-name" || argument == "--ap-password" || argument == "--ap-uuid" ||
        argument == "--ap-tags" || argument == "--tracker-pack" ||
        argument == "--tracker-variant" || argument == "--tracker-snapshot" ||
        argument == "--tracker-command-log" || argument == "--tracker-assets-root" ||
        argument == "--tracker-bundle" ||
        argument == "--tracker-state" || argument == "--chat-inbox" ||
        argument == "--chat-outbox" || argument == "--input-script" ||
        argument == "--save-state-at-frame" || argument == "--quit-after-frame" ||
        argument == "--dump-frame-at-frame" || argument == "--dump-frame-path") {
      if (index + 1 >= args.size()) {
        result.user_message = "Missing value for " + argument + ".";
        return result;
      }
      const auto& value = args[++index];
      if (argument == "--profile") {
        result.request.profile_path = value;
      } else if (argument == "--core") {
        result.request.core_path = value;
      } else if (argument == "--game") {
        result.request.game_path = value;
      } else if (argument == "--patch") {
        result.request.patch_path = value;
      } else if (argument == "--base-rom") {
        result.request.base_rom_path = value;
      } else if (argument == "--system-dir") {
        result.request.system_directory = value;
      } else if (argument == "--save-dir") {
        result.request.save_directory = value;
      } else if (argument == "--log-dir") {
        result.request.log_directory = value;
      } else if (argument == "--memory-socket") {
        result.request.memory_socket_path = value;
      } else if (argument == "--sklmi-runtime") {
        result.request.sklmi_runtime_path = value;
      } else if (argument == "--sklmi-manifest-dir") {
        result.request.sklmi_manifest_directory = value;
      } else if (argument == "--ap-host") {
        result.request.ap_host = value;
      } else if (argument == "--ap-port") {
        const auto port = ParsePort(value);
        if (!port.has_value()) {
          result.user_message = "Invalid Archipelago server port.";
          return result;
        }
        result.request.ap_port = *port;
      } else if (argument == "--ap-path") {
        result.request.ap_path = value.empty() ? "/" : value;
      } else if (argument == "--ap-game") {
        result.request.ap_game = value;
      } else if (argument == "--ap-slot-name") {
        result.request.ap_slot_name = value;
      } else if (argument == "--ap-password") {
        result.request.ap_password = value;
      } else if (argument == "--ap-uuid") {
        result.request.ap_uuid = value;
      } else if (argument == "--ap-tags") {
        result.request.ap_tags = value;
      } else if (argument == "--tracker-pack") {
        result.request.tracker_pack_path = value;
      } else if (argument == "--tracker-variant") {
        result.request.tracker_variant = value;
      } else if (argument == "--tracker-snapshot") {
        result.request.tracker_snapshot_path = value;
      } else if (argument == "--tracker-command-log") {
        result.request.tracker_command_log_path = value;
      } else if (argument == "--tracker-assets-root") {
        result.request.tracker_assets_root = value;
      } else if (argument == "--tracker-bundle") {
        result.request.tracker_bundle_path = value;
      } else if (argument == "--tracker-state") {
        result.request.tracker_state_path = value;
      } else if (argument == "--chat-inbox") {
        result.request.chat_inbox_path = value;
      } else if (argument == "--chat-outbox") {
        result.request.chat_outbox_path = value;
      } else if (argument == "--input-script") {
        result.request.input_script_path = value;
      } else if (argument == "--save-state-at-frame") {
        const auto frame = ParseUint64(value);
        if (!frame.has_value()) {
          result.user_message = "Invalid frame for --save-state-at-frame.";
          return result;
        }
        result.request.save_state_at_frame = *frame;
      } else if (argument == "--quit-after-frame") {
        const auto frame = ParseUint64(value);
        if (!frame.has_value()) {
          result.user_message = "Invalid frame for --quit-after-frame.";
          return result;
        }
        result.request.quit_after_frame = *frame;
      } else if (argument == "--dump-frame-at-frame") {
        const auto frame = ParseUint64(value);
        if (!frame.has_value()) {
          result.user_message = "Invalid frame for --dump-frame-at-frame.";
          return result;
        }
        result.request.dump_frame_at_frame = *frame;
      } else if (argument == "--dump-frame-path") {
        result.request.dump_frame_path = value;
      }
      continue;
    }
    if (IsFlag(argument)) {
      result.user_message = "Unknown option: " + argument + ".";
      return result;
    }
    positional.push_back(argument);
  }

  if (result.request.core_path.empty() && !positional.empty()) {
    result.request.core_path = positional[0];
  }
  if (result.request.game_path.empty() && positional.size() >= 2) {
    result.request.game_path = positional[1];
  }
  if (positional.size() >= 3) {
    result.request.system_directory = positional[2];
  }
  if (positional.size() >= 4) {
    result.request.save_directory = positional[3];
  }

  if (result.request.core_path.empty()) {
    result.user_message = "No libretro core path was provided.";
    result.show_usage = true;
    return result;
  }
  if (result.request.game_path.empty() && result.request.patch_path.empty()) {
    result.user_message = "No game path or patch path was provided.";
    result.show_usage = true;
    return result;
  }
  if (!result.request.patch_path.empty() && result.request.base_rom_path.empty()) {
    result.user_message = "No base ROM path was provided for the selected patch.";
    result.show_usage = true;
    return result;
  }

  result.ok = true;
  result.exit_code = 0;
  return result;
}

HostOptions BuildHostOptions(const LaunchRequest& request) {
  return HostOptions{
      .core_path = request.core_path,
      .game_path = request.game_path,
      .system_directory = request.system_directory,
      .save_directory = request.save_directory,
      .profile_path = request.profile_path,
      .memory_socket_path = request.memory_socket_path,
      .sklmi_runtime_path = request.sklmi_runtime_path,
      .sklmi_manifest_directory = request.sklmi_manifest_directory,
      .ap_host = request.ap_host,
      .ap_port = request.ap_port,
      .ap_path = request.ap_path,
      .ap_game = request.ap_game,
      .ap_slot_name = request.ap_slot_name,
      .ap_password = request.ap_password,
      .ap_uuid = request.ap_uuid,
      .ap_tags = request.ap_tags,
      .tracker_pack_path = request.tracker_pack_path,
      .tracker_variant = request.tracker_variant,
      .tracker_snapshot_path = request.tracker_snapshot_path,
      .tracker_command_log_path = request.tracker_command_log_path,
      .tracker_assets_root = request.tracker_assets_root,
      .tracker_bundle_path = request.tracker_bundle_path,
      .tracker_state_path = request.tracker_state_path,
      .tracker_required = request.tracker_required,
      .chat_inbox_path = request.chat_inbox_path,
      .chat_outbox_path = request.chat_outbox_path,
      .input_script_path = request.input_script_path,
      .dump_frame_path = request.dump_frame_path,
      .save_state_at_frame = request.save_state_at_frame,
      .quit_after_frame = request.quit_after_frame,
      .dump_frame_at_frame = request.dump_frame_at_frame,
      .input_script_quit_after_end = request.input_script_quit_after_end,
      .load_state_on_start = request.load_state_on_start,
      .probe_only = request.probe_only,
  };
}

}  // namespace sekaiemu::spike
