#include "runtime_jsonl_sink.hpp"
#include "runtime_options.hpp"
#include "sekailink_sklmi/api.hpp"
#include "tracker_headless_runtime.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

namespace sekailink::sklmi {
namespace {

std::atomic<bool> g_stop_requested{false};
constexpr std::uint64_t kTrackerPublishIntervalMs = 250;

void HandleSignal(int) {
    g_stop_requested.store(true);
}

std::string SanitizePathStem(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) return "bridge";
    return out;
}

std::filesystem::path ResolveBridgeStatePath(const BridgeManifest& manifest, const std::filesystem::path& runtime_state_root) {
    if (!manifest.state_file.empty()) {
        return runtime_state_root / manifest.state_file;
    }
    return runtime_state_root / (SanitizePathStem(manifest.bridge_id) + ".bridge.state");
}

std::filesystem::path ResolveRoomSyncStatePath(const BridgeManifest& manifest, const std::filesystem::path& runtime_state_root) {
    return runtime_state_root / (SanitizePathStem(manifest.bridge_id) + ".room-sync.state");
}

std::vector<InjectRule> FilterRoomControlledInjections(const BridgeManifest& manifest) {
    std::vector<InjectRule> rules;
    for (const auto& rule : manifest.injections) {
        if (rule.room_controlled) {
            rules.push_back(rule);
        }
    }
    return rules;
}

template <typename Provider>
bool ConnectWithRetry(Provider& provider, JsonlFileEventSink& sink, std::string* error) {
    for (int attempt = 0; attempt < 50 && !g_stop_requested.load(); ++attempt) {
        if (provider.connect(error)) {
            sink.trace({LogLevel::info, "sklmi_runtime", "provider_connect", "connected", 0, 0});
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    sink.trace({LogLevel::error, "sklmi_runtime", "provider_connect", error != nullptr ? *error : "connect_failed", 0, 0});
    return false;
}

std::optional<std::pair<std::string, std::uint16_t>> ParseTcpMemoryEndpoint(const std::string& endpoint) {
    constexpr std::string_view prefix = "tcp://";
    if (endpoint.rfind(std::string(prefix), 0) != 0) {
        return std::nullopt;
    }
    const auto rest = endpoint.substr(prefix.size());
    const auto colon = rest.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= rest.size()) {
        return std::nullopt;
    }
    const auto host = rest.substr(0, colon);
    const auto port_text = rest.substr(colon + 1);
    std::uint64_t port = 0;
    const auto result = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port, 10);
    if (result.ec != std::errc{} || result.ptr != port_text.data() + port_text.size() || port > 65535) {
        return std::nullopt;
    }
    return std::make_pair(host, static_cast<std::uint16_t>(port));
}

std::string DescribeManifest(const BridgeManifest& manifest) {
    std::ostringstream out;
    out << "bridge_id=" << manifest.bridge_id
        << " linkedworld_id=" << manifest.linkedworld_id
        << " core_profile=" << manifest.core_profile.name
        << " checks=" << manifest.checks.size()
        << " injections=" << manifest.injections.size();
    return out.str();
}

std::string DescribeRuntimeConfig(const RuntimeOptions& options,
                                  const std::filesystem::path& bridge_state_path,
                                  const std::filesystem::path& room_sync_state_path) {
    std::ostringstream out;
    out << "mode=" << options.mode
        << " tick_ms=" << options.tick_ms
        << " max_ticks=";
    if (options.max_ticks.has_value()) {
        out << *options.max_ticks;
    } else {
        out << "unbounded";
    }
    out << " room_state=" << options.room_state_path.string()
        << " bridge_state=" << bridge_state_path.string()
        << " room_sync_state=" << room_sync_state_path.string()
        << " trace_log=" << options.trace_log_path.string();
    return out.str();
}

std::string DescribeRoomClient(const RuntimeOptions& options) {
    const auto control_port = options.room_control_port != 0 ? options.room_control_port : options.room_port;
    const auto runtime_port = options.room_runtime_port != 0 ? options.room_runtime_port : options.room_port;
    std::ostringstream out;
    out << "mode=" << options.mode;
    if (options.mode == "offline") {
        out << " room_state=" << options.room_state_path.string();
        return out.str();
    }
    if (options.mode == "archipelago") {
        out << " ap_host=" << options.ap_host
            << " ap_port=" << options.ap_port
            << " ap_path=" << options.ap_path
            << " ap_game=" << options.ap_game
            << " ap_slot_name=" << options.ap_slot_name;
        if (!options.player_alias.empty()) {
            out << " player_alias=" << options.player_alias;
        }
        return out.str();
    }
    out << " host=" << options.room_host
        << " control_port=" << control_port
        << " runtime_port=" << runtime_port
        << " session_name=" << options.room_session_name
        << " slot_id=" << options.room_slot_id
        << " control_channel=" << options.room_control_channel
        << " runtime_session_token=" << (options.room_runtime_session_token.empty() ? "issued_at_connect" : "provided");
    return out.str();
}

std::string DescribeMemoryProvider(const MemoryProvider& provider) {
    std::ostringstream out;
    const auto domains = provider.domains();
    out << "domains=";
    for (std::size_t i = 0; i < domains.size(); ++i) {
        const auto& domain = domains[i];
        if (i != 0) {
            out << ",";
        }
        out << domain.id
            << "(size=" << domain.size_bytes
            << ",writable=" << (domain.writable ? "true" : "false")
            << ",endianness=" << (domain.endianness == Endianness::big ? "big" : "little")
            << ")";
    }
    return out.str();
}

}  // namespace
}  // namespace sekailink::sklmi

int main(int argc, char** argv) {
    using namespace sekailink::sklmi;

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    std::vector<std::string> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const auto parse_result = ParseRuntimeOptions(args);
    if (!parse_result.ok) {
        std::cerr << parse_result.error << "\n" << RuntimeUsage() << "\n";
        return 1;
    }
    if (parse_result.show_help) {
        std::cout << RuntimeUsage() << "\n";
        return 0;
    }

    const auto& options = parse_result.options;
    JsonlFileEventSink sink(options.trace_log_path);
    if (!sink.good()) {
        std::cerr << "trace_log_open_failed\n";
        return 1;
    }

    std::unique_ptr<TrackerHeadlessRuntime> tracker_runtime;
    std::unique_ptr<TrackerForwardingEventSink> tracker_sink;
    EventSink* active_sink = &sink;
    if (!options.tracker_snapshot_path.empty()) {
        if (options.tracker_pack_path.empty()) {
            std::cerr << "tracker_pack_missing\n";
            return 1;
        }
        tracker_runtime = std::make_unique<TrackerHeadlessRuntime>();
        std::string tracker_error;
        if (!tracker_runtime->Initialize(
                {.bundle_path = options.tracker_pack_path,
                 .snapshot_path = options.tracker_snapshot_path,
                 .command_log_path = options.tracker_command_log_path,
                 .room_state_path = options.room_state_path,
                 .assets_root = options.tracker_assets_root,
                 .tracker_variant = options.tracker_variant,
                 .initial_player_alias = options.player_alias},
                &tracker_error)) {
            std::cerr << "tracker_init_failed:" << tracker_error << "\n";
            return 1;
        }
        tracker_sink = std::make_unique<TrackerForwardingEventSink>(sink, *tracker_runtime);
        active_sink = tracker_sink.get();
    }

    std::string manifest_error;
    auto manifest = load_bridge_manifest(options.bridge_manifest_path, &manifest_error);
    if (!manifest.has_value()) {
        sink.trace({LogLevel::error, "sklmi_runtime", "manifest_load", manifest_error, 0, 0});
        std::cerr << "manifest_load_failed:" << manifest_error << "\n";
        return 1;
    }
    if (!options.driver_instance_id.empty()) {
        manifest->driver_instance_id = options.driver_instance_id;
    }
    sink.trace({LogLevel::info, "sklmi_runtime", "manifest_loaded", DescribeManifest(*manifest), 0, 0});

    std::filesystem::create_directories(options.runtime_state_root);
    const auto bridge_state_path = ResolveBridgeStatePath(*manifest, options.runtime_state_root);
    const auto room_sync_state_path = ResolveRoomSyncStatePath(*manifest, options.runtime_state_root);
    sink.trace({LogLevel::info, "sklmi_runtime", "runtime_config",
                DescribeRuntimeConfig(options, bridge_state_path, room_sync_state_path), 0, 0});

    std::string connect_error;
    std::unique_ptr<MemoryProvider> provider;
    const auto memory_endpoint = options.memory_socket_path.string();
    if (const auto tcp_endpoint = ParseTcpMemoryEndpoint(memory_endpoint); tcp_endpoint.has_value()) {
        auto tcp_provider =
            std::make_unique<RuntimeSocketMemoryProvider>(tcp_endpoint->first, tcp_endpoint->second);
        if (!ConnectWithRetry(*tcp_provider, sink, &connect_error)) {
            std::cerr << "provider_connect_failed:" << connect_error << "\n";
            return 1;
        }
        provider = std::move(tcp_provider);
    } else {
#if defined(_WIN32)
        std::cerr << "provider_connect_failed:windows_requires_tcp_memory_endpoint\n";
        return 1;
#else
        auto unix_provider = std::make_unique<UnixSocketMemoryProvider>(options.memory_socket_path);
        if (!ConnectWithRetry(*unix_provider, sink, &connect_error)) {
            std::cerr << "provider_connect_failed:" << connect_error << "\n";
            return 1;
        }
        provider = std::move(unix_provider);
#endif
    }
    sink.trace({LogLevel::info, "sklmi_runtime", "provider_metadata", DescribeMemoryProvider(*provider), 0, 0});

    std::unique_ptr<RoomClient> room_client;
    if (options.mode == "offline") {
        room_client = std::make_unique<OfflineRoomClient>(options.room_state_path);
    } else if (options.mode == "archipelago") {
        ArchipelagoConnectOptions connect_options;
        connect_options.game = options.ap_game;
        connect_options.slot_name = options.ap_slot_name;
        connect_options.player_alias = options.player_alias;
        connect_options.password = options.ap_password;
        connect_options.uuid = options.ap_uuid;
        connect_options.tags = options.ap_tags;
        room_client = std::make_unique<ArchipelagoRoomClient>(
            std::make_unique<TcpWebSocketArchipelagoTransport>(options.ap_host, options.ap_port, options.ap_path),
            std::move(connect_options));
    } else {
        room_client = std::make_unique<GameServerRoomClient>(
            options.room_host,
            options.room_control_port != 0 ? options.room_control_port : options.room_port,
            options.room_runtime_port != 0 ? options.room_runtime_port : options.room_port,
            options.room_session_name,
            options.room_slot_id,
            options.room_control_channel,
            options.room_control_auth_token,
            options.room_runtime_auth_token,
            manifest->driver_instance_id,
            manifest->linkedworld_id,
            manifest->core_profile.name,
            options.room_runtime_session_token);
    }
    sink.trace({LogLevel::info, "sklmi_runtime", "room_client_ready", DescribeRoomClient(options), 0, 0});

    std::uint64_t next_tracker_publish_ms = 0;
    auto publish_tracker = [&](std::uint64_t monotonic_ms, bool force) -> bool {
        if (tracker_runtime == nullptr) {
            return true;
        }
        const bool command_changed = tracker_runtime->PollCommands();
        if (command_changed) {
            std::string tracker_error;
            if (!tracker_runtime->PublishSnapshotFastIfChanged(&tracker_error)) {
                std::cerr << "tracker_snapshot_failed:" << tracker_error << "\n";
                return false;
            }
            next_tracker_publish_ms = monotonic_ms + kTrackerPublishIntervalMs;
            if (!force) {
                return true;
            }
        }
        if (!force && monotonic_ms < next_tracker_publish_ms) {
            return true;
        }
        std::string tracker_error;
        if (!tracker_runtime->PublishSnapshotIfChanged(&tracker_error)) {
            std::cerr << "tracker_snapshot_failed:" << tracker_error << "\n";
            return false;
        }
        next_tracker_publish_ms = monotonic_ms + kTrackerPublishIntervalMs;
        return true;
    };

    if (tracker_runtime != nullptr) {
        if (!publish_tracker(0, true)) {
            return 1;
        }
        sink.trace({LogLevel::info, "sklmi_runtime", "tracker_initial_snapshot", "published_before_room_session", 0, 0});
    }

    auto session = RoomSynchronizedRuntimeSession(
        *provider,
        *active_sink,
        std::make_unique<ManifestBridgeSession>(*manifest),
        std::move(room_client),
        FilterRoomControlledInjections(*manifest),
        bridge_state_path,
        room_sync_state_path,
        manifest->driver_instance_id,
        manifest->linkedworld_id,
        manifest->core_profile.name);
    if (tracker_runtime != nullptr) {
        tracker_runtime->SetChatMessageSender([&session](std::string_view text, std::string* error) {
            return session.send_chat_message(text, error);
        });
    }

    auto status = session.start();
    sink.trace({status.state == RuntimeConnectionState::connected ? LogLevel::info : LogLevel::error,
                "sklmi_runtime", "session_start", status.detail, 0, 0});
    if (status.state != RuntimeConnectionState::connected) {
        std::cerr << "session_start_failed:" << status.detail << "\n";
        return 1;
    }
    if (tracker_runtime != nullptr) {
        if (!publish_tracker(0, true)) {
            return 1;
        }
    }

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t tick_index = 0;
    while (!g_stop_requested.load()) {
        ++tick_index;
        const auto now = std::chrono::steady_clock::now();
        const auto monotonic_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
        status = session.tick({tick_index, monotonic_ms});
        if (status.state != RuntimeConnectionState::connected) {
            sink.trace({LogLevel::error, "sklmi_runtime", "session_tick", status.detail, tick_index, monotonic_ms});
            session.stop();
            std::cerr << "session_tick_failed:" << status.detail << "\n";
            return 1;
        }
        if (tracker_runtime != nullptr) {
            if (!publish_tracker(monotonic_ms, false)) {
                session.stop();
                return 1;
            }
        }
        if (options.max_ticks.has_value() && tick_index >= *options.max_ticks) {
            sink.trace({LogLevel::info, "sklmi_runtime", "stop_condition", "max_ticks_reached", tick_index, monotonic_ms});
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(options.tick_ms));
    }

    if (tracker_runtime != nullptr) {
        const auto final_now = std::chrono::steady_clock::now();
        const auto final_monotonic_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(final_now - start).count());
        if (!publish_tracker(final_monotonic_ms, true)) {
            session.stop();
            return 1;
        }
    }

    const auto stop_status = session.stop();
    sink.trace({LogLevel::info, "sklmi_runtime", "session_stop", stop_status.detail, tick_index, 0});
    return stop_status.state == RuntimeConnectionState::stopped ? 0 : 1;
}
