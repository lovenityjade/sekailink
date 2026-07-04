#include "sekailink/base64.hpp"
#include "sekailink/snes_runtime.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

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

namespace sekailink {

namespace {

constexpr int kScriptVersion = 1;

std::atomic<bool> g_stop_requested{false};

std::string errno_message(const std::string& prefix) {
    return prefix + ":" + std::to_string(errno) + ":" + std::strerror(errno);
}

void close_socket(const socket_handle handle) {
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

void init_sockets() {
#ifdef _WIN32
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("winsock_startup_failed");
    }
#endif
}

void shutdown_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string read_line(const socket_handle client) {
    std::string line;
    char ch = '\0';
    while (true) {
        const int received = recv(client, &ch, 1, 0);
        if (received == 0) return {};
        if (received < 0) throw std::runtime_error("socket_read_failed");
        if (ch == '\n') break;
        if (ch != '\r') line.push_back(ch);
    }
    return line;
}

void write_all(const socket_handle client, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const int count = send(client, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (count <= 0) throw std::runtime_error("socket_write_failed");
        sent += static_cast<std::size_t>(count);
    }
}

struct Args {
    std::string host = "127.0.0.1";
    int port = 43056;
    std::string rom;
    std::optional<std::string> save;
};

std::string default_save_path(const std::string& rom_path) {
    std::filesystem::path path(rom_path);
    path.replace_extension(".srm");
    return path.string();
}

Args parse_args(const std::span<char*> argv) {
    Args args;
    for (std::size_t index = 1; index < argv.size(); ++index) {
        const std::string_view arg = argv[index];
        auto take_value = [&](const std::string_view name) -> std::string {
            if (index + 1 >= argv.size()) throw std::runtime_error("missing_value_for_" + std::string(name));
            ++index;
            return argv[index];
        };

        if (arg == "--host") {
            args.host = take_value("host");
        } else if (arg == "--port") {
            args.port = std::stoi(take_value("port"));
        } else if (arg == "--rom") {
            args.rom = take_value("rom");
        } else if (arg == "--save") {
            args.save = take_value("save");
        } else {
            throw std::runtime_error("unknown_argument:" + std::string(arg));
        }
    }

    if (args.rom.empty()) throw std::runtime_error("missing_required_argument:--rom");
    return args;
}

nlohmann::json process_request(SnesRuntime& runtime, const nlohmann::json& req) {
    const std::string type = req.at("type").get<std::string>();

    if (type == "PING") return {{"type", "PONG"}};
    if (type == "SYSTEM") return {{"type", "SYSTEM_RESPONSE"}, {"value", runtime.system()}};
    if (type == "PREFERRED_CORES") return {{"type", "PREFERRED_CORES_RESPONSE"}, {"value", nlohmann::json::object()}};
    if (type == "HASH") return {{"type", "HASH_RESPONSE"}, {"value", runtime.rom_hash()}};
    if (type == "MEMORY_SIZE") {
        return {{"type", "MEMORY_SIZE_RESPONSE"}, {"value", runtime.memory_size(req.at("domain").get<std::string>())}};
    }
    if (type == "DOMAINS") {
        nlohmann::json domains = nlohmann::json::array();
        for (const auto& domain : runtime.memory_domains()) {
            domains.push_back({
                {"name", domain.name},
                {"size", domain.size},
                {"writable", domain.writable},
                {"endianness", "little"},
            });
        }
        return {{"type", "DOMAINS_RESPONSE"}, {"value", std::move(domains)}};
    }
    if (type == "GUARD") {
        const auto address = req.at("address").get<std::uint32_t>();
        const auto domain = req.at("domain").get<std::string>();
        const auto expected = base64_decode(req.at("expected_data").get<std::string>());
        const auto actual = runtime.read_memory(address, expected.size(), domain);
        return {{"type", "GUARD_RESPONSE"}, {"value", actual == expected}, {"address", address}};
    }
    if (type == "READ") {
        const auto value = runtime.read_memory(
            req.at("address").get<std::uint32_t>(),
            req.at("size").get<std::size_t>(),
            req.at("domain").get<std::string>());
        return {{"type", "READ_RESPONSE"}, {"value", base64_encode(value)}};
    }
    if (type == "WRITE") {
        runtime.write_memory(
            req.at("address").get<std::uint32_t>(),
            base64_decode(req.at("value").get<std::string>()),
            req.at("domain").get<std::string>());
        return {{"type", "WRITE_RESPONSE"}};
    }

    return {{"type", "ERROR"}, {"err", "Unknown command: " + type}};
}

std::string handle_line(SnesRuntime& runtime, const std::string& line) {
    if (line == "VERSION") return std::to_string(kScriptVersion) + "\n";
    const auto requests = nlohmann::json::parse(line);
    if (!requests.is_array()) throw std::runtime_error("request_must_be_array");
    nlohmann::json responses = nlohmann::json::array();
    for (const auto& req : requests) {
        try {
            responses.push_back(process_request(runtime, req));
        } catch (const std::exception& ex) {
            responses.push_back({{"type", "ERROR"}, {"err", ex.what()}});
        }
    }
    return responses.dump() + "\n";
}

void serve_forever(SnesRuntime& runtime, const Args& args) {
    init_sockets();
    socket_handle server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == kInvalidSocket) {
        shutdown_sockets();
        throw std::runtime_error("socket_create_failed");
    }

    int opt = 1;
#ifndef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(args.port));
    if (inet_pton(AF_INET, args.host.c_str(), &addr.sin_addr) != 1) {
        close_socket(server);
        shutdown_sockets();
        throw std::runtime_error("invalid_bind_address");
    }
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket(server);
        shutdown_sockets();
        throw std::runtime_error(errno_message("socket_bind_failed"));
    }
    if (listen(server, 4) < 0) {
        close_socket(server);
        shutdown_sockets();
        throw std::runtime_error(errno_message("socket_listen_failed"));
    }

    std::cout << "[sekailink-runtime-snes] listening on " << args.host << ":" << args.port
              << " protocol=ULCPv1 system=SNES rom_hash=" << runtime.rom_hash();
    if (const auto save_path = runtime.save_path(); save_path.has_value()) {
        std::cout << " save=" << *save_path;
    }
    std::cout << std::endl;

    while (!g_stop_requested.load()) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(server, &read_set);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        const int ready = select(server + 1, &read_set, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (g_stop_requested.load()) break;
            continue;
        }
        if (ready == 0 || !FD_ISSET(server, &read_set)) continue;

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        socket_handle client = accept(server, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client == kInvalidSocket) {
            if (g_stop_requested.load()) break;
            continue;
        }
        std::thread([client, &runtime]() {
            try {
                while (!g_stop_requested.load()) {
                    const std::string line = read_line(client);
                    if (line.empty()) break;
                    write_all(client, handle_line(runtime, line));
                }
            } catch (const std::exception& ex) {
                try {
                    write_all(client, nlohmann::json::array({{{"type", "ERROR"}, {"err", ex.what()}}}).dump() + "\n");
                } catch (...) {}
            }
            close_socket(client);
        }).detach();
    }

    close_socket(server);
    shutdown_sockets();
}

void on_signal(int) {
    g_stop_requested.store(true);
}

}  // namespace

}  // namespace sekailink

int main(int argc, char** argv) {
    using namespace sekailink;
    try {
        const auto args = parse_args(std::span<char*>(argv, static_cast<std::size_t>(argc)));
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        SnesRuntime runtime;
        runtime.set_save_path(args.save.value_or(default_save_path(args.rom)));
        runtime.load_rom(args.rom);
        runtime.start();
        serve_forever(runtime, args);
        g_stop_requested.store(true);
        runtime.stop();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[sekailink-runtime-snes] fatal: " << ex.what() << '\n';
        return 1;
    }
}
