#include "sekailink_sklmi/api.hpp"

#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace sekailink::sklmi;

namespace {

std::optional<MemoryDomainDescriptor> find_domain(const RuntimeSocketMemoryProvider& provider, const std::string& id) {
    for (const auto& domain : provider.domains()) {
        if (domain.id == id) {
            return domain;
        }
    }
    return std::nullopt;
}

std::optional<MemoryDomainDescriptor> find_domain(const UnixSocketMemoryProvider& provider, const std::string& id) {
    for (const auto& domain : provider.domains()) {
        if (domain.id == id) {
            return domain;
        }
    }
    return std::nullopt;
}

#ifndef _WIN32
struct ChildProcess {
    pid_t pid = -1;

    ~ChildProcess() {
        stop();
    }

    bool start(const std::filesystem::path& binary,
               const std::filesystem::path& core,
               const std::filesystem::path& rom,
               const std::filesystem::path& save,
               const std::filesystem::path& socket_path) {
        pid = fork();
        if (pid < 0) {
            return false;
        }
        if (pid == 0) {
            setenv("SDL_VIDEODRIVER", "dummy", 1);
            setenv("SDL_AUDIODRIVER", "dummy", 1);
            execl(binary.c_str(),
                  binary.c_str(),
                  "--core",
                  core.c_str(),
                  "--game",
                  rom.c_str(),
                  "--save-dir",
                  save.c_str(),
                  "--memory-socket",
                  socket_path.c_str(),
                  static_cast<char*>(nullptr));
            _exit(127);
        }
        return true;
    }

    void stop() {
        if (pid <= 0) {
            return;
        }
        kill(pid, SIGTERM);
        int status = 0;
        waitpid(pid, &status, 0);
        pid = -1;
    }

    [[nodiscard]] bool exited() const {
        if (pid <= 0) {
            return true;
        }
        int status = 0;
        const auto result = waitpid(pid, &status, WNOHANG);
        return result == pid;
    }
};
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    std::cerr << "linux_only_runtime_proof\n";
    return EXIT_FAILURE;
#else
    if (argc < 4) {
        std::cerr << "usage: sklmi_sekaiemu_runtime_proof <sekaiemu_binary> <core_libretro> <rom> [save_dir]\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path runtime_binary = argv[1];
    const std::filesystem::path core_path = argv[2];
    const std::filesystem::path rom_path = argv[3];
    const std::filesystem::path save_dir =
        argc >= 5 ? std::filesystem::path(argv[4]) : std::filesystem::temp_directory_path() / "sklmi-sekaiemu-proof";
    const auto socket_path = save_dir / "runtime" / "sekaiemu-memory.sock";

    if (!std::filesystem::exists(runtime_binary)) {
        std::cerr << "runtime_binary_missing\n";
        return EXIT_FAILURE;
    }
    if (!std::filesystem::exists(core_path)) {
        std::cerr << "core_missing\n";
        return EXIT_FAILURE;
    }
    if (!std::filesystem::exists(rom_path)) {
        std::cerr << "rom_missing\n";
        return EXIT_FAILURE;
    }

    ChildProcess child;
    if (!child.start(runtime_binary, core_path, rom_path, save_dir, socket_path)) {
        std::cerr << "runtime_spawn_failed\n";
        return EXIT_FAILURE;
    }

    UnixSocketMemoryProvider provider(socket_path);
    std::string connect_error;
    bool connected = false;
    for (int attempt = 0; attempt < 80; ++attempt) {
        if (child.exited()) {
            std::cerr << "runtime_exited_early\n";
            return EXIT_FAILURE;
        }
        if (provider.connect(&connect_error)) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!connected) {
        std::cerr << "provider_connect_failed:" << connect_error << "\n";
        return EXIT_FAILURE;
    }

    if (!provider.protocol_version().has_value()) {
        std::cerr << "runtime_metadata_invalid\n";
        return EXIT_FAILURE;
    }

    const auto wram = find_domain(provider, "system_ram");
    if (!wram.has_value() || !wram->writable || wram->size_bytes < 2) {
        std::cerr << "wram_domain_invalid\n";
        return EXIT_FAILURE;
    }

    const auto watch_address = static_cast<std::uint64_t>(wram->size_bytes - 2);
    const auto inject_address = static_cast<std::uint64_t>(wram->size_bytes - 1);
    const std::array<std::byte, 1> zero{std::byte{0x00}};
    const std::array<std::byte, 1> trigger{std::byte{0x5A}};

    if (!provider.write("system_ram", watch_address, zero.data(), zero.size()) ||
        !provider.write("system_ram", inject_address, zero.data(), zero.size())) {
        std::cerr << "seed_memory_failed\n";
        return EXIT_FAILURE;
    }

    BridgeManifest manifest{
        .linkedworld_id = "earthbound",
        .bridge_id = "sekaiemu-snes-runtime-proof",
        .module = "manifest",
        .state_file = "",
        .poll_interval_ms = 1,
        .checks = {WatchRule{
            .domain_id = "system_ram",
            .address = watch_address,
            .size = 1,
            .compare = CompareOp::equals,
            .operand_u64 = 0x5A,
            .event_type = EventType::location_checked,
            .event_key = "earthbound.proof.check",
            .mapped_value = "loc:earthbound-proof"
        }},
        .injections = {InjectRule{
            .domain_id = "system_ram",
            .address = inject_address,
            .value_u64 = 0x01,
            .size = 1,
            .event_key = "earthbound.proof.item",
            .mapped_value = "item:earthbound-proof"
        }}
    };

    VectorEventSink sink;
    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(manifest));
    if (session.start().state != RuntimeConnectionState::connected) {
        std::cerr << "session_start_failed\n";
        return EXIT_FAILURE;
    }

    if (!provider.write("system_ram", watch_address, trigger.data(), trigger.size())) {
        std::cerr << "trigger_write_failed\n";
        return EXIT_FAILURE;
    }

    if (session.tick({.tick_index = 1, .monotonic_ms = 10}).state != RuntimeConnectionState::connected) {
        std::cerr << "session_tick_failed\n";
        return EXIT_FAILURE;
    }

    std::byte injected{};
    if (!provider.read("system_ram", inject_address, &injected, 1)) {
        std::cerr << "injected_read_failed\n";
        return EXIT_FAILURE;
    }

    bool saw_check = false;
    bool saw_item = false;
    for (const auto& event : sink.events()) {
        if (event.type == EventType::location_checked && event.value == "loc:earthbound-proof") {
            saw_check = true;
        }
        if (event.type == EventType::item_received && event.value == "item:earthbound-proof") {
            saw_item = true;
        }
    }

    if (!saw_check || !saw_item || injected != std::byte{0x01}) {
        std::cerr << "bridge_proof_failed\n";
        return EXIT_FAILURE;
    }

    provider.disconnect();
    child.stop();
    std::cout << "sklmi_sekaiemu_runtime_proof_ok\n";
    return EXIT_SUCCESS;
#endif
}
