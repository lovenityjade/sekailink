#include "sekailink_sklmi/api.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace sekailink::sklmi;

int main() {
    FakeMemoryProvider provider;
    provider.add_domain({
        .id = "system_ram",
        .display_name = "System RAM",
        .size_bytes = 0xF600,
        .endianness = Endianness::little,
        .writable = true,
    });

    std::vector<std::byte> bytes(0xF600, std::byte{0x00});
    bytes[0xF4D0] = std::byte{0x0C};
    bytes[0xF4D1] = std::byte{0x00};
    provider.set_domain_bytes("system_ram", bytes);

    const auto root = std::filesystem::temp_directory_path() / "sekailink-sklmi-alttp-golden";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto manifest_path = root / "linkedworld.json";
    {
        std::ofstream manifest(manifest_path);
        manifest << R"({
  "id": "alttp",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "alttp-sekaiemu-phase0",
    "driver_instance_id": "alttp-golden-driver",
    "core_profile": {
      "name": "snes_v1",
      "domains": [
        { "id": "system_ram" }
      ]
    },
    "module": "linkedworld.sklmi",
    "poll_interval_ms": 1,
    "checks": [
      {
        "domain_id": "system_ram",
        "address": 61476,
        "size": 1,
        "compare": "mask_any",
        "operand_u64": 16,
        "event_type": "location_checked",
        "event_key": "0xEA79",
        "mapped_value": "Sanctuary",
        "location_id": 60025,
        "location_name": "Sanctuary"
      },
      {
        "domain_id": "system_ram",
        "address": 61960,
        "size": 1,
        "compare": "mask_any",
        "operand_u64": 16,
        "event_type": "location_checked",
        "event_key": "0xE9BC",
        "mapped_value": "Link's House",
        "location_id": 59836,
        "location_name": "Link's House"
      }
    ],
    "actions": [
      {
        "event_key": "item.hookshot",
        "mapped_value": "Hookshot",
        "item_id": 10,
        "item_name": "Hookshot",
        "writes": [
          {
            "domain_id": "system_ram",
            "address": 62672,
            "size": 2,
            "source": "current_plus",
            "delta_u64": 1
          },
          {
            "domain_id": "system_ram",
            "address": 62674,
            "size": 1,
            "source": "constant",
            "value_u64": 10
          },
          {
            "domain_id": "system_ram",
            "address": 62675,
            "size": 1,
            "source": "constant",
            "value_u64": 0
          }
        ]
      }
    ]
  }
})";
    }

    std::string manifest_error;
    const auto manifest = load_bridge_manifest(manifest_path, &manifest_error);
    if (!manifest.has_value()) {
        std::cerr << "manifest_load_failed:" << manifest_error << "\n";
        return EXIT_FAILURE;
    }
    if (manifest->linkedworld_id != "alttp" || manifest->checks.size() != 2 || manifest->injections.size() != 1 ||
        manifest->injections.front().writes.size() != 3 || manifest->core_profile.domain_ids.size() != 1) {
        std::cerr << "manifest_decode_invalid\n";
        return EXIT_FAILURE;
    }

    VectorEventSink sink;
    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(*manifest));
    if (session.start().state != RuntimeConnectionState::connected) {
        std::cerr << "start_failed\n";
        return EXIT_FAILURE;
    }

    bytes[0xF024] = std::byte{0x10};
    bytes[0xF208] = std::byte{0x10};
    provider.set_domain_bytes("system_ram", bytes);

    if (session.tick({.tick_index = 1, .monotonic_ms = 16}).state != RuntimeConnectionState::connected) {
        std::cerr << "tick_failed\n";
        return EXIT_FAILURE;
    }

    std::byte item{};
    std::byte player{};
    std::array<std::byte, 2> progress{};
    if (!provider.read("system_ram", 0xF4D0, progress.data(), progress.size()) ||
        !provider.read("system_ram", 0xF4D2, &item, 1) ||
        !provider.read("system_ram", 0xF4D3, &player, 1)) {
        std::cerr << "readback_failed\n";
        return EXIT_FAILURE;
    }

    const std::uint16_t progress_value =
        static_cast<std::uint16_t>(std::to_integer<unsigned char>(progress[0])) |
        (static_cast<std::uint16_t>(std::to_integer<unsigned char>(progress[1])) << 8U);

    if (progress_value != 0x000D || item != std::byte{0x0A} || player != std::byte{0x00}) {
        std::cerr << "injection_state_invalid\n";
        return EXIT_FAILURE;
    }

    bool saw_sanctuary = false;
    bool saw_links_house = false;
    bool saw_hookshot = false;
    for (const auto& event : sink.events()) {
        if (event.type == EventType::location_checked && event.key == "0xEA79") saw_sanctuary = true;
        if (event.type == EventType::location_checked && event.key == "0xE9BC") saw_links_house = true;
        if (event.type == EventType::item_received && event.value == "Hookshot") saw_hookshot = true;
    }

    if (!saw_sanctuary || !saw_links_house || !saw_hookshot) {
        std::cerr << "missing_expected_events\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_alttp_golden_ok\n";
    return EXIT_SUCCESS;
}
