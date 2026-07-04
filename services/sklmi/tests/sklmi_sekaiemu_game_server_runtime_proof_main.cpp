#include "sekailink_sklmi/api.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace sekailink::sklmi;

namespace {

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

    bool start_sekaiemu(const std::filesystem::path& binary,
                        const std::filesystem::path& core,
                        const std::filesystem::path& rom,
                        const std::filesystem::path& save_dir,
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
                  save_dir.c_str(),
                  "--memory-socket",
                  socket_path.c_str(),
                  static_cast<char*>(nullptr));
            _exit(127);
        }
        return true;
    }

    bool start_sklmi_runtime(const std::filesystem::path& binary,
                             const std::filesystem::path& socket_path,
                             const std::filesystem::path& manifest_path,
                             const std::filesystem::path& room_state_path,
                             const std::filesystem::path& runtime_state_root,
                             const std::filesystem::path& trace_log_path,
                             const std::string& driver_instance_id,
                             std::uint16_t room_port) {
        pid = fork();
        if (pid < 0) {
            return false;
        }
        if (pid == 0) {
            execl(binary.c_str(),
                  binary.c_str(),
                  "--memory-socket",
                  socket_path.c_str(),
                  "--bridge-manifest",
                  manifest_path.c_str(),
                  "--room-state",
                  room_state_path.c_str(),
                  "--runtime-state",
                  runtime_state_root.c_str(),
                  "--trace-log",
                  trace_log_path.c_str(),
                  "--driver-instance-id",
                  driver_instance_id.c_str(),
                  "--mode",
                  "sekailink_game_server",
                  "--room-host",
                  "127.0.0.1",
                  "--room-port",
                  std::to_string(room_port).c_str(),
                  "--room-session-name",
                  "earthbound-session",
                  "--room-slot-id",
                  "1",
                  "--room-control-channel",
                  "core",
                  "--room-control-auth-token",
                  "core-secret",
                  "--room-runtime-auth-token",
                  "runtime-secret",
                  "--tick-ms",
                  "10",
                  "--max-ticks",
                  "400",
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

void close_socket_fd(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

std::string read_line(int fd) {
    std::string line;
    char ch = '\0';
    while (true) {
        const auto received = recv(fd, &ch, 1, 0);
        if (received <= 0) {
            return {};
        }
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            line.push_back(ch);
        }
    }
    return line;
}

bool write_all(int fd, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto written = send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (written <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

std::filesystem::path make_temp_dir() {
    const auto dir = std::filesystem::temp_directory_path() / ("sklmi-sgs-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    std::cerr << "linux_only_runtime_proof\n";
    return EXIT_FAILURE;
#else
    if (argc < 5) {
        std::cerr << "usage: sklmi_sekaiemu_game_server_runtime_proof <sekaiemu_binary> <sklmi_runtime_binary> <core_libretro> <rom> [save_dir]\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path sekaiemu_binary = argv[1];
    const std::filesystem::path sklmi_runtime_binary = argv[2];
    const std::filesystem::path core_path = argv[3];
    const std::filesystem::path rom_path = argv[4];
    const std::filesystem::path root_dir = argc >= 6 ? std::filesystem::path(argv[5]) : make_temp_dir();
    const auto sekaiemu_save_dir = root_dir / "sekaiemu";
    const auto sklmi_state_root = root_dir / "sklmi";
    const auto socket_path = root_dir / "sekaiemu-memory.sock";
    const auto manifest_path = root_dir / "linkedworld.json";
    const auto room_state_path = root_dir / "room-sync.state";
    const auto trace_log_path = root_dir / "trace.jsonl";
    const std::string driver_instance_id = "earthbound-runtime-proof";

    if (!std::filesystem::exists(sekaiemu_binary)) {
        std::cerr << "sekaiemu_binary_missing\n";
        return EXIT_FAILURE;
    }
    if (!std::filesystem::exists(sklmi_runtime_binary)) {
        std::cerr << "sklmi_runtime_binary_missing\n";
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

    std::filesystem::create_directories(sekaiemu_save_dir);
    std::filesystem::create_directories(sklmi_state_root);

    ChildProcess sekaiemu_child;
    if (!sekaiemu_child.start_sekaiemu(sekaiemu_binary, core_path, rom_path, sekaiemu_save_dir, socket_path)) {
        std::cerr << "sekaiemu_spawn_failed\n";
        return EXIT_FAILURE;
    }

    UnixSocketMemoryProvider provider(socket_path);
    std::string connect_error;
    bool connected = false;
    for (int attempt = 0; attempt < 120; ++attempt) {
        if (sekaiemu_child.exited()) {
            std::cerr << "sekaiemu_exited_early\n";
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

    const auto wram = find_domain(provider, "system_ram");
    if (!wram.has_value() || !wram->writable || wram->size_bytes < 2) {
        std::cerr << "wram_domain_invalid\n";
        return EXIT_FAILURE;
    }

    const auto watch_address = static_cast<std::uint64_t>(wram->size_bytes - 2);
    std::string inject_domain = "system_ram";
    std::uint64_t inject_address = static_cast<std::uint64_t>(wram->size_bytes - 1);
    if (const auto save_ram = find_domain(provider, "save_ram");
        save_ram.has_value() && save_ram->writable && save_ram->size_bytes > 0) {
        inject_domain = "save_ram";
        inject_address = static_cast<std::uint64_t>(save_ram->size_bytes - 1);
    }
    const std::array<std::byte, 1> zero{std::byte{0x00}};
    const std::array<std::byte, 1> trigger{std::byte{0x5A}};
    const auto injected_value = static_cast<std::uint64_t>(0x21);

    if (!provider.write("system_ram", watch_address, trigger.data(), trigger.size()) ||
        !provider.write(inject_domain, inject_address, zero.data(), zero.size())) {
        std::cerr << "seed_memory_failed\n";
        return EXIT_FAILURE;
    }

    {
        std::ofstream manifest(manifest_path, std::ios::trunc);
        manifest << "{\n"
                 << "  \"id\": \"earthbound\",\n"
                 << "  \"type\": \"linkedworld\",\n"
                 << "  \"version\": \"0.1.0-dev\",\n"
                 << "  \"sklmi\": {\n"
                 << "    \"contract_version\": \"1.0\",\n"
                 << "    \"bridge_id\": \"earthbound-game-server-runtime-proof\",\n"
                 << "    \"driver_instance_id\": \"" << driver_instance_id << "\",\n"
                 << "    \"core_profile\": \"snes_v1\",\n"
                 << "    \"module\": \"sekaiemu\",\n"
                 << "    \"state_file\": \"earthbound-game-server-runtime-proof.state\",\n"
                 << "    \"poll_interval_ms\": 10,\n"
                 << "    \"checks\": [\n"
                 << "      {\n"
                 << "        \"domain_id\": \"system_ram\",\n"
                 << "        \"address\": " << watch_address << ",\n"
                 << "        \"size\": 1,\n"
                 << "        \"compare\": \"equals\",\n"
                 << "        \"operand_u64\": 90,\n"
                 << "        \"event_type\": \"location_checked\",\n"
                 << "        \"location_id\": 44001,\n"
                 << "        \"location_name\": \"EarthBound Runtime Proof\"\n"
                 << "      }\n"
                 << "    ],\n"
                 << "    \"actions\": [\n"
                 << "      {\n"
                 << "        \"domain_id\": \"" << inject_domain << "\",\n"
                 << "        \"address\": " << inject_address << ",\n"
                 << "        \"value_u64\": " << injected_value << ",\n"
                 << "        \"size\": 1,\n"
                 << "        \"item_id\": 55001,\n"
                 << "        \"item_name\": \"Runtime Proof Item\",\n"
                 << "        \"room_controlled\": true\n"
                 << "      }\n"
                 << "    ]\n"
                 << "  }\n"
                 << "}\n";
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "server_socket_failed\n";
        return EXIT_FAILURE;
    }
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(server_fd, 8) != 0) {
        close_socket_fd(server_fd);
        std::cerr << "server_bind_failed\n";
        return EXIT_FAILURE;
    }
    socklen_t addr_len = sizeof(addr);
    if (getsockname(server_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
        close_socket_fd(server_fd);
        std::cerr << "server_getsockname_failed\n";
        return EXIT_FAILURE;
    }
    const auto room_port = ntohs(addr.sin_port);

    std::atomic<bool> stop_server{false};
    std::atomic<bool> event_seen{false};
    std::atomic<bool> pending_served{false};
    std::atomic<bool> ack_seen{false};
    std::string issue_ticket_request;
    std::string runtime_event_request;
    std::string pending_items_request;
    std::string acknowledge_request;

    std::thread server([&]() {
        while (!stop_server.load()) {
            const int client = accept(server_fd, nullptr, nullptr);
            if (client < 0) {
                if (stop_server.load()) {
                    break;
                }
                continue;
            }
            const auto line = read_line(client);
            if (line.find("\"cmd\":\"issue_ticket\"") != std::string::npos) {
                issue_ticket_request = line;
                write_all(client, "{\"ok\":true,\"ticket\":{\"session_token\":\"runtime-ticket-1\"}}\n");
            } else if (line.find("\"cmd\":\"runtime_event\"") != std::string::npos) {
                runtime_event_request = line;
                event_seen.store(true);
                write_all(client, "{\"ok\":true,\"accepted\":true,\"duplicate\":false,\"reason\":\"accepted\",\"created_delivery_ids\":[7]}\n");
            } else if (line.find("\"cmd\":\"pending_items\"") != std::string::npos) {
                pending_items_request = line;
                if (event_seen.load() && !ack_seen.load()) {
                    pending_served.store(true);
                    write_all(client, "{\"ok\":true,\"pending_items\":[{\"delivery_id\":7,\"item_id\":55001,\"item_name\":\"Runtime Proof Item\"}]}\n");
                } else {
                    write_all(client, "{\"ok\":true,\"pending_items\":[]}\n");
                }
            } else if (line.find("\"cmd\":\"acknowledge_delivery\"") != std::string::npos) {
                acknowledge_request = line;
                ack_seen.store(true);
                write_all(client, "{\"ok\":true}\n");
            } else {
                write_all(client, "{\"ok\":false,\"error\":\"unsupported_command\"}\n");
            }
            close_socket_fd(client);
        }
        close_socket_fd(server_fd);
    });

    ChildProcess sklmi_child;
    provider.disconnect();
    if (!sklmi_child.start_sklmi_runtime(sklmi_runtime_binary,
                                         socket_path,
                                         manifest_path,
                                         room_state_path,
                                         sklmi_state_root,
                                         trace_log_path,
                                         driver_instance_id,
                                         room_port)) {
        stop_server.store(true);
        shutdown(server_fd, SHUT_RDWR);
        if (server.joinable()) {
            server.join();
        }
        std::cerr << "sklmi_spawn_failed\n";
        return EXIT_FAILURE;
    }

    bool injected = false;
    for (int attempt = 0; attempt < 300; ++attempt) {
        if (sekaiemu_child.exited()) {
            std::cerr << "sekaiemu_exited_during_runtime\n";
            return EXIT_FAILURE;
        }
        if (sklmi_child.exited()) {
            break;
        }
        if (ack_seen.load()) {
            injected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    sklmi_child.stop();
    stop_server.store(true);
    shutdown(server_fd, SHUT_RDWR);
    if (server.joinable()) {
        server.join();
    }

    bool injected_value_observed = false;
    if (inject_domain == "save_ram") {
        const auto battery_path = sekaiemu_save_dir / "battery" / (rom_path.stem().string() + ".sav");
        if (std::filesystem::exists(battery_path)) {
            std::ifstream battery(battery_path, std::ios::binary);
            std::vector<char> bytes((std::istreambuf_iterator<char>(battery)), std::istreambuf_iterator<char>());
            if (!bytes.empty()) {
                injected_value_observed = static_cast<unsigned char>(bytes.back()) == injected_value;
            }
        }
    } else {
        std::string reconnect_error;
        if (!provider.connect(&reconnect_error)) {
            std::cerr << "provider_reconnect_failed:" << reconnect_error << "\n";
            return EXIT_FAILURE;
        }
        std::byte injected_byte{};
        injected_value_observed =
            provider.read(inject_domain, inject_address, &injected_byte, 1) &&
            injected_byte == static_cast<std::byte>(injected_value);
    }

    if (!injected) {
        std::cerr << "runtime_injection_not_observed\n";
        if (std::filesystem::exists(trace_log_path)) {
            std::cerr << slurp(trace_log_path) << "\n";
        }
        return EXIT_FAILURE;
    }

    const auto trace_log = std::filesystem::exists(trace_log_path) ? slurp(trace_log_path) : std::string{};
    if (!event_seen.load() ||
        !pending_served.load() ||
        !ack_seen.load() ||
        issue_ticket_request.find("\"cmd\":\"issue_ticket\"") == std::string::npos ||
        issue_ticket_request.find("\"driver_instance_id\":\"" + driver_instance_id + "\"") == std::string::npos ||
        runtime_event_request.find("\"canonical_id\":44001") == std::string::npos ||
        trace_log.find("\"event_type\":\"location_checked\"") == std::string::npos ||
        trace_log.find("\"event_type\":\"item_received\"") == std::string::npos) {
        std::cerr << "runtime_game_server_flow_incomplete\n";
        std::cerr << "issue_ticket_request=" << issue_ticket_request << "\n";
        std::cerr << "runtime_event_request=" << runtime_event_request << "\n";
        std::cerr << "pending_items_request=" << pending_items_request << "\n";
        std::cerr << "acknowledge_request=" << acknowledge_request << "\n";
        std::cerr << "injected_value_observed=" << (injected_value_observed ? "true" : "false") << "\n";
        std::cerr << trace_log << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_sekaiemu_game_server_runtime_proof_ok\n";
    return EXIT_SUCCESS;
#endif
}
