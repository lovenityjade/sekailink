#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#ifndef _WIN32

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void close_socket(int fd) {
    if (fd >= 0) close(fd);
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
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
    const auto dir = std::filesystem::temp_directory_path() /
                     ("sklmi-ap-runtime-live-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string arg_value(const std::vector<std::string>& args,
                      const std::string& name,
                      const std::string& fallback = {}) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == name) {
            return args[index + 1];
        }
    }
    return fallback;
}

void print_usage(const char* exe) {
    std::cerr
        << "Usage: " << exe
        << " --runtime <sekailink_sklmi_runtime> --manifest <bridge.json>"
        << " --ap-host <host> --ap-port <port> --ap-game <game> --ap-slot <slot>\n";
}

#endif

}  // namespace

int main(int argc, char** argv) {
#ifndef _WIN32
    const std::vector<std::string> args(argv + 1, argv + argc);
    const auto runtime_binary = std::filesystem::path(arg_value(args, "--runtime"));
    const auto manifest_path = std::filesystem::path(arg_value(args, "--manifest"));
    const auto ap_host = arg_value(args, "--ap-host", "127.0.0.1");
    const auto ap_port = arg_value(args, "--ap-port");
    const auto ap_game = arg_value(args, "--ap-game", "A Link to the Past");
    const auto ap_slot = arg_value(args, "--ap-slot", "Jade-ALTTP");

    if (runtime_binary.empty() || manifest_path.empty() || ap_port.empty() ||
        !std::filesystem::exists(runtime_binary) || !std::filesystem::exists(manifest_path)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const auto temp_dir = make_temp_dir();
    const auto socket_path = std::filesystem::temp_directory_path() /
                             ("sklmi-ap-runtime-live-" + std::to_string(::getpid()) + ".sock");
    const auto room_state_path = temp_dir / "room.state";
    const auto runtime_state_root = temp_dir / "runtime-state";
    const auto trace_log_path = temp_dir / "trace.jsonl";
    std::filesystem::create_directories(runtime_state_root);

    std::atomic<bool> stop{false};
    std::atomic<bool> ready{false};
    std::vector<std::byte> memory(62976, std::byte{0x00});
    memory[61476] = std::byte{0x10};  // Sanctuary checked bit in the phase-1 ALTTP manifest.

    std::thread memory_server([&]() {
        const int srv = socket(AF_UNIX, SOCK_STREAM, 0);
        if (srv < 0) return;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const auto path = socket_path.string();
        std::filesystem::remove(socket_path);
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        const auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
        if (bind(srv, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0 || listen(srv, 1) < 0) {
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
                               "{\"type\":\"DOMAINS_RESPONSE\",\"value\":[{\"name\":\"system_ram\",\"size\":62976,\"writable\":true,\"endianness\":\"little\"}]}]\n")) break;
                continue;
            }
            if (line.find("\"READ\"") != std::string::npos) {
                const auto address = extract_uint(line, "address").value_or(0);
                const auto size = extract_uint(line, "size").value_or(0);
                std::vector<std::byte> out(size, std::byte{0});
                for (std::size_t i = 0; i < size && address + i < memory.size(); ++i) {
                    out[i] = memory[address + i];
                }
                if (!write_all(client, "[{\"type\":\"READ_RESPONSE\",\"value\":\"" + base64_encode_bytes(out) + "\"}]\n")) break;
                continue;
            }
            if (line.find("\"WRITE\"") != std::string::npos) {
                const auto address = extract_uint(line, "address").value_or(0);
                const auto encoded = extract_string(line, "value").value_or("");
                const auto decoded = base64_decode_bytes(encoded);
                if (decoded.has_value()) {
                    for (std::size_t i = 0; i < decoded->size() && address + i < memory.size(); ++i) {
                        memory[address + i] = (*decoded)[i];
                    }
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
        if (memory_server.joinable()) memory_server.join();
        std::cerr << "memory_server_not_ready\n";
        return EXIT_FAILURE;
    }

    const auto child = fork();
    if (child < 0) {
        stop.store(true);
        if (memory_server.joinable()) memory_server.join();
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
              "--mode", "archipelago",
              "--ap-host", ap_host.c_str(),
              "--ap-port", ap_port.c_str(),
              "--ap-game", ap_game.c_str(),
              "--ap-slot-name", ap_slot.c_str(),
              "--ap-uuid", "sekailink-sklmi-runtime-live-probe",
              "--ap-tags", "AP,SekaiLink,SKLMI,RuntimeProbe",
              "--tick-ms", "1",
              "--max-ticks", "8",
              static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    waitpid(child, &status, 0);
    stop.store(true);
    if (memory_server.joinable()) memory_server.join();

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "runtime_failed\n";
        std::cerr << slurp(trace_log_path);
        return EXIT_FAILURE;
    }

    const auto room_sync_text = slurp(runtime_state_root / "alttp_phase1.room-sync.state");
    const auto trace_text = slurp(trace_log_path);
    if (!contains(room_sync_text, "meta|room_mode|archipelago") ||
        !contains(room_sync_text, "meta|seed_name|") ||
        !contains(room_sync_text, "reported|60025|Sanctuary") ||
        !contains(trace_text, "\"event_type\":\"location_checked\"") ||
        !contains(trace_text, "\"canonical_id\":60025") ||
        contains(trace_text, "\"event\":\"report_location_checked\"")) {
        std::cerr << "runtime_live_probe_invalid\n";
        std::cerr << "room_sync:\n" << room_sync_text << "\ntrace:\n" << trace_text << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_archipelago_runtime_live_probe_ok\n";
    std::cout << "room_sync=" << (runtime_state_root / "alttp_phase1.room-sync.state") << "\n";
    std::cout << "trace_log=" << trace_log_path << "\n";
    return EXIT_SUCCESS;
#else
    std::cout << "sklmi_archipelago_runtime_live_probe_skipped_windows\n";
    return EXIT_SUCCESS;
#endif
}
