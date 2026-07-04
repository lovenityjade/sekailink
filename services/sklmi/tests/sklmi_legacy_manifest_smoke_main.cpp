#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace sekailink::sklmi;

int main() {
    const auto root = std::filesystem::temp_directory_path() / "sekailink-sklmi-legacy-smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto legacy_manifest_path = root / "legacy-bridge.json";
    {
        std::ofstream manifest(legacy_manifest_path);
        manifest << R"({
  "linkedworld_id": "earthbound",
  "bridge_id": "earthbound-legacy",
  "module": "manifest",
  "state_file": "earthbound-legacy.state",
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
      "location_name": "Onett - Legacy Test"
    }
  ],
  "injections": [
    {
      "domain_id": "WRAM",
      "address": 8,
      "size": 1,
      "value_u64": 1,
      "item_id": 1337,
      "item_name": "Cookie"
    }
  ]
})";
    }

    std::string error;
    const auto legacy_manifest = load_bridge_manifest(legacy_manifest_path, &error);
    if (!legacy_manifest.has_value()) {
        std::cerr << "legacy_manifest_load_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    if (legacy_manifest->linkedworld_id != "earthbound" || legacy_manifest->contract_version != "legacy-bridge-v1" ||
        legacy_manifest->core_profile.name != "legacy.default" ||
        legacy_manifest->driver_instance_id != "earthbound-legacy" || legacy_manifest->checks.size() != 1 ||
        legacy_manifest->injections.size() != 1 || legacy_manifest->checks.front().event_key != "42001" ||
        legacy_manifest->injections.front().event_key != "1337") {
        std::cerr << "legacy_manifest_decode_invalid\n";
        return EXIT_FAILURE;
    }

    const auto forbidden_manifest_path = root / "forbidden-linkedworld.json";
    {
        std::ofstream manifest(forbidden_manifest_path);
        manifest << R"({
  "id": "earthbound",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "earthbound-scripted",
    "driver_instance_id": "earthbound-scripted",
    "core_profile": {
      "name": "snes_v1",
      "domains": [
        { "id": "WRAM" }
      ]
    },
    "checks": [
      {
        "domain_id": "WRAM",
        "address": 4,
        "size": 1,
        "compare": "equals",
        "operand_u64": 1,
        "event_type": "location_checked",
        "location_id": 42001,
        "script": "return true;"
      }
    ]
  }
})";
    }

    error.clear();
    const auto forbidden_manifest = load_bridge_manifest(forbidden_manifest_path, &error);
    if (forbidden_manifest.has_value()) {
        std::cerr << "forbidden_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "manifest_contains_forbidden_programming_fields") {
        std::cerr << "unexpected_forbidden_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto invalid_write_source_manifest_path = root / "invalid-write-source.json";
    {
        std::ofstream manifest(invalid_write_source_manifest_path);
        manifest << R"({
  "id": "earthbound",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "earthbound-invalid-write-source",
    "driver_instance_id": "earthbound-invalid-write-source",
    "core_profile": {
      "name": "snes_v1",
      "domains": [
        { "id": "WRAM" }
      ]
    },
    "checks": [
      {
        "domain_id": "WRAM",
        "address": 4,
        "size": 1,
        "compare": "equals",
        "operand_u64": 1,
        "event_type": "location_checked",
        "location_id": 42001,
        "location_name": "Onett - Invalid Write Source"
      }
    ],
    "actions": [
      {
        "item_id": 1337,
        "item_name": "Cookie",
        "writes": [
          {
            "domain_id": "WRAM",
            "address": 8,
            "size": 1,
            "source": "eval_runtime",
            "value_u64": 1
          }
        ]
      }
    ]
  }
})";
    }

    error.clear();
    const auto invalid_write_source_manifest = load_bridge_manifest(invalid_write_source_manifest_path, &error);
    if (invalid_write_source_manifest.has_value()) {
        std::cerr << "invalid_write_source_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "manifest_action_invalid_write_source") {
        std::cerr << "unexpected_invalid_write_source_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto invalid_write_step_manifest_path = root / "invalid-write-step.json";
    {
        std::ofstream manifest(invalid_write_step_manifest_path);
        manifest << R"({
  "id": "alttp",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "alttp-invalid-write-step",
    "driver_instance_id": "alttp-invalid-write-step",
    "core_profile": "snes_v1",
    "actions": [
      {
        "item_id": 10,
        "item_name": "Hookshot",
        "writes": [
          {
            "address": 62672,
            "size": 1,
            "source": "constant",
            "value_u64": 10
          }
        ]
      }
    ]
  }
})";
    }

    error.clear();
    const auto invalid_write_step_manifest = load_bridge_manifest(invalid_write_step_manifest_path, &error);
    if (invalid_write_step_manifest.has_value()) {
        std::cerr << "invalid_write_step_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "manifest_action_invalid_write_step") {
        std::cerr << "unexpected_invalid_write_step_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto invalid_type_manifest_path = root / "invalid-linkedworld-type.json";
    {
        std::ofstream manifest(invalid_type_manifest_path);
        manifest << R"({
  "id": "alttp",
  "type": "bridge_manifest",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "alttp-invalid-type",
    "driver_instance_id": "alttp-invalid-type",
    "core_profile": "snes_v1",
    "checks": [
      {
        "domain_id": "system_ram",
        "address": 61476,
        "size": 1,
        "compare": "mask_any",
        "operand_u64": 16,
        "event_type": "location_checked",
        "location_id": 60025,
        "location_name": "Sanctuary"
      }
    ]
  }
})";
    }

    error.clear();
    const auto invalid_type_manifest = load_bridge_manifest(invalid_type_manifest_path, &error);
    if (invalid_type_manifest.has_value()) {
        std::cerr << "invalid_type_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "linkedworld_invalid_type") {
        std::cerr << "unexpected_invalid_type_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto invalid_compare_manifest_path = root / "invalid-compare.json";
    {
        std::ofstream manifest(invalid_compare_manifest_path);
        manifest << R"({
  "id": "alttp",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "alttp-invalid-compare",
    "driver_instance_id": "alttp-invalid-compare",
    "core_profile": "snes_v1",
    "checks": [
      {
        "domain_id": "system_ram",
        "address": 61476,
        "size": 1,
        "compare": "alttp_magic",
        "operand_u64": 16,
        "event_type": "location_checked",
        "location_id": 60025,
        "location_name": "Sanctuary"
      }
    ]
  }
})";
    }

    error.clear();
    const auto invalid_compare_manifest = load_bridge_manifest(invalid_compare_manifest_path, &error);
    if (invalid_compare_manifest.has_value()) {
        std::cerr << "invalid_compare_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "manifest_check_invalid_compare") {
        std::cerr << "unexpected_invalid_compare_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto invalid_event_type_manifest_path = root / "invalid-event-type.json";
    {
        std::ofstream manifest(invalid_event_type_manifest_path);
        manifest << R"({
  "id": "alttp",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "alttp-invalid-event-type",
    "driver_instance_id": "alttp-invalid-event-type",
    "core_profile": "snes_v1",
    "checks": [
      {
        "domain_id": "system_ram",
        "address": 61476,
        "size": 1,
        "compare": "mask_any",
        "operand_u64": 16,
        "event_type": "alttp_room_clear",
        "location_id": 60025,
        "location_name": "Sanctuary"
      }
    ]
  }
})";
    }

    error.clear();
    const auto invalid_event_type_manifest = load_bridge_manifest(invalid_event_type_manifest_path, &error);
    if (invalid_event_type_manifest.has_value()) {
        std::cerr << "invalid_event_type_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "manifest_check_invalid_event_type") {
        std::cerr << "unexpected_invalid_event_type_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto undeclared_domain_manifest_path = root / "undeclared-domain.json";
    {
        std::ofstream manifest(undeclared_domain_manifest_path);
        manifest << R"({
  "id": "alttp",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "alttp-undeclared-domain",
    "driver_instance_id": "alttp-undeclared-domain",
    "core_profile": {
      "name": "snes_v1",
      "domains": [
        { "id": "system_ram" }
      ]
    },
    "checks": [
      {
        "domain_id": "cart_rom",
        "address": 61476,
        "size": 1,
        "compare": "mask_any",
        "operand_u64": 16,
        "event_type": "location_checked",
        "location_id": 60025,
        "location_name": "Sanctuary"
      }
    ]
  }
})";
    }

    error.clear();
    const auto undeclared_domain_manifest = load_bridge_manifest(undeclared_domain_manifest_path, &error);
    if (undeclared_domain_manifest.has_value()) {
        std::cerr << "undeclared_domain_manifest_should_fail\n";
        return EXIT_FAILURE;
    }
    if (error != "manifest_check_unknown_domain") {
        std::cerr << "unexpected_undeclared_domain_error:" << error << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_legacy_manifest_smoke_ok\n";
    return EXIT_SUCCESS;
}
