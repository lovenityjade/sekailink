#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#ifndef _WIN32

void close_socket(int fd) {
    if (fd >= 0) close(fd);
}

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

std::optional<std::uint64_t> extract_uint(const std::string& text, const std::string& key) {
    const auto begin = text.find("\"" + key + "\":");
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + key.size() + 3;
    auto end = text.find_first_of(",}]", start);
    if (end == std::string::npos) end = text.size();
    return static_cast<std::uint64_t>(std::stoull(text.substr(start, end - start)));
}

std::optional<std::string> extract_string(const std::string& text, const std::string& key) {
    const auto begin = text.find("\"" + key + "\":\"");
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + key.size() + 4;
    const auto end = text.find('"', start);
    if (end == std::string::npos) return std::nullopt;
    return text.substr(start, end - start);
}

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode_bytes(const std::vector<std::byte>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const auto b0 = static_cast<unsigned>(std::to_integer<unsigned char>(data[i]));
        const auto b1 = (i + 1 < data.size()) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 1])) : 0U;
        const auto b2 = (i + 2 < data.size()) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 2])) : 0U;
        const auto combined = (b0 << 16U) | (b1 << 8U) | b2;
        out.push_back(kAlphabet[(combined >> 18U) & 0x3FU]);
        out.push_back(kAlphabet[(combined >> 12U) & 0x3FU]);
        out.push_back(i + 1 < data.size() ? kAlphabet[(combined >> 6U) & 0x3FU] : '=');
        out.push_back(i + 2 < data.size() ? kAlphabet[combined & 0x3FU] : '=');
    }
    return out;
}

std::optional<std::vector<std::byte>> base64_decode_bytes(const std::string& input) {
    auto decode_char = [](char c) -> std::optional<unsigned> {
        const auto* found = std::strchr(kAlphabet, c);
        if (found == nullptr) return std::nullopt;
        return static_cast<unsigned>(found - kAlphabet);
    };
    if (input.size() % 4 != 0) return std::nullopt;
    std::vector<std::byte> out;
    out.reserve((input.size() / 4) * 3);
    for (std::size_t i = 0; i < input.size(); i += 4) {
        const auto d0 = decode_char(input[i]);
        const auto d1 = decode_char(input[i + 1]);
        if (!d0.has_value() || !d1.has_value()) return std::nullopt;
        const auto d2 = input[i + 2] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 2]);
        const auto d3 = input[i + 3] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 3]);
        if (!d2.has_value() || !d3.has_value()) return std::nullopt;
        const auto combined = (*d0 << 18U) | (*d1 << 12U) | (*d2 << 6U) | *d3;
        out.push_back(static_cast<std::byte>((combined >> 16U) & 0xFFU));
        if (input[i + 2] != '=') out.push_back(static_cast<std::byte>((combined >> 8U) & 0xFFU));
        if (input[i + 3] != '=') out.push_back(static_cast<std::byte>(combined & 0xFFU));
    }
    return out;
}

std::filesystem::path make_temp_dir() {
    const auto dir = std::filesystem::temp_directory_path() / ("sklmi-rr-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: sklmi_runtime_runner_smoke <runtime_binary>\n";
        return EXIT_FAILURE;
    }

    const auto runtime_binary = std::filesystem::path(argv[1]);
    if (!std::filesystem::exists(runtime_binary)) {
        std::cerr << "runtime_binary_missing\n";
        return EXIT_FAILURE;
    }

    const auto temp_dir = make_temp_dir();
    const auto socket_path = std::filesystem::temp_directory_path() / ("sklmi-rr-" + std::to_string(::getpid()) + ".sock");
    const auto manifest_path = temp_dir / "bridge.json";
    const auto room_state_path = temp_dir / "room.state";
    const auto runtime_state_root = temp_dir / "runtime-state";
    const auto trace_log_path = temp_dir / "trace.jsonl";
    std::filesystem::create_directories(runtime_state_root);

    {
        std::ofstream manifest(manifest_path, std::ios::trunc);
        manifest << "{\n"
                 << "  \"id\": \"earthbound\",\n"
                 << "  \"type\": \"linkedworld\",\n"
                 << "  \"version\": \"0.1.0-dev\",\n"
                 << "  \"sklmi\": {\n"
                 << "    \"contract_version\": \"1.0\",\n"
                 << "    \"bridge_id\": \"earthbound-runtime-runner\",\n"
                 << "    \"driver_instance_id\": \"runtime-runner-1\",\n"
                 << "    \"core_profile\": \"snes_v1\",\n"
                 << "    \"module\": \"sekaiemu\",\n"
                 << "    \"state_file\": \"earthbound-runtime.bridge.state\",\n"
                 << "    \"poll_interval_ms\": 1,\n"
                 << "    \"checks\": [\n"
                 << "      {\n"
                 << "        \"domain_id\": \"system_ram\",\n"
                 << "        \"address\": 4,\n"
                 << "        \"size\": 1,\n"
                 << "        \"compare\": \"equals\",\n"
                 << "        \"operand_u64\": 90,\n"
                 << "        \"event_type\": \"location_checked\",\n"
                 << "        \"location_id\": 44001,\n"
                 << "        \"location_name\": \"Test Location\"\n"
                 << "      }\n"
                 << "    ],\n"
                 << "    \"actions\": [\n"
                 << "      {\n"
                 << "        \"domain_id\": \"system_ram\",\n"
                 << "        \"address\": 8,\n"
                 << "        \"size\": 1,\n"
                 << "        \"value_u64\": 33,\n"
                 << "        \"item_id\": 55001,\n"
                 << "        \"item_name\": \"Test Item\",\n"
                 << "        \"room_controlled\": true\n"
                 << "      }\n"
                 << "    ]\n"
                 << "  }\n"
                 << "}\n";
    }

    {
        std::ofstream room(room_state_path, std::ios::trunc);
        room << "meta|connected|1\n";
        room << "meta|world_id|world-eb-runner\n";
        room << "meta|seed_id|seed-eb-runner\n";
        room << "meta|seed_hash|RUNNER123\n";
        room << "meta|slot_data|{\"slot\":1,\"name\":\"Jade\"}\n";
        room << "pending|item-1|||33|55001|Test Item\n";
    }

    std::atomic<bool> stop{false};
    std::atomic<bool> ready{false};
    std::vector<std::byte> memory(16, std::byte{0x00});
    memory[4] = std::byte{0x5A};

    std::thread server([&]() {
        const int srv = socket(AF_UNIX, SOCK_STREAM, 0);
        if (srv < 0) return;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const auto path = socket_path.string();
        std::filesystem::remove(socket_path);
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        const auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
        if (bind(srv, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
            std::cerr << "bind_failed:" << errno << ":" << std::strerror(errno) << "\n";
            close_socket(srv);
            return;
        }
        if (listen(srv, 1) < 0) {
            std::cerr << "listen_failed:" << errno << ":" << std::strerror(errno) << "\n";
            close_socket(srv);
            return;
        }
        ready.store(true);
        const int client = accept(srv, nullptr, nullptr);
        if (client < 0) {
            close_socket(srv);
            return;
        }
        while (!stop.load()) {
            const auto line = read_line(client);
            if (line.empty()) break;
            if (line == "VERSION") {
                if (!write_all(client, "1\n")) break;
                continue;
            }
            if (line.find("\"SYSTEM\"") != std::string::npos && line.find("\"DOMAINS\"") != std::string::npos) {
                if (!write_all(client,
                               "[{\"type\":\"SYSTEM_RESPONSE\",\"value\":\"SNES\"},"
                               "{\"type\":\"DOMAINS_RESPONSE\",\"value\":[{\"name\":\"system_ram\",\"size\":16,\"writable\":true,\"endianness\":\"little\"}]}]\n")) break;
                continue;
            }
            if (line.find("\"READ\"") != std::string::npos) {
                const auto address = extract_uint(line, "address").value_or(0);
                const auto size = extract_uint(line, "size").value_or(0);
                std::vector<std::byte> out(size, std::byte{0});
                for (std::size_t i = 0; i < size && address + i < memory.size(); ++i) out[i] = memory[address + i];
                const auto encoded = base64_encode_bytes(out);
                if (!write_all(client, "[{\"type\":\"READ_RESPONSE\",\"value\":\"" + encoded + "\"}]\n")) break;
                continue;
            }
            if (line.find("\"WRITE\"") != std::string::npos) {
                const auto address = extract_uint(line, "address").value_or(0);
                const auto encoded = extract_string(line, "value").value_or("");
                const auto decoded = base64_decode_bytes(encoded);
                if (decoded.has_value()) {
                    for (std::size_t i = 0; i < decoded->size() && address + i < memory.size(); ++i) memory[address + i] = (*decoded)[i];
                }
                if (!write_all(client, "[{\"type\":\"WRITE_RESPONSE\"}]\n")) break;
                continue;
            }
        }
        close_socket(client);
        close_socket(srv);
        std::filesystem::remove(socket_path);
    });

    for (int attempt = 0; attempt < 40 && !ready.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (!ready.load()) {
        stop.store(true);
        if (server.joinable()) server.join();
        std::cerr << "server_not_ready\n";
        return EXIT_FAILURE;
    }

    pid_t child = fork();
    if (child < 0) {
        stop.store(true);
        if (server.joinable()) server.join();
        std::cerr << "fork_failed\n";
        return EXIT_FAILURE;
    }
    if (child == 0) {
        execl(runtime_binary.c_str(),
              runtime_binary.c_str(),
              "--memory-socket", socket_path.c_str(),
              "--bridge-manifest", manifest_path.c_str(),
              "--room-state", room_state_path.c_str(),
              "--runtime-state", runtime_state_root.c_str(),
              "--trace-log", trace_log_path.c_str(),
              "--tick-ms", "1",
              "--max-ticks", "3",
              static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    waitpid(child, &status, 0);
    stop.store(true);
    if (server.joinable()) server.join();

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "runtime_runner_failed\n";
        return EXIT_FAILURE;
    }

    if (memory[8] != std::byte{0x21}) {
        std::cerr << "runtime_runner_did_not_inject_item\n";
        return EXIT_FAILURE;
    }

    const auto room_text = slurp(room_state_path);
    const auto room_sync_text = slurp(runtime_state_root / "earthbound_runtime_runner.room-sync.state");
    const auto trace_text = slurp(trace_log_path);
    if (!contains(room_text, "meta|world_id|world-eb-runner") ||
        !contains(room_text, "meta|seed_id|seed-eb-runner") ||
        !contains(room_text, "meta|seed_hash|RUNNER123") ||
        !contains(room_text, "meta|slot_data|{\"slot\":1,\"name\":\"Jade\"}") ||
        !contains(room_text, "checked|44001|Test Location") ||
        !contains(room_text, "consumed|item-1|||33|55001|Test Item")) {
        std::cerr << "runtime_runner_room_state_invalid\n";
        return EXIT_FAILURE;
    }
    if (!contains(room_sync_text, "meta|room_mode|offline") ||
        !contains(room_sync_text, "meta|world_id|world-eb-runner") ||
        !contains(room_sync_text, "meta|seed_id|seed-eb-runner") ||
        !contains(room_sync_text, "meta|seed_hash|RUNNER123") ||
        !contains(room_sync_text, "meta|slot_data|{\"slot\":1,\"name\":\"Jade\"}") ||
        !contains(room_sync_text, "meta|driver_instance_id|runtime-runner-1") ||
        !contains(room_sync_text, "meta|linkedworld_id|earthbound") ||
        !contains(room_sync_text, "meta|core_profile|snes_v1") ||
        !contains(room_sync_text, "reported|44001|Test Location") ||
        !contains(room_sync_text, "applied|item-1|Test Item")) {
        std::cerr << "runtime_runner_room_sync_state_invalid\n";
        return EXIT_FAILURE;
    }
    if (!contains(trace_text, "\"record_type\":\"trace\"") ||
        !contains(trace_text, "\"event\":\"manifest_loaded\"") ||
        !contains(trace_text, "bridge_id=earthbound-runtime-runner linkedworld_id=earthbound core_profile=snes_v1 checks=1 injections=1") ||
        !contains(trace_text, "\"event\":\"runtime_config\"") ||
        !contains(trace_text, "mode=offline tick_ms=1 max_ticks=3") ||
        !contains(trace_text, "\"event\":\"room_client_ready\"") ||
        !contains(trace_text, "\"event\":\"session_start\"") ||
        !contains(trace_text, "\"event\":\"stop_condition\"") ||
        !contains(trace_text, "\"record_type\":\"event\"") ||
        !contains(trace_text, "\"driver_instance_id\":\"runtime-runner-1\"") ||
        !contains(trace_text, "\"linkedworld_id\":\"earthbound\"") ||
        !contains(trace_text, "\"core_profile\":\"snes_v1\"")) {
        std::cerr << "runtime_runner_trace_log_invalid\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_runtime_runner_smoke_ok\n";
    return EXIT_SUCCESS;
}

#else

int main() {
    return 0;
}

#endif
