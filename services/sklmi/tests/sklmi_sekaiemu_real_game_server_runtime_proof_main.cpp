#include "sekailink_sklmi/api.hpp"

#include <array>
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
        if (pid < 0) return false;
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
        if (pid < 0) return false;
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

    bool start_game_server(const std::filesystem::path& binary, std::uint16_t tcp_port, std::uint16_t http_port) {
        pid = fork();
        if (pid < 0) return false;
        if (pid == 0) {
            setenv("SEKAILINK_GAME_SERVER_ADMIN_TOKEN", "admin-secret", 1);
            setenv("SEKAILINK_GAME_SERVER_CORE_TOKEN", "core-secret", 1);
            setenv("SEKAILINK_GAME_SERVER_RUNTIME_TOKEN", "runtime-secret", 1);
            execl(binary.c_str(),
                  binary.c_str(),
                  "--tcp-port",
                  std::to_string(tcp_port).c_str(),
                  "--http-port",
                  std::to_string(http_port).c_str(),
                  static_cast<char*>(nullptr));
            _exit(127);
        }
        return true;
    }

    void stop() {
        if (pid <= 0) return;
        kill(pid, SIGTERM);
        int status = 0;
        for (int attempt = 0; attempt < 20; ++attempt) {
            const auto result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                pid = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        pid = -1;
    }

    [[nodiscard]] bool exited() const {
        if (pid <= 0) return true;
        int status = 0;
        const auto result = waitpid(pid, &status, WNOHANG);
        return result == pid;
    }
};

std::string read_line(int fd) {
    std::string line;
    char ch = '\0';
    while (true) {
        const auto received = recv(fd, &ch, 1, 0);
        if (received <= 0) return {};
        if (ch == '\n') break;
        if (ch != '\r') line.push_back(ch);
    }
    return line;
}

bool write_all(int fd, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto written = send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (written <= 0) return false;
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

std::optional<std::string> extract_json_string(const std::string& text, const std::string& key) {
    const auto needle = "\"" + key + "\":\"";
    const auto begin = text.find(needle);
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + needle.size();
    const auto end = text.find('"', start);
    if (end == std::string::npos) return std::nullopt;
    return text.substr(start, end - start);
}

std::optional<std::uint64_t> extract_json_u64(const std::string& text, const std::string& key) {
    const auto needle = "\"" + key + "\":";
    const auto begin = text.find(needle);
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + needle.size();
    const auto end = text.find_first_of(",}", start);
    if (end == std::string::npos) return std::nullopt;
    return static_cast<std::uint64_t>(std::stoull(text.substr(start, end - start)));
}

bool json_ok(const std::string& text) {
    return text.find("\"ok\":true") != std::string::npos;
}

std::string tcp_json_line_request(const std::string& host, std::uint16_t port, const std::string& payload) {
    const int fd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0) {
        return {};
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return {};
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return {};
    }
    if (!write_all(fd, payload + "\n")) {
        close(fd);
        return {};
    }
    const auto response = read_line(fd);
    close(fd);
    return response;
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::uint16_t pick_loopback_port() {
    const int fd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0) return 0;
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return 0;
    }
    socklen_t len = sizeof(addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        close(fd);
        return 0;
    }
    const auto port = ntohs(addr.sin_port);
    close(fd);
    return port;
}

#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    std::cerr << "linux_only_runtime_proof\n";
    return EXIT_FAILURE;
#else
    if (argc < 6) {
        std::cerr << "usage: sklmi_sekaiemu_real_game_server_runtime_proof <sekaiemu_binary> <sklmi_runtime_binary> <game_server_binary> <core_libretro> <rom> [root_dir]\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path sekaiemu_binary = argv[1];
    const std::filesystem::path sklmi_runtime_binary = argv[2];
    const std::filesystem::path game_server_binary = argv[3];
    const std::filesystem::path core_path = argv[4];
    const std::filesystem::path rom_path = argv[5];
    const std::filesystem::path root_dir =
        argc >= 7 ? std::filesystem::path(argv[6]) : (std::filesystem::temp_directory_path() / ("sklmi-real-gs-" + std::to_string(::getpid())));

    std::filesystem::remove_all(root_dir);
    std::filesystem::create_directories(root_dir);

    const auto sekaiemu_save_dir = root_dir / "sekaiemu";
    const auto sklmi_state_root = root_dir / "sklmi";
    const auto socket_path = root_dir / "sekaiemu-memory.sock";
    const auto manifest_path = root_dir / "linkedworld.json";
    const auto room_state_path = root_dir / "room-sync.state";
    const auto trace_log_path = root_dir / "trace.jsonl";

    if (!std::filesystem::exists(sekaiemu_binary) ||
        !std::filesystem::exists(sklmi_runtime_binary) ||
        !std::filesystem::exists(game_server_binary) ||
        !std::filesystem::exists(core_path) ||
        !std::filesystem::exists(rom_path)) {
        std::cerr << "binary_or_rom_missing\n";
        return EXIT_FAILURE;
    }

    std::filesystem::create_directories(sekaiemu_save_dir);
    std::filesystem::create_directories(sklmi_state_root);

    const auto tcp_port = pick_loopback_port();
    const auto http_port = pick_loopback_port();
    if (tcp_port == 0 || http_port == 0) {
        std::cerr << "port_pick_failed\n";
        return EXIT_FAILURE;
    }

    ChildProcess game_server_child;
    if (!game_server_child.start_game_server(game_server_binary, tcp_port, http_port)) {
        std::cerr << "game_server_spawn_failed\n";
        return EXIT_FAILURE;
    }

    bool server_ready = false;
    for (int attempt = 0; attempt < 80; ++attempt) {
        if (game_server_child.exited()) {
            std::cerr << "game_server_exited_early\n";
            return EXIT_FAILURE;
        }
        const auto response = tcp_json_line_request(
            "127.0.0.1",
            tcp_port,
            R"({"channel":"admin","auth_token":"admin-secret","command":{"cmd":"list_sessions"}})");
        if (json_ok(response)) {
            server_ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!server_ready) {
        std::cerr << "game_server_not_ready\n";
        return EXIT_FAILURE;
    }

    const auto create_response = tcp_json_line_request(
        "127.0.0.1",
        tcp_port,
        R"({"channel":"admin","auth_token":"admin-secret","command":{"cmd":"create_session_from_ap_import","session_name":"earthbound-session","world_id":"world-eb-proof","world_version":"1.0","seed_id":"seed-eb-proof","seed_hash":"ABC123","linkedworld_id":"earthbound","archipelago":{"slot_info":{"1":{"name":"Jade","game":"EarthBound","type":"player"}},"locations":{"1":{"44001":{"receiver_slot":1,"item_id":55001,"item_name":"Runtime Proof Item","location_name":"EarthBound Runtime Proof","flags":0}}},"game_options":{"release_mode":"enabled"}}}})");
    if (!json_ok(create_response)) {
        std::cerr << "session_create_failed:" << create_response << "\n";
        return EXIT_FAILURE;
    }

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
    const std::array<std::byte, 1> trigger{std::byte{0x5A}};
    if (!provider.write("system_ram", watch_address, trigger.data(), trigger.size())) {
        std::cerr << "seed_memory_failed\n";
        return EXIT_FAILURE;
    }

    const std::string driver_instance_id = "earthbound-runtime-proof";
    {
        std::ofstream manifest(manifest_path, std::ios::trunc);
        manifest << "{\n"
                 << "  \"id\": \"earthbound\",\n"
                 << "  \"type\": \"linkedworld\",\n"
                 << "  \"version\": \"0.1.0-dev\",\n"
                 << "  \"sklmi\": {\n"
                 << "    \"contract_version\": \"1.0\",\n"
                 << "    \"bridge_id\": \"earthbound-real-game-server-runtime-proof\",\n"
                 << "    \"driver_instance_id\": \"" << driver_instance_id << "\",\n"
                 << "    \"core_profile\": \"snes_v1\",\n"
                 << "    \"module\": \"sekaiemu\",\n"
                 << "    \"state_file\": \"earthbound-real-game-server-runtime-proof.state\",\n"
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
                 << "        \"domain_id\": \"system_ram\",\n"
                 << "        \"address\": " << static_cast<std::uint64_t>(wram->size_bytes - 1) << ",\n"
                 << "        \"value_u64\": 33,\n"
                 << "        \"size\": 1,\n"
                 << "        \"item_id\": 55001,\n"
                 << "        \"item_name\": \"Runtime Proof Item\",\n"
                 << "        \"room_controlled\": true\n"
                 << "      }\n"
                 << "    ]\n"
                 << "  }\n"
                 << "}\n";
    }

    provider.disconnect();

    ChildProcess sklmi_child;
    if (!sklmi_child.start_sklmi_runtime(
            sklmi_runtime_binary, socket_path, manifest_path, room_state_path, sklmi_state_root, trace_log_path, driver_instance_id, tcp_port)) {
        std::cerr << "sklmi_spawn_failed\n";
        return EXIT_FAILURE;
    }

    bool saw_item = false;
    for (int attempt = 0; attempt < 300; ++attempt) {
        if (game_server_child.exited()) {
            std::cerr << "game_server_exited_during_runtime\n";
            return EXIT_FAILURE;
        }
        if (sekaiemu_child.exited()) {
            std::cerr << "sekaiemu_exited_during_runtime\n";
            return EXIT_FAILURE;
        }
        if (sklmi_child.exited()) {
            break;
        }
        if (std::filesystem::exists(trace_log_path)) {
            const auto trace_log = slurp(trace_log_path);
            if (trace_log.find("\"event_type\":\"item_received\"") != std::string::npos) {
                saw_item = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    sklmi_child.stop();
    sekaiemu_child.stop();

    const auto summary_response = tcp_json_line_request(
        "127.0.0.1",
        tcp_port,
        R"({"channel":"admin","auth_token":"admin-secret","command":{"cmd":"session_summary","session_name":"earthbound-session"}})");

    game_server_child.stop();

    const auto trace_log = std::filesystem::exists(trace_log_path) ? slurp(trace_log_path) : std::string{};
    const auto checked_count = extract_json_u64(summary_response, "checked_count").value_or(0);
    const auto delivered_count = extract_json_u64(summary_response, "delivered_count").value_or(0);
    const auto acknowledged_count = extract_json_u64(summary_response, "acknowledged_count").value_or(0);

    if (!json_ok(summary_response) ||
        !saw_item ||
        trace_log.find("\"event_type\":\"location_checked\"") == std::string::npos ||
        trace_log.find("\"event_type\":\"item_received\"") == std::string::npos ||
        checked_count < 1 ||
        delivered_count < 1 ||
        acknowledged_count < 1) {
        std::cerr << "real_game_server_flow_incomplete\n";
        std::cerr << "summary_response=" << summary_response << "\n";
        std::cerr << trace_log << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_sekaiemu_real_game_server_runtime_proof_ok\n";
    return EXIT_SUCCESS;
#endif
}
