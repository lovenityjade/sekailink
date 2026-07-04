#include "sekailink_sklmi/api.hpp"

#ifndef _WIN32

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace sekailink::sklmi;

namespace {

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

}  // namespace

int main() {
    const auto socket_path = std::filesystem::path("/tmp") / ("sklmi-unix-" + std::to_string(::getpid()) + ".sock");
    std::filesystem::remove(socket_path);

    std::atomic<bool> stop{false};
    std::atomic<bool> ready{false};
    std::vector<std::byte> memory(16);
    memory[4] = std::byte{0x7F};

    std::thread server([&]() {
        const int srv = socket(AF_UNIX, SOCK_STREAM, 0);
        if (srv < 0) return;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const auto path = socket_path.string();
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        const auto addr_len =
            static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
        if (bind(srv, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
            close_socket(srv);
            return;
        }
        if (listen(srv, 1) < 0) {
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

    UnixSocketMemoryProvider provider(socket_path);
    std::string error;
    bool connected = false;
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (provider.connect(&error)) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (!connected) {
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
    if (!provider.has_domain("system_ram") || !provider.is_address_valid("system_ram", 4, 1)) {
        std::cerr << "domain_invalid\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    std::byte read_byte{};
    if (!provider.read("system_ram", 4, &read_byte, 1) || read_byte != std::byte{0x7F}) {
        std::cerr << "read_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    const std::byte write_byte = std::byte{0x33};
    if (!provider.write("system_ram", 5, &write_byte, 1) || memory[5] != std::byte{0x33}) {
        std::cerr << "write_failed\n";
        stop.store(true);
        provider.disconnect();
        if (server.joinable()) server.join();
        return EXIT_FAILURE;
    }

    provider.disconnect();
    stop.store(true);
    if (server.joinable()) server.join();
    std::cout << "sklmi_unix_socket_smoke_ok\n";
    return EXIT_SUCCESS;
}

#else

int main() {
    return 0;
}

#endif
