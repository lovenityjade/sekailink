#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <iostream>

using namespace sekailink::sklmi;

int main() {
    FakeMemoryProvider provider;
    provider.add_domain({
        .id = "system_ram",
        .display_name = "System RAM",
        .size_bytes = 0xB600,
        .endianness = Endianness::little,
        .writable = true,
    });

    std::vector<std::byte> before(0xB600, std::byte{0x00});
    before[0x9C11] = std::byte{0x00};
    before[0x9C6C] = std::byte{0x00};
    before[0xB570] = std::byte{0x00};
    provider.set_domain_bytes("system_ram", before);

    BridgeManifest manifest{
        .linkedworld_id = "earthbound",
        .bridge_id = "earthbound-sekaiemu-phase0",
        .module = "sekaiemu",
        .state_file = "",
        .poll_interval_ms = 1,
        .checks = {
            WatchRule{
                .domain_id = "system_ram",
                .address = 0x9C11,
                .size = 1,
                .compare = CompareOp::mask_any,
                .operand_u64 = 0x08,
                .event_type = EventType::location_checked,
                .event_key = "0xEB0000",
                .mapped_value = "Onett - Tracy Gift",
            },
            WatchRule{
                .domain_id = "system_ram",
                .address = 0x9C6C,
                .size = 1,
                .compare = CompareOp::mask_any,
                .operand_u64 = 0x10,
                .event_type = EventType::location_checked,
                .event_key = "0xEB0001",
                .mapped_value = "Onett - Tracy's Room Present",
            },
        },
        .injections = {
            InjectRule{
                .domain_id = "system_ram",
                .address = 0xB570,
                .value_u64 = 0x9A,
                .size = 1,
                .event_key = "item.toothbrush",
                .mapped_value = "Toothbrush",
            },
        },
    };

    VectorEventSink sink;
    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(manifest));
    if (session.start().state != RuntimeConnectionState::connected) {
        std::cerr << "start_failed\n";
        return EXIT_FAILURE;
    }

    std::vector<std::byte> after = before;
    after[0x9C11] = std::byte{0x08};
    after[0x9C6C] = std::byte{0x10};
    provider.set_domain_bytes("system_ram", after);

    if (session.tick({.tick_index = 1, .monotonic_ms = 16}).state != RuntimeConnectionState::connected) {
        std::cerr << "tick_failed\n";
        return EXIT_FAILURE;
    }

    std::byte injected{};
    if (!provider.read("system_ram", 0xB570, &injected, 1) || injected != std::byte{0x9A}) {
        std::cerr << "inject_failed\n";
        return EXIT_FAILURE;
    }

    bool saw_tracy_gift = false;
    bool saw_tracy_present = false;
    bool saw_item = false;
    for (const auto& event : sink.events()) {
        if (event.type == EventType::location_checked && event.key == "0xEB0000") saw_tracy_gift = true;
        if (event.type == EventType::location_checked && event.key == "0xEB0001") saw_tracy_present = true;
        if (event.type == EventType::item_received && event.value == "Toothbrush") saw_item = true;
    }

    if (!saw_tracy_gift || !saw_tracy_present || !saw_item) {
        std::cerr << "missing_expected_events\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_earthbound_golden_ok\n";
    return EXIT_SUCCESS;
}
