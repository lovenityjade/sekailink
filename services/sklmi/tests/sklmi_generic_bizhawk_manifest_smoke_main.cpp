#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace sekailink::sklmi;

namespace {

class SingleItemRoomClient final : public RoomClient {
  public:
    explicit SingleItemRoomClient(RoomItem item) : item_(std::move(item)) {}

    bool connect(std::string* = nullptr) override {
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    [[nodiscard]] bool connected() const override {
        return connected_;
    }

    bool report_location_checked(const Event& event, std::string* = nullptr) override {
        reported_locations_.push_back(event.canonical_id);
        return true;
    }

    std::vector<RoomItem> poll_pending_items(std::string* = nullptr) override {
        if (item_delivered_) {
            return {};
        }
        item_delivered_ = true;
        return {item_};
    }

    bool acknowledge_item(std::string_view item_id, std::string* = nullptr) override {
        acknowledged_item_id_ = std::string(item_id);
        return true;
    }

    std::vector<RoomChatMessage> poll_pending_chat(std::string* = nullptr) override {
        return {};
    }

    bool send_chat_message(std::string_view, std::string* = nullptr) override {
        return true;
    }

    [[nodiscard]] std::unordered_map<std::string, std::string> metadata_snapshot() const override {
        return {
            {"slot", "1"},
            {"checked_locations", "[]"},
            {"room_mode", "smoke"},
        };
    }

    std::vector<std::uint64_t> reported_locations_;
    std::string acknowledged_item_id_;

  private:
    RoomItem item_;
    bool connected_ = false;
    bool item_delivered_ = false;
};

FakeMemoryProvider MakeProvider(std::string domain_id, std::size_t size, bool writable = true) {
    FakeMemoryProvider provider;
    provider.add_domain({
        .id = std::move(domain_id),
        .display_name = "Runtime Memory",
        .size_bytes = size,
        .endianness = Endianness::little,
        .writable = writable,
    });
    return provider;
}

BridgeManifest LoadManifest(const std::filesystem::path& source_root, const std::string& name) {
    std::string error;
    const auto path = source_root / "manifests" / name;
    auto manifest = load_bridge_manifest(path, &error);
    if (!manifest.has_value()) {
        std::cerr << "manifest_load_failed:" << name << ":" << error << "\n";
        std::exit(EXIT_FAILURE);
    }
    return *manifest;
}

bool SawLocation(const VectorEventSink& sink, std::uint64_t location_id) {
    for (const auto& event : sink.events()) {
        if (event.type == EventType::location_checked && event.canonical_id == location_id) {
            return true;
        }
    }
    return false;
}

void ExpectConnected(const RuntimeStatus& status, std::string_view label) {
    if (status.state != RuntimeConnectionState::connected) {
        std::cerr << label << "_failed:" << status.detail << "\n";
        std::exit(EXIT_FAILURE);
    }
}

void RunTlozSmoke(const std::filesystem::path& source_root) {
    auto manifest = LoadManifest(source_root, "tloz.phase1.json");
    auto provider = MakeProvider("system_ram", 0x800);
    provider.write_unsigned("system_ram", 0x06F6, 0x10, 1);

    VectorEventSink sink;
    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(manifest));
    ExpectConnected(session.start(), "tloz_start");
    ExpectConnected(session.tick({.tick_index = 1, .monotonic_ms = 16}), "tloz_tick");

    if (!SawLocation(sink, 7000)) {
        std::cerr << "tloz_missing_starting_sword_check\n";
        std::exit(EXIT_FAILURE);
    }
}

void RunLadxSmoke(const std::filesystem::path& source_root) {
    auto manifest = LoadManifest(source_root, "ladx.phase1.json");
    auto provider = MakeProvider("system_ram", 0x10000);
    provider.write_unsigned("system_ram", 0xDAA3, 0x20, 1);

    VectorEventSink sink;
    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(manifest));
    ExpectConnected(session.start(), "ladx_start");
    ExpectConnected(session.tick({.tick_index = 1, .monotonic_ms = 16}), "ladx_tick");

    if (!SawLocation(sink, 10000675)) {
        std::cerr << "ladx_missing_tarins_gift_check\n";
        std::exit(EXIT_FAILURE);
    }
}

void RunFireRedSmoke(const std::filesystem::path& source_root) {
    auto manifest = LoadManifest(source_root, "firered.phase1.json");
    auto provider = MakeProvider("gba_system_bus", 0x04000000);
    constexpr std::uint64_t kSaveBlock1 = 0x02030000;
    provider.write_unsigned("gba_system_bus", 0x03003078, 1, 1);
    provider.write_unsigned("gba_system_bus", 0x03004F58, kSaveBlock1, 4);
    provider.write_unsigned("gba_system_bus", kSaveBlock1 + 0x1130 + (740 / 8), 1u << (740 % 8), 1);
    provider.write_unsigned("gba_system_bus", kSaveBlock1 + 0x3DE8, 41, 2);
    provider.write_unsigned("gba_system_bus", 0x0203F718, 0, 1);

    VectorEventSink sink;
    auto room_client = std::make_unique<SingleItemRoomClient>(RoomItem{
        .item_id = "delivery-1",
        .value_u64 = 0,
        .ap_item_id = 13,
        .item_name = "Potion",
    });

    RoomSynchronizedRuntimeSession session(
        provider,
        sink,
        std::make_unique<ManifestBridgeSession>(manifest),
        std::move(room_client),
        manifest.injections,
        {},
        {},
        manifest.driver_instance_id,
        manifest.linkedworld_id,
        manifest.core_profile.name);

    ExpectConnected(session.start(), "firered_start");
    ExpectConnected(session.tick({.tick_index = 1, .monotonic_ms = 16}), "firered_tick");

    if (!SawLocation(sink, 740)) {
        std::cerr << "firered_missing_dynamic_flag_check\n";
        std::exit(EXIT_FAILURE);
    }
    if (provider.read_unsigned("gba_system_bus", 0x0203F714, 2).value_or(0) != 0x000D ||
        provider.read_unsigned("gba_system_bus", 0x0203F716, 2).value_or(0) != 42 ||
        provider.read_unsigned("gba_system_bus", 0x0203F718, 1).value_or(0) != 1 ||
        provider.read_unsigned("gba_system_bus", 0x0203F719, 1).value_or(0) != 1) {
        std::cerr << "firered_dynamic_injection_failed\n";
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: sklmi_generic_bizhawk_manifest_smoke <sklmi-source-root>\n";
        return EXIT_FAILURE;
    }
    const std::filesystem::path source_root = argv[1];
    RunTlozSmoke(source_root);
    RunLadxSmoke(source_root);
    RunFireRedSmoke(source_root);
    return EXIT_SUCCESS;
}
