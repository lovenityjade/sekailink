#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace sekailink::sklmi;

int main() {
    FakeMemoryProvider provider;
    provider.add_domain({
        .id = "WRAM",
        .display_name = "Work RAM",
        .size_bytes = 16,
        .endianness = Endianness::little,
        .writable = true,
    });

    std::vector<std::byte> bytes(16);
    bytes[4] = std::byte{0x7F};
    if (!provider.set_domain_bytes("WRAM", bytes)) {
        std::cerr << "set_domain_bytes_failed\n";
        return EXIT_FAILURE;
    }

    const auto root = std::filesystem::temp_directory_path() / "sekailink-sklmi-smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto manifest_path = root / "bridge-manifest.json";
    const auto state_path = root / "bridge.state";
    {
        std::ofstream manifest(manifest_path);
        manifest << R"({
  "id": "earthbound",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "earthbound-smoke",
    "driver_instance_id": "driver-smoke-1",
    "core_profile": {
      "name": "snes_v1",
      "domains": [
        { "id": "WRAM" }
      ]
    },
    "state_file": "bridge.state",
    "poll_interval_ms": 33,
	    "checks": [
	      {
        "domain_id": "WRAM",
        "address": 4,
        "size": 1,
        "compare": "gte",
        "operand_u64": 127,
        "event_type": "location_checked",
        "location_id": 42001,
        "location_name": "Onett - Test"
      },
      {
        "domain_id": "WRAM",
        "address": 6,
        "size": 1,
        "compare": "mask_any",
        "operand_u64": 4,
        "event_type": "map_changed",
        "event_key": "map.onett",
	        "mapped_value": "Onett"
	      }
	    ],
	    "context_watches": [
	      {
	        "domain_id": "WRAM",
	        "address": 10,
	        "size": 1,
	        "context_key": "smoke.room",
	        "event_type": "map_changed",
	        "values": [
	          {
	            "value": 2,
	            "event_key": "room.two",
	            "mapped_value": "Room Two",
	            "tab_id": "dungeon",
	            "map_id": "dungeon_map",
	            "zone_id": "room_two"
	          }
	        ]
	      }
	    ],
	    "actions": [
      {
        "domain_id": "WRAM",
        "address": 8,
        "size": 1,
        "value_u64": 1,
        "item_id": 1337,
        "item_name": "Cookie"
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
	    if (manifest->linkedworld_id != "earthbound" || manifest->checks.size() != 2 ||
	        manifest->context_watches.size() != 1 || manifest->injections.size() != 1) {
	        std::cerr << "manifest_invalid\n";
	        return EXIT_FAILURE;
	    }
	    if (manifest->contract_version != "1.0" || manifest->core_profile.name != "snes_v1" ||
	        manifest->driver_instance_id != "driver-smoke-1" ||
	        manifest->checks[0].compare != CompareOp::greater_or_equal || manifest->checks[1].event_type != EventType::map_changed ||
	        manifest->checks[0].event_key != "42001" || manifest->checks[0].mapped_value != "Onett - Test" ||
	        manifest->context_watches[0].context_key != "smoke.room" ||
	        manifest->context_watches[0].values[0].tab_id != "dungeon" ||
	        manifest->context_watches[0].values[0].map_id != "dungeon_map" ||
	        manifest->injections[0].event_key != "1337" || manifest->injections[0].mapped_value != "Cookie") {
	        std::cerr << "manifest_rule_decode_failed\n";
	        return EXIT_FAILURE;
	    }

    VectorEventSink sink;
    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(*manifest), state_path);

    const auto started = session.start();
    if (started.state != RuntimeConnectionState::connected) {
        std::cerr << "start_failed:" << started.detail << "\n";
        return EXIT_FAILURE;
    }

	    bytes[6] = std::byte{0x04};
	    bytes[10] = std::byte{0x02};
	    if (!provider.set_domain_bytes("WRAM", bytes)) {
        std::cerr << "state_set_failed\n";
        return EXIT_FAILURE;
    }

    const auto skipped = session.tick({.tick_index = 1, .monotonic_ms = 16});
    if (skipped.detail != "tick_ok") {
        std::cerr << "first_tick_failed:" << skipped.detail << "\n";
        return EXIT_FAILURE;
    }

    const auto ticked = session.tick({.tick_index = 2, .monotonic_ms = 64});
    if (ticked.state != RuntimeConnectionState::connected) {
        std::cerr << "tick_failed:" << ticked.detail << "\n";
        return EXIT_FAILURE;
    }

    std::byte injected{};
    if (!provider.read("WRAM", 8, &injected, 1) || injected != std::byte{0x01}) {
        std::cerr << "inject_failed\n";
        return EXIT_FAILURE;
    }

    bool saw_check = false;
	    bool saw_item = false;
	    bool saw_map = false;
	    bool saw_context_map = false;
	    bool saw_reset = false;
    for (const auto& event : sink.events()) {
        if (event.type == EventType::location_checked && event.key == "42001" && event.value == "Onett - Test" &&
            event.driver_instance_id == "driver-smoke-1" && event.linkedworld_id == "earthbound" &&
            event.core_profile == "snes_v1" && event.canonical_id == 42001) {
            saw_check = true;
        }
        if (event.type == EventType::item_received && event.key == "1337" && event.value == "Cookie" &&
            event.driver_instance_id == "driver-smoke-1" && event.linkedworld_id == "earthbound" &&
            event.core_profile == "snes_v1" && event.canonical_id == 1337) {
            saw_item = true;
        }
	        if (event.type == EventType::map_changed && event.value == "Onett") saw_map = true;
	        if (event.type == EventType::map_changed && event.key == "room.two" &&
	            event.value == "Room Two" && event.tab_id == "dungeon" &&
	            event.map_id == "dungeon_map" && event.zone_id == "room_two") {
	            saw_context_map = true;
	        }
	    }

	    if (!saw_check || !saw_item || !saw_map || !saw_context_map) {
	        std::cerr << "missing_expected_events\n";
	        return EXIT_FAILURE;
	    }
    if (!std::filesystem::exists(state_path)) {
        std::cerr << "state_file_missing\n";
        return EXIT_FAILURE;
    }

    const auto reset = session.reset();
    if (reset.detail != "reset") {
        std::cerr << "reset_failed\n";
        return EXIT_FAILURE;
    }
    for (const auto& event : sink.events()) {
        if (event.type == EventType::runtime_reset) saw_reset = true;
    }
    if (!saw_reset) {
        std::cerr << "missing_reset_event\n";
        return EXIT_FAILURE;
    }

    const auto reconnect = session.reconnect();
    if (reconnect.state != RuntimeConnectionState::connected) {
        std::cerr << "reconnect_failed\n";
        return EXIT_FAILURE;
    }
    const auto after_reconnect = session.tick({.tick_index = 3, .monotonic_ms = 128});
    if (after_reconnect.state != RuntimeConnectionState::connected) {
        std::cerr << "reconnect_tick_failed\n";
        return EXIT_FAILURE;
    }
    if (sink.traces().empty()) {
        std::cerr << "missing_trace_records\n";
        return EXIT_FAILURE;
    }
    const auto formatted = format_trace_record(sink.traces().front());
    if (formatted.find("[sklmi]") == std::string::npos) {
        std::cerr << "trace_format_invalid\n";
        return EXIT_FAILURE;
    }

    const auto stopped = session.stop();
    if (stopped.state != RuntimeConnectionState::stopped) {
        std::cerr << "stop_failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_smoke_ok\n";
    return EXIT_SUCCESS;
}
