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
#include <arpa/inet.h>
#include <netinet/in.h>
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
    const auto dir = std::filesystem::temp_directory_path() / ("sklmi-alttp-gs-" + std::to_string(::getpid()));
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

#endif

}  // namespace

int main(int argc, char** argv) {
#ifndef _WIN32
    if (argc < 2) {
        std::cerr << "usage: sklmi_alttp_runtime_game_server_smoke <runtime_binary>\n";
        return EXIT_FAILURE;
    }
    try {
        const auto runtime_binary = std::filesystem::path(argv[1]);
        const auto temp_dir = make_temp_dir();
        const auto socket_path = std::filesystem::temp_directory_path() / ("sklmi-alttp-gs-" + std::to_string(::getpid()) + ".sock");
        const auto manifest_path = temp_dir / "bridge.json";
        const auto room_state_path = temp_dir / "room.state";
        const auto runtime_state_root = temp_dir / "runtime-state";
        const auto trace_log_path = temp_dir / "trace.jsonl";
        std::filesystem::create_directories(runtime_state_root);

        {
            std::ofstream manifest(manifest_path, std::ios::trunc);
            manifest << "{\n"
                     << "  \"id\": \"alttp\",\n"
                     << "  \"type\": \"linkedworld\",\n"
                     << "  \"version\": \"0.1.0-dev\",\n"
                     << "  \"sklmi\": {\n"
                     << "    \"contract_version\": \"1.0\",\n"
                     << "    \"bridge_id\": \"alttp-runtime-game-server\",\n"
                     << "    \"driver_instance_id\": \"alttp-game-server-1\",\n"
                     << "    \"core_profile\": \"snes_v1\",\n"
                     << "    \"module\": \"sekaiemu\",\n"
                     << "    \"state_file\": \"alttp-runtime.bridge.state\",\n"
                     << "    \"poll_interval_ms\": 1,\n"
                     << "    \"checks\": [\n"
                     << "      {\n"
                     << "        \"domain_id\": \"system_ram\",\n"
                     << "        \"address\": 61476,\n"
                     << "        \"size\": 1,\n"
                     << "        \"compare\": \"mask_any\",\n"
                     << "        \"operand_u64\": 16,\n"
                     << "        \"event_type\": \"location_checked\",\n"
                     << "        \"location_id\": 60025,\n"
                     << "        \"location_name\": \"Sanctuary\",\n"
                     << "        \"event_key\": \"0xEA79\",\n"
                     << "        \"mapped_value\": \"Sanctuary\"\n"
                     << "      },\n"
                     << "      {\n"
                     << "        \"domain_id\": \"system_ram\",\n"
                     << "        \"address\": 61960,\n"
                     << "        \"size\": 1,\n"
                     << "        \"compare\": \"mask_any\",\n"
                     << "        \"operand_u64\": 16,\n"
                     << "        \"event_type\": \"location_checked\",\n"
                     << "        \"location_id\": 59836,\n"
                     << "        \"location_name\": \"Link's House\",\n"
                     << "        \"event_key\": \"0xE9BC\",\n"
                     << "        \"mapped_value\": \"Link's House\"\n"
                     << "      }\n"
                     << "    ],\n"
                     << "    \"actions\": [\n"
                     << "      {\n"
                     << "        \"item_id\": 10,\n"
                     << "        \"item_name\": \"Hookshot\",\n"
                     << "        \"event_key\": \"item.hookshot\",\n"
                     << "        \"mapped_value\": \"Hookshot\",\n"
                     << "        \"room_controlled\": true,\n"
                     << "        \"writes\": [\n"
                     << "          {\n"
                     << "            \"domain_id\": \"system_ram\",\n"
                     << "            \"address\": 62672,\n"
                     << "            \"size\": 2,\n"
                     << "            \"source\": \"current_plus\",\n"
                     << "            \"delta_u64\": 1\n"
                     << "          },\n"
                     << "          {\n"
                     << "            \"domain_id\": \"system_ram\",\n"
                     << "            \"address\": 62674,\n"
                     << "            \"size\": 1,\n"
                     << "            \"source\": \"constant\",\n"
                     << "            \"value_u64\": 10\n"
                     << "          },\n"
                     << "          {\n"
                     << "            \"domain_id\": \"system_ram\",\n"
                     << "            \"address\": 62675,\n"
                     << "            \"size\": 1,\n"
                     << "            \"source\": \"constant\",\n"
                     << "            \"value_u64\": 0\n"
                     << "          }\n"
                     << "        ]\n"
                     << "      }\n"
                     << "    ]\n"
                     << "  }\n"
                     << "}\n";
        }

        {
            std::ofstream room(room_state_path, std::ios::trunc);
            room << "meta|world_id|world-alttp-gs\n";
            room << "meta|seed_id|seed-alttp-gs\n";
            room << "meta|seed_hash|ALTTPGS\n";
        }

        std::atomic<bool> stop{false};
        std::atomic<bool> unix_ready{false};
        std::atomic<bool> control_ready{false};
        std::atomic<bool> runtime_ready{false};
        std::vector<std::byte> memory(0xF600, std::byte{0x00});
        memory[0xF024] = std::byte{0x10};
        memory[0xF208] = std::byte{0x10};
        memory[0xF4D0] = std::byte{0x0C};
        memory[0xF4D1] = std::byte{0x00};
        std::string issue_ticket_request;
        std::string runtime_event_request;
        std::string pending_items_request;
        std::string acknowledge_request;

        std::thread unix_server([&]() {
            const int srv = socket(AF_UNIX, SOCK_STREAM, 0);
            if (srv < 0) return;
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            const auto path = socket_path.string();
            std::filesystem::remove(socket_path);
            std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
            const auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
            if (bind(srv, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
                close_socket(srv);
                return;
            }
            if (listen(srv, 1) < 0) {
                close_socket(srv);
                return;
            }
            unix_ready.store(true);
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
        });

        const int control_srv = socket(AF_INET, SOCK_STREAM, 0);
        if (control_srv < 0) {
            std::cerr << "control_server_socket_failed\n";
            return EXIT_FAILURE;
        }
        int reuse = 1;
        setsockopt(control_srv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in control_addr{};
        control_addr.sin_family = AF_INET;
        control_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        control_addr.sin_port = htons(0);
        if (bind(control_srv, reinterpret_cast<sockaddr*>(&control_addr), sizeof(control_addr)) != 0 ||
            listen(control_srv, 4) != 0) {
            std::cerr << "control_server_bind_failed\n";
            close_socket(control_srv);
            return EXIT_FAILURE;
        }
        socklen_t control_len = sizeof(control_addr);
        getsockname(control_srv, reinterpret_cast<sockaddr*>(&control_addr), &control_len);
        const auto control_port = ntohs(control_addr.sin_port);

        const int runtime_srv = socket(AF_INET, SOCK_STREAM, 0);
        if (runtime_srv < 0) {
            std::cerr << "runtime_server_socket_failed\n";
            close_socket(control_srv);
            return EXIT_FAILURE;
        }
        setsockopt(runtime_srv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in runtime_addr{};
        runtime_addr.sin_family = AF_INET;
        runtime_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        runtime_addr.sin_port = htons(0);
        if (bind(runtime_srv, reinterpret_cast<sockaddr*>(&runtime_addr), sizeof(runtime_addr)) != 0 ||
            listen(runtime_srv, 8) != 0) {
            std::cerr << "runtime_server_bind_failed\n";
            close_socket(control_srv);
            close_socket(runtime_srv);
            return EXIT_FAILURE;
        }
        socklen_t runtime_len = sizeof(runtime_addr);
        getsockname(runtime_srv, reinterpret_cast<sockaddr*>(&runtime_addr), &runtime_len);
        const auto runtime_port = ntohs(runtime_addr.sin_port);

        std::thread control_server([&]() {
            control_ready.store(true);
            const int client = accept(control_srv, nullptr, nullptr);
            if (client < 0) return;
            issue_ticket_request = read_line(client);
            write_all(client,
                      "{\"ok\":true,\"ticket\":{\"session_token\":\"runtime-ticket-alttp-1\"},"
                      "\"seed_id\":\"seed-alttp-gs\",\"seed_hash\":\"ALTTPGS\"}\n");
            close_socket(client);
            close_socket(control_srv);
        });

        std::thread runtime_server([&]() {
            runtime_ready.store(true);
            bool item_still_pending = true;
            while (!stop.load()) {
                const int client = accept(runtime_srv, nullptr, nullptr);
                if (client < 0) return;
                const auto line = read_line(client);
                if (line.empty()) {
                    close_socket(client);
                    continue;
                }
                if (line.find("\"cmd\":\"runtime_event\"") != std::string::npos) {
                    runtime_event_request += line;
                    runtime_event_request.push_back('\n');
                    write_all(client, "{\"ok\":true,\"accepted\":true,\"duplicate\":false,\"reason\":\"accepted\"}\n");
                } else if (line.find("\"cmd\":\"pending_items\"") != std::string::npos) {
                    pending_items_request = line;
                    if (item_still_pending) {
                        write_all(client, "{\"ok\":true,\"pending_items\":[{\"delivery_id\":7,\"tracker_semantic_id\":\"item.hookshot\",\"mapped_value\":\"Hookshot\"}]}\n");
                    } else {
                        write_all(client, "{\"ok\":true,\"pending_items\":[]}\n");
                    }
                } else if (line.find("\"cmd\":\"acknowledge_delivery\"") != std::string::npos) {
                    acknowledge_request = line;
                    item_still_pending = false;
                    write_all(client, "{\"ok\":true}\n");
                } else {
                    write_all(client, "{\"ok\":false,\"error\":\"unexpected_command\"}\n");
                }
                close_socket(client);
            }
            close_socket(runtime_srv);
        });

        while (!unix_ready.load() || !control_ready.load() || !runtime_ready.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        pid_t child = fork();
        if (child == 0) {
            execl(runtime_binary.c_str(),
                  runtime_binary.c_str(),
                  "--memory-socket", socket_path.c_str(),
                  "--bridge-manifest", manifest_path.c_str(),
                  "--room-state", room_state_path.c_str(),
                  "--runtime-state", runtime_state_root.c_str(),
                  "--trace-log", trace_log_path.c_str(),
                  "--mode", "sekailink_game_server",
                  "--room-host", "localhost",
                  "--room-control-port", std::to_string(control_port).c_str(),
                  "--room-runtime-port", std::to_string(runtime_port).c_str(),
                  "--room-session-name", "alttp-session",
                  "--room-slot-id", "1",
                  "--room-control-channel", "core",
                  "--room-control-auth-token", "core-secret",
                  "--room-runtime-auth-token", "runtime-secret",
                  "--tick-ms", "1",
                  "--max-ticks", "3",
                  static_cast<char*>(nullptr));
            _exit(127);
        }

        int status = 0;
        waitpid(child, &status, 0);
        stop.store(true);
        shutdown(runtime_srv, SHUT_RDWR);
        close_socket(runtime_srv);
        if (unix_server.joinable()) unix_server.join();
        if (control_server.joinable()) control_server.join();
        if (runtime_server.joinable()) runtime_server.join();

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::cerr << "alttp_runtime_game_server_failed\n";
            return EXIT_FAILURE;
        }

        const std::uint16_t progress_value =
            static_cast<std::uint16_t>(std::to_integer<unsigned char>(memory[0xF4D0])) |
            (static_cast<std::uint16_t>(std::to_integer<unsigned char>(memory[0xF4D1])) << 8U);
        if (progress_value != 0x000D || memory[0xF4D2] != std::byte{0x0A} || memory[0xF4D3] != std::byte{0x00}) {
            std::cerr << "alttp_runtime_game_server_did_not_inject_hookshot\n";
            return EXIT_FAILURE;
        }

        const auto trace_text = slurp(trace_log_path);
        const auto preferred_room_sync_path = runtime_state_root / "alttp_runtime_game_server.room-sync.state";
        const auto actual_room_sync_path =
            std::filesystem::exists(preferred_room_sync_path) ? preferred_room_sync_path : room_state_path;
        const auto room_sync_text = slurp(actual_room_sync_path);
        if (!contains(issue_ticket_request, "\"cmd\":\"issue_ticket\"") ||
            !contains(runtime_event_request, "\"canonical_id\":60025") ||
            !contains(runtime_event_request, "\"canonical_id\":59836") ||
            !contains(pending_items_request, "\"cmd\":\"pending_items\"") ||
            !contains(acknowledge_request, "\"cmd\":\"acknowledge_delivery\"") ||
            !contains(trace_text, "host=localhost control_port=") ||
            !contains(trace_text, "runtime_port=") ||
            !contains(trace_text, "\"event\":\"room_item_pending\"") ||
            !contains(trace_text, "\"event\":\"room_item_applied\"") ||
            !contains(trace_text, "\"event\":\"room_item_acknowledged\"") ||
            !contains(trace_text, "event_key=item.hookshot mapped_value=Hookshot mode=write_sequence steps=3") ||
            contains(trace_text, "no_matching_injection_rule") ||
            !contains(room_sync_text, "meta|room_mode|sekailink_game_server") ||
            !contains(room_sync_text, "meta|seed_id|seed-alttp-gs") ||
            !contains(room_sync_text, "meta|seed_hash|ALTTPGS") ||
            !contains(room_sync_text, "reported|60025|Sanctuary") ||
            !contains(room_sync_text, "reported|59836|Link's House") ||
            !contains(room_sync_text, "applied|delivery:7|Hookshot")) {
            std::cerr << "alttp_runtime_game_server_protocol_invalid\n";
            return EXIT_FAILURE;
        }

        std::cout << "sklmi_alttp_runtime_game_server_smoke_ok\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "sklmi_alttp_runtime_game_server_smoke failed: " << exception.what() << "\n";
        return EXIT_FAILURE;
    }
#else
    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
#endif
}
