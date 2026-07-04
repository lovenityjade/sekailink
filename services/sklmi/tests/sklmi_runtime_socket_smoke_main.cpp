#include "sekailink_sklmi/api.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_handle = SOCKET;
constexpr socket_handle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_handle = int;
constexpr socket_handle kInvalidSocket = -1;
#endif

using namespace sekailink::sklmi;

namespace {

void close_socket(socket_handle handle) {
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

std::string read_line(socket_handle client) {
    std::string line;
    char ch = '\0';
    while (true) {
        const int received = recv(client, &ch, 1, 0);
        if (received <= 0) return {};
        if (ch == '\n') break;
        if (ch != '\r') line.push_back(ch);
    }
    return line;
}

bool write_all(socket_handle client, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const int written = send(client, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (written <= 0) return false;
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

void replace_all(std::string& text, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::optional<std::string> extract_between(const std::string& text, const std::string& left, const std::string& right) {
    const auto begin = text.find(left);
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + left.size();
    const auto end = text.find(right, start);
    if (end == std::string::npos) return std::nullopt;
    return text.substr(start, end - start);
}

std::optional<std::uint64_t> extract_uint(const std::string& text, const std::string& key) {
    const auto value = extract_between(text, "\"" + key + "\":", ",");
    if (!value.has_value()) {
        const auto last = extract_between(text, "\"" + key + "\":", "}");
        if (!last.has_value()) return std::nullopt;
        return static_cast<std::uint64_t>(std::stoull(*last));
    }
    return static_cast<std::uint64_t>(std::stoull(*value));
}

std::string unescape_json(std::string value) {
    replace_all(value, "\\\\", "\\");
    replace_all(value, "\\\"", "\"");
    replace_all(value, "\\n", "\n");
    replace_all(value, "\\r", "\r");
    replace_all(value, "\\t", "\t");
    return value;
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

}  // namespace

int main() {
    constexpr std::uint16_t port = 43123;
    std::atomic<bool> stop{false};
    std::vector<std::byte> memory(16);
    memory[4] = std::byte{0x7F};

    std::thread server([&]() {
        socket_handle srv = socket(AF_INET, SOCK_STREAM, 0);
        if (srv == kInvalidSocket) return;
        int opt = 1;
#ifndef _WIN32
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close_socket(srv);
            return;
        }
        if (listen(srv, 1) < 0) {
            close_socket(srv);
            return;
        }
        socket_handle client = accept(srv, nullptr, nullptr);
        if (client == kInvalidSocket) {
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
                    "{\"type\":\"DOMAINS_RESPONSE\",\"value\":[{\"name\":\"WRAM\",\"size\":16,\"writable\":true,\"endianness\":\"little\"}]}]\n")) break;
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
                const auto encoded = extract_between(line, "\"value\":\"", "\"").value_or("");
                const auto decoded = base64_decode_bytes(unescape_json(encoded));
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

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    RuntimeSocketMemoryProvider provider("127.0.0.1", port);
    std::string error;
    if (!provider.connect(&error)) {
        std::cerr << "connect_failed:" << error << "\n";
        stop.store(true);
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }
    if (provider.system_name() != "SNES" || !provider.protocol_version().has_value() || *provider.protocol_version() != 1) {
        std::cerr << "metadata_invalid\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }
    if (!provider.has_domain("WRAM") || !provider.is_address_valid("WRAM", 4, 1)) {
        std::cerr << "domain_invalid\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    std::byte read_byte{};
    if (!provider.read("WRAM", 4, &read_byte, 1) || read_byte != std::byte{0x7F}) {
        std::cerr << "read_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    const std::byte write_byte = std::byte{0x33};
    if (!provider.write("WRAM", 5, &write_byte, 1) || memory[5] != std::byte{0x33}) {
        std::cerr << "write_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    VectorEventSink sink;
    BridgeManifest manifest{
        .linkedworld_id = "earthbound",
        .bridge_id = "remote-snes-proof",
        .module = "manifest",
        .state_file = "",
        .poll_interval_ms = 1,
        .checks = {WatchRule{
            .domain_id = "WRAM",
            .address = 4,
            .size = 1,
            .compare = CompareOp::equals,
            .operand_u64 = 127,
            .event_type = EventType::location_checked,
            .event_key = "check.remote",
            .mapped_value = "loc:remote"
        }},
        .injections = {InjectRule{
            .domain_id = "WRAM",
            .address = 6,
            .value_u64 = 1,
            .size = 1,
            .event_key = "item.remote",
            .mapped_value = "item:remote"
        }}
    };

    BasicRuntimeSession session(provider, sink, std::make_unique<ManifestBridgeSession>(manifest));
    if (session.start().state != RuntimeConnectionState::connected) {
        std::cerr << "session_start_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }
    if (session.tick({.tick_index = 1, .monotonic_ms = 10}).state != RuntimeConnectionState::connected) {
        std::cerr << "session_tick_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    bool saw_check = false;
    bool saw_item = false;
    for (const auto& event : sink.events()) {
        if (event.type == EventType::location_checked && event.value == "loc:remote") saw_check = true;
        if (event.type == EventType::item_received && event.value == "item:remote") saw_item = true;
    }
    if (!saw_check || !saw_item || memory[6] != std::byte{0x01}) {
        std::cerr << "bridge_events_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    provider.disconnect();
    stop.store(true);
    if (server.joinable()) server.join();
    std::cout << "sklmi_runtime_socket_smoke_ok\n";
    return EXIT_SUCCESS;
}
