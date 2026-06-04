#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace sekailink::sklmi;

namespace {

std::filesystem::path make_temp_dir() {
    const auto dir = std::filesystem::temp_directory_path() / ("sklmi-room-sync-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

BridgeManifest MakeManifest() {
    return BridgeManifest{
        .linkedworld_id = "earthbound",
        .bridge_id = "earthbound-phase1-test",
        .module = "sekaiemu",
        .state_file = "earthbound.test.bridge.state",
        .poll_interval_ms = 1,
        .checks = {
            WatchRule{
                .domain_id = "system_ram",
                .address = 0x10,
                .size = 1,
                .compare = CompareOp::mask_any,
                .operand_u64 = 0x08,
                .event_type = EventType::location_checked,
                .event_key = "0xEB0000",
                .mapped_value = "Onett - Tracy Gift",
            },
        },
        .injections = {
            InjectRule{
                .domain_id = "system_ram",
                .address = 0x20,
                .value_u64 = 0x9A,
                .size = 1,
                .item_id = 154,
                .item_name = "Toothbrush",
                .room_controlled = true,
            },
        },
    };
}

BridgeManifest MakeSelfOriginManifest() {
    auto manifest = MakeManifest();
    manifest.checks.front().location_id = 12345;
    return manifest;
}

struct SelfOriginRoomClientState {
    bool connected = false;
    bool acknowledged = false;
    std::uint64_t reported_location_id = 0;
    std::string checked_locations_json;
};

class SelfOriginRoomClient final : public RoomClient {
  public:
    explicit SelfOriginRoomClient(SelfOriginRoomClientState& state) : state_(state) {}

    bool connect(std::string*) override {
        state_.connected = true;
        return true;
    }

    void disconnect() override {
        state_.connected = false;
    }

    [[nodiscard]] bool connected() const override {
        return state_.connected;
    }

    bool report_location_checked(const Event& event, std::string*) override {
        state_.reported_location_id = event.canonical_id;
        return true;
    }

    std::vector<RoomItem> poll_pending_items(std::string*) override {
        if (state_.acknowledged) {
            return {};
        }
        RoomItem item;
        item.item_id = "ap:0:154:12345:1";
        item.event_key = "154";
        item.mapped_value = "154";
        item.value_u64 = 154;
        item.ap_item_id = 154;
        item.ap_location_id = 12345;
        item.ap_player_id = 1;
        item.item_name = "Toothbrush";
        return {item};
    }

    bool acknowledge_item(std::string_view item_id, std::string*) override {
        state_.acknowledged = item_id == "ap:0:154:12345:1";
        return state_.acknowledged;
    }

    std::vector<RoomChatMessage> poll_pending_chat(std::string*) override {
        return {};
    }

    bool send_chat_message(std::string_view, std::string*) override {
        return true;
    }

    [[nodiscard]] std::unordered_map<std::string, std::string> metadata_snapshot() const override {
        std::unordered_map<std::string, std::string> metadata{
            {"connected", state_.connected ? "1" : "0"},
            {"room_mode", "archipelago"},
            {"slot", "1"},
            {"slot_name", "Jade"},
        };
        if (!state_.checked_locations_json.empty()) {
            metadata["checked_locations"] = state_.checked_locations_json;
        }
        return metadata;
    }

  private:
    SelfOriginRoomClientState& state_;
};

void SeedOfflineRoomState(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << "meta|connected|1\n";
    output << "meta|world_id|world-eb-proof\n";
    output << "meta|seed_id|seed-eb-proof\n";
    output << "meta|seed_hash|ABC123\n";
    output << "meta|slot_data|{\"slot\":1,\"name\":\"Jade\",\"game\":\"EarthBound\"}\n";
    output << "pending|item-1|||154|154|Toothbrush\n";
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    const auto temp_dir = make_temp_dir();
    const auto room_state = temp_dir / "room.state";
    const auto bridge_state = temp_dir / "bridge.state";
    const auto room_sync_state = temp_dir / "room-sync.state";

    FakeMemoryProvider provider;
    provider.add_domain({
        .id = "system_ram",
        .display_name = "System RAM",
        .size_bytes = 0x100,
        .endianness = Endianness::little,
        .writable = true,
    });
    std::vector<std::byte> bytes(0x100, std::byte{0x00});
    bytes[0x10] = std::byte{0x08};
    provider.set_domain_bytes("system_ram", bytes);

    SeedOfflineRoomState(room_state);
    VectorEventSink sink;
    {
        RoomSynchronizedRuntimeSession session(
            provider,
            sink,
            std::make_unique<ManifestBridgeSession>(MakeManifest()),
            std::make_unique<OfflineRoomClient>(room_state),
            MakeManifest().injections,
            bridge_state,
            room_sync_state,
            "room-sync-driver-1",
            "earthbound",
            "snes_v1");

        if (session.start().state != RuntimeConnectionState::connected) {
            std::cerr << "room_session_start_failed\n";
            return EXIT_FAILURE;
        }
        if (session.tick({.tick_index = 1, .monotonic_ms = 16}).state != RuntimeConnectionState::connected) {
            std::cerr << "room_session_tick_failed\n";
            return EXIT_FAILURE;
        }
        session.stop();
    }

    std::byte injected{};
    if (!provider.read("system_ram", 0x20, &injected, 1) || injected != std::byte{0x9A}) {
        std::cerr << "room_injection_failed\n";
        return EXIT_FAILURE;
    }

    const auto room_text = slurp(room_state);
    const auto sync_text = slurp(room_sync_state);
    if (!contains(room_text, "meta|world_id|world-eb-proof") ||
        !contains(room_text, "meta|seed_id|seed-eb-proof") ||
        !contains(room_text, "meta|seed_hash|ABC123") ||
        !contains(room_text, "meta|slot_data|{\"slot\":1,\"name\":\"Jade\",\"game\":\"EarthBound\"}") ||
        !contains(room_text, "checked|0xEB0000|Onett - Tracy Gift") ||
        !contains(room_text, "consumed|item-1|||154|154|Toothbrush") ||
        contains(room_text, "pending|item-1|")) {
        std::cerr << "room_state_not_persisted\n";
        return EXIT_FAILURE;
    }
    if (!contains(sync_text, "meta|room_mode|offline") ||
        !contains(sync_text, "meta|driver_instance_id|room-sync-driver-1") ||
        !contains(sync_text, "meta|linkedworld_id|earthbound") ||
        !contains(sync_text, "meta|core_profile|snes_v1") ||
        !contains(sync_text, "meta|world_id|world-eb-proof") ||
        !contains(sync_text, "meta|seed_id|seed-eb-proof") ||
        !contains(sync_text, "meta|seed_hash|ABC123") ||
        !contains(sync_text, "meta|slot_data|{\"slot\":1,\"name\":\"Jade\",\"game\":\"EarthBound\"}") ||
        !contains(sync_text, "reported|0xEB0000|Onett - Tracy Gift") ||
        !contains(sync_text, "applied|item-1|Toothbrush")) {
        std::cerr << "room_sync_state_not_persisted\n";
        return EXIT_FAILURE;
    }

    bool saw_room_item_event = false;
    for (const auto& event : sink.events()) {
        if (event.type == EventType::item_received && event.value == "Toothbrush" &&
            event.driver_instance_id == "room-sync-driver-1" && event.linkedworld_id == "earthbound" &&
            event.core_profile == "snes_v1" && event.canonical_id == 154) {
            saw_room_item_event = true;
        }
    }
    if (!saw_room_item_event) {
        std::cerr << "room_item_event_missing_context\n";
        return EXIT_FAILURE;
    }

    provider.write_unsigned("system_ram", 0x20, 0x00, 1);
    {
        RoomSynchronizedRuntimeSession session(
            provider,
            sink,
            std::make_unique<ManifestBridgeSession>(MakeManifest()),
            std::make_unique<OfflineRoomClient>(room_state),
            MakeManifest().injections,
            bridge_state,
            room_sync_state,
            "room-sync-driver-1",
            "earthbound",
            "snes_v1");

        if (session.start().state != RuntimeConnectionState::connected) {
            std::cerr << "room_session_restart_failed\n";
            return EXIT_FAILURE;
        }
        if (session.tick({.tick_index = 2, .monotonic_ms = 32}).state != RuntimeConnectionState::connected) {
            std::cerr << "room_session_restart_tick_failed\n";
            return EXIT_FAILURE;
        }
        session.stop();
    }

    if (!provider.read("system_ram", 0x20, &injected, 1) || injected != std::byte{0x00}) {
        std::cerr << "room_item_reapplied_after_restart\n";
        return EXIT_FAILURE;
    }

    provider.write_unsigned("system_ram", 0x20, 0x00, 1);
    const auto self_origin_sync_state = temp_dir / "self-origin.room-sync.state";
    SelfOriginRoomClientState self_origin_client_state;
    auto self_origin_client = std::make_unique<SelfOriginRoomClient>(self_origin_client_state);
    VectorEventSink self_origin_sink;
    {
        RoomSynchronizedRuntimeSession session(
            provider,
            self_origin_sink,
            std::make_unique<ManifestBridgeSession>(MakeSelfOriginManifest()),
            std::move(self_origin_client),
            MakeSelfOriginManifest().injections,
            temp_dir / "self-origin.bridge.state",
            self_origin_sync_state,
            "room-sync-driver-1",
            "earthbound",
            "snes_v1");

        if (session.start().state != RuntimeConnectionState::connected) {
            std::cerr << "self_origin_session_start_failed\n";
            return EXIT_FAILURE;
        }
        if (session.tick({.tick_index = 3, .monotonic_ms = 48}).state != RuntimeConnectionState::connected) {
            std::cerr << "self_origin_session_tick_failed\n";
            return EXIT_FAILURE;
        }
        session.stop();
    }

    if (self_origin_client_state.reported_location_id != 12345 || !self_origin_client_state.acknowledged) {
        std::cerr << "self_origin_not_reported_or_acknowledged\n";
        return EXIT_FAILURE;
    }
    if (!provider.read("system_ram", 0x20, &injected, 1) || injected != std::byte{0x00}) {
        std::cerr << "self_origin_item_was_injected\n";
        return EXIT_FAILURE;
    }
    bool saw_self_origin_item_event = false;
    for (const auto& event : self_origin_sink.events()) {
        if (event.type == EventType::item_received &&
            event.key == "ap:0:154:12345:1" &&
            event.value == "Toothbrush" &&
            event.driver_instance_id == "room-sync-driver-1" &&
            event.linkedworld_id == "earthbound" &&
            event.core_profile == "snes_v1" &&
            event.canonical_id == 154) {
            saw_self_origin_item_event = true;
        }
    }
    if (!saw_self_origin_item_event) {
        std::cerr << "self_origin_item_event_missing\n";
        return EXIT_FAILURE;
    }
    const auto self_origin_sync_text = slurp(self_origin_sync_state);
    if (!contains(self_origin_sync_text, "reported|12345|Onett - Tracy Gift") ||
        !contains(self_origin_sync_text, "applied|ap:0:154:12345:1|self_origin_suppressed")) {
        std::cerr << "self_origin_sync_state_missing\n";
        return EXIT_FAILURE;
    }

    provider.write_unsigned("system_ram", 0x20, 0x00, 1);
    const auto stale_bridge_state = temp_dir / "stale-self-origin.bridge.state";
    const auto stale_room_sync_state = temp_dir / "stale-self-origin.room-sync.state";
    {
        std::ofstream bridge_out(stale_bridge_state, std::ios::trunc);
        bridge_out << "meta|bridge_id|earthbound-phase1-test\n";
        bridge_out << "check|0xEB0000|1\n";
    }
    {
        std::ofstream room_out(stale_room_sync_state, std::ios::trunc);
        room_out << "meta|connected|1\n";
        room_out << "meta|room_mode|archipelago\n";
        room_out << "meta|slot|1\n";
        room_out << "reported|12345|Onett - Tracy Gift\n";
    }
    SelfOriginRoomClientState stale_self_origin_state;
    stale_self_origin_state.checked_locations_json = "[]";
    {
        RoomSynchronizedRuntimeSession session(
            provider,
            self_origin_sink,
            std::make_unique<ManifestBridgeSession>(MakeSelfOriginManifest()),
            std::make_unique<SelfOriginRoomClient>(stale_self_origin_state),
            MakeSelfOriginManifest().injections,
            stale_bridge_state,
            stale_room_sync_state,
            "room-sync-driver-1",
            "earthbound",
            "snes_v1");

        if (session.start().state != RuntimeConnectionState::connected) {
            std::cerr << "stale_self_origin_session_start_failed\n";
            return EXIT_FAILURE;
        }
        if (session.tick({.tick_index = 4, .monotonic_ms = 64}).state != RuntimeConnectionState::connected) {
            std::cerr << "stale_self_origin_session_tick_failed\n";
            return EXIT_FAILURE;
        }
        session.stop();
    }
    if (stale_self_origin_state.reported_location_id != 12345 || !stale_self_origin_state.acknowledged) {
        std::cerr << "stale_self_origin_not_rereported\n";
        return EXIT_FAILURE;
    }
    const auto stale_self_origin_sync_text = slurp(stale_room_sync_state);
    if (!contains(stale_self_origin_sync_text, "reported|12345|Onett - Tracy Gift") ||
        !contains(stale_self_origin_sync_text, "applied|ap:0:154:12345:1|self_origin_suppressed")) {
        std::cerr << "stale_self_origin_sync_state_missing\n";
        return EXIT_FAILURE;
    }

    FakeMemoryProvider failing_provider;
    failing_provider.add_domain({
        .id = "system_ram",
        .display_name = "System RAM",
        .size_bytes = 0x100,
        .endianness = Endianness::little,
        .writable = false,
    });
    failing_provider.set_domain_bytes("system_ram", bytes);
    const auto failing_room_state = temp_dir / "room-failing.state";
    SeedOfflineRoomState(failing_room_state);
    {
        RoomSynchronizedRuntimeSession session(
            failing_provider,
            sink,
            std::make_unique<ManifestBridgeSession>(MakeManifest()),
            std::make_unique<OfflineRoomClient>(failing_room_state),
            MakeManifest().injections,
            temp_dir / "failing.bridge.state",
            temp_dir / "failing.room-sync.state",
            "room-sync-driver-1",
            "earthbound",
            "snes_v1");

        if (session.start().state != RuntimeConnectionState::connected) {
            std::cerr << "failing_room_session_start_failed\n";
            return EXIT_FAILURE;
        }
        if (session.tick({.tick_index = 3, .monotonic_ms = 48}).state != RuntimeConnectionState::connected) {
            std::cerr << "failing_room_session_tick_failed\n";
            return EXIT_FAILURE;
        }
        session.stop();
    }

    const auto failing_room_text = slurp(failing_room_state);
    if (!contains(failing_room_text, "pending|item-1|||154|154|Toothbrush") ||
        contains(failing_room_text, "consumed|item-1|||154|154|Toothbrush")) {
        std::cerr << "failed_write_acknowledged_item\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_room_sync_smoke_ok\n";
    return EXIT_SUCCESS;
}
