#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace sekailink::sklmi;

namespace {

std::filesystem::path WriteManifest(const std::filesystem::path& dir,
                                    const std::string& name,
                                    const std::string& wrapper,
                                    const std::string& module = "") {
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::ofstream out(path);
    out << R"({
  "linkedworld_id": "alttp",
  "linkedworld_version": "0.3.1",
  "contract_version": "1.0",
  "bridge_id": "alttp-sni-wrapper",
  "driver_instance_id": "alttp-sni-wrapper",
  "module": "tests.wrapper",
  "core_profile": {
    "name": "snes.sni",
    "domains": [{"id": "system_bus"}]
  },
  "archipelago_client_wrapper": {
    "enabled": true,
    "game_key": "alttp",
    "game": "A Link to the Past",
    "platform": "SNES",
    "world": "worlds.alttp",
    "wrapper": ")" << wrapper << R"(",
    "module": ")" << module << R"(",
    "client_file": "worlds/alttp/__init__.py",
    "status": "enabled",
    "requires": ["sni", "portable_python"]
  },
  "checks": [{
    "domain_id": "system_bus",
    "address": 4660,
    "size": 1,
    "compare": "equals",
    "operand_u64": 1,
    "location_id": 123,
    "location_name": "Smoke Check"
  }]
})";
    return path;
}

}  // namespace

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "sekailink-sklmi-wrapper-manifest-smoke";
    std::filesystem::remove_all(dir);

    std::string error;
    auto manifest = load_bridge_manifest(WriteManifest(dir, "valid.json", "sni"), &error);
    if (!manifest.has_value()) {
        std::cerr << "valid_manifest_load_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    const auto& wrapper = manifest->archipelago_client_wrapper;
    if (!wrapper.enabled || wrapper.wrapper != "sni" || wrapper.game_key != "alttp" ||
        wrapper.world != "worlds.alttp" || wrapper.platform != "SNES" ||
        wrapper.dependency_hints.size() != 2 || wrapper.dependency_hints[0] != "sni" ||
        wrapper.dependency_hints[1] != "portable_python") {
        std::cerr << "wrapper_metadata_mismatch\n";
        return EXIT_FAILURE;
    }

    error.clear();
    const auto invalid = load_bridge_manifest(WriteManifest(dir, "invalid.json", "unsupported"), &error);
    if (invalid.has_value() || error != "manifest_client_wrapper_invalid_wrapper") {
        std::cerr << "invalid_wrapper_not_rejected:" << error << "\n";
        return EXIT_FAILURE;
    }

    error.clear();
    const auto module_manifest = load_bridge_manifest(WriteManifest(dir, "module.json", "module", "worlds.dk64.Client"), &error);
    if (!module_manifest.has_value() || module_manifest->archipelago_client_wrapper.module != "worlds.dk64.Client") {
        std::cerr << "module_wrapper_load_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
