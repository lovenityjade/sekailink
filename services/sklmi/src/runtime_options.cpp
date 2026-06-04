#include "runtime_options.hpp"

#include <charconv>
#include <sstream>

namespace sekailink::sklmi {

namespace {

std::optional<std::uint64_t> ParseUnsigned(std::string_view text) {
    if (text.empty()) return std::nullopt;
    std::uint64_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value, 10);
    if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
    return value;
}

bool RequireValue(const std::vector<std::string>& args, std::size_t index, std::string* out, std::string* error) {
    if (index + 1 >= args.size()) {
        *error = "missing_value_for_" + args[index];
        return false;
    }
    *out = args[index + 1];
    return true;
}

std::uint16_t EffectivePort(std::uint16_t specific_port, std::uint16_t shared_port) {
    return specific_port != 0 ? specific_port : shared_port;
}

std::vector<std::string> SplitCsv(std::string_view text) {
    std::vector<std::string> out;
    std::string current;
    for (const char ch : text) {
        if (ch == ',') {
            if (!current.empty()) out.push_back(current);
            current.clear();
        } else if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            current.push_back(ch);
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

}  // namespace

RuntimeParseResult ParseRuntimeOptions(const std::vector<std::string>& args) {
    RuntimeParseResult result;
    if (args.empty()) {
        result.show_help = true;
        result.ok = true;
        return result;
    }

    RuntimeOptions options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        std::string value;
        if (arg == "--help" || arg == "-h") {
            result.show_help = true;
            result.ok = true;
            return result;
        } else if (arg == "--memory-socket") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.memory_socket_path = value;
            ++i;
        } else if (arg == "--bridge-manifest") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.bridge_manifest_path = value;
            ++i;
        } else if (arg == "--room-state") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_state_path = value;
            ++i;
        } else if (arg == "--runtime-state") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.runtime_state_root = value;
            ++i;
        } else if (arg == "--trace-log") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.trace_log_path = value;
            ++i;
        } else if (arg == "--tracker-pack") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.tracker_pack_path = value;
            ++i;
        } else if (arg == "--tracker-variant") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.tracker_variant = value;
            ++i;
        } else if (arg == "--tracker-snapshot") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.tracker_snapshot_path = value;
            ++i;
        } else if (arg == "--tracker-command-log") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.tracker_command_log_path = value;
            ++i;
        } else if (arg == "--tracker-assets-root") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.tracker_assets_root = value;
            ++i;
        } else if (arg == "--driver-instance-id") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.driver_instance_id = value;
            ++i;
        } else if (arg == "--mode") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.mode = value;
            ++i;
        } else if (arg == "--room-host") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_host = value;
            ++i;
        } else if (arg == "--room-port") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed > 65535) {
                result.error = "invalid_room_port";
                return result;
            }
            options.room_port = static_cast<std::uint16_t>(*parsed);
            ++i;
        } else if (arg == "--room-control-port") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed > 65535) {
                result.error = "invalid_room_control_port";
                return result;
            }
            options.room_control_port = static_cast<std::uint16_t>(*parsed);
            ++i;
        } else if (arg == "--room-runtime-port") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed > 65535) {
                result.error = "invalid_room_runtime_port";
                return result;
            }
            options.room_runtime_port = static_cast<std::uint16_t>(*parsed);
            ++i;
        } else if (arg == "--room-session-name") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_session_name = value;
            ++i;
        } else if (arg == "--room-slot-id") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed == 0 || *parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
                result.error = "invalid_room_slot_id";
                return result;
            }
            options.room_slot_id = static_cast<int>(*parsed);
            ++i;
        } else if (arg == "--room-control-channel") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_control_channel = value;
            ++i;
        } else if (arg == "--room-control-auth-token") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_control_auth_token = value;
            ++i;
        } else if (arg == "--room-runtime-auth-token") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_runtime_auth_token = value;
            ++i;
        } else if (arg == "--room-runtime-session-token") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.room_runtime_session_token = value;
            ++i;
        } else if (arg == "--ap-host") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_host = value;
            ++i;
        } else if (arg == "--ap-port") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed > 65535) {
                result.error = "invalid_ap_port";
                return result;
            }
            options.ap_port = static_cast<std::uint16_t>(*parsed);
            ++i;
        } else if (arg == "--ap-path") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_path = value.empty() ? "/" : value;
            ++i;
        } else if (arg == "--ap-game") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_game = value;
            ++i;
        } else if (arg == "--ap-slot-name") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_slot_name = value;
            ++i;
        } else if (arg == "--player-alias") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.player_alias = value;
            ++i;
        } else if (arg == "--ap-password") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_password = value;
            ++i;
        } else if (arg == "--ap-uuid") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_uuid = value;
            ++i;
        } else if (arg == "--ap-tags") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            options.ap_tags = SplitCsv(value);
            if (options.ap_tags.empty()) {
                options.ap_tags = {"AP", "SekaiLink", "SKLMI"};
            }
            ++i;
        } else if (arg == "--tick-ms") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value()) {
                result.error = "invalid_tick_ms";
                return result;
            }
            options.tick_ms = *parsed;
            ++i;
        } else if (arg == "--max-ticks") {
            if (!RequireValue(args, i, &value, &result.error)) return result;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value()) {
                result.error = "invalid_max_ticks";
                return result;
            }
            options.max_ticks = *parsed;
            ++i;
        } else {
            result.error = "unknown_argument:" + arg;
            return result;
        }
    }

    if (options.memory_socket_path.empty()) {
        result.error = "missing_memory_socket";
        return result;
    }
    if (options.bridge_manifest_path.empty()) {
        result.error = "missing_bridge_manifest";
        return result;
    }
    if (options.room_state_path.empty()) {
        result.error = "missing_room_state";
        return result;
    }
    if (options.runtime_state_root.empty()) {
        result.error = "missing_runtime_state";
        return result;
    }
    if (options.trace_log_path.empty()) {
        result.error = "missing_trace_log";
        return result;
    }
    if (options.mode != "offline" && options.mode != "sekailink_game_server" && options.mode != "archipelago") {
        result.error = "unsupported_mode:" + options.mode;
        return result;
    }
    if (options.mode == "sekailink_game_server") {
        const auto effective_control_port = EffectivePort(options.room_control_port, options.room_port);
        const auto effective_runtime_port = EffectivePort(options.room_runtime_port, options.room_port);
        if (effective_runtime_port == 0) {
            result.error = "missing_room_runtime_port";
            return result;
        }
        if (options.room_session_name.empty()) {
            result.error = "missing_room_session_name";
            return result;
        }
        if (options.room_slot_id <= 0) {
            result.error = "missing_room_slot_id";
            return result;
        }
        if (options.room_control_channel != "core" && options.room_control_channel != "admin") {
            result.error = "invalid_room_control_channel";
            return result;
        }
        if (options.room_runtime_session_token.empty() && options.room_control_auth_token.empty()) {
            result.error = "missing_room_control_auth_token";
            return result;
        }
        if (options.room_runtime_session_token.empty() && effective_control_port == 0) {
            result.error = "missing_room_control_port";
            return result;
        }
    }
    if (options.mode == "archipelago") {
        if (options.ap_host.empty()) {
            options.ap_host = options.room_host;
        }
        if (options.ap_port == 0) {
            options.ap_port = options.room_port;
        }
        if (options.ap_host.empty()) {
            result.error = "missing_ap_host";
            return result;
        }
        if (options.ap_port == 0) {
            result.error = "missing_ap_port";
            return result;
        }
        if (options.ap_game.empty()) {
            result.error = "missing_ap_game";
            return result;
        }
        if (options.ap_slot_name.empty()) {
            result.error = "missing_ap_slot_name";
            return result;
        }
        if (options.ap_uuid.empty()) {
            result.error = "missing_ap_uuid";
            return result;
        }
    }

    result.ok = true;
    result.options = std::move(options);
    return result;
}

std::string RuntimeUsage() {
    std::ostringstream out;
    out << "usage: sekailink_sklmi_runtime "
        << "--memory-socket <path> "
        << "--bridge-manifest <path> "
        << "--room-state <path> "
        << "--runtime-state <dir> "
        << "--trace-log <path> "
        << "[--tracker-pack <path>] "
        << "[--tracker-variant <name>] "
        << "[--tracker-snapshot <path>] "
        << "[--tracker-command-log <path>] "
        << "[--tracker-assets-root <path>] "
        << "[--driver-instance-id <id>] "
        << "[--mode offline|sekailink_game_server|archipelago] "
        << "[--room-host <host>] "
        << "[--room-port <port>] "
        << "[--room-control-port <port>] "
        << "[--room-runtime-port <port>] "
        << "[--room-session-name <name>] "
        << "[--room-slot-id <id>] "
        << "[--room-control-channel core|admin] "
        << "[--room-control-auth-token <token>] "
        << "[--room-runtime-auth-token <token>] "
        << "[--room-runtime-session-token <token>] "
        << "[--ap-host <host>] "
        << "[--ap-port <port>] "
        << "[--ap-path <path>] "
        << "[--ap-game <game>] "
        << "[--ap-slot-name <slot>] "
        << "[--player-alias <name>] "
        << "[--ap-password <password>] "
        << "[--ap-uuid <uuid>] "
        << "[--ap-tags AP,SekaiLink,SKLMI] "
        << "[--tick-ms <ms>] "
        << "[--max-ticks <count>]";
    return out.str();
}

}  // namespace sekailink::sklmi
