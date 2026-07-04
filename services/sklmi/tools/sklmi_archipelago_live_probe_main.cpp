#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace sekailink::sklmi;

namespace {

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

std::uint16_t arg_port(const std::vector<std::string>& args,
                       const std::string& name,
                       std::uint16_t fallback) {
    const auto value = arg_value(args, name);
    if (value.empty()) {
        return fallback;
    }
    try {
        const auto parsed = std::stoul(value);
        if (parsed == 0 || parsed > 65535) {
            return fallback;
        }
        return static_cast<std::uint16_t>(parsed);
    } catch (...) {
        return fallback;
    }
}

std::uint64_t arg_u64(const std::vector<std::string>& args,
                      const std::string& name,
                      std::uint64_t fallback = 0) {
    const auto value = arg_value(args, name);
    if (value.empty()) {
        return fallback;
    }
    try {
        return std::stoull(value);
    } catch (...) {
        return fallback;
    }
}

bool has_arg(const std::vector<std::string>& args, const std::string& name) {
    for (const auto& arg : args) {
        if (arg == name) {
            return true;
        }
    }
    return false;
}

void print_usage(const char* exe) {
    std::cerr
        << "Usage: " << exe
        << " --host <host> --port <port> --game <game> --slot <slot>"
        << " [--path <path>] [--password <password>] [--uuid <uuid>]"
        << " [--location <id>] [--poll-items]\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (has_arg(args, "--help")) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const auto host = arg_value(args, "--host", "127.0.0.1");
    const auto port = arg_port(args, "--port", 38281);
    const auto path = arg_value(args, "--path", "/");
    const auto game = arg_value(args, "--game", "A Link to the Past");
    const auto slot = arg_value(args, "--slot");
    if (slot.empty()) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    ArchipelagoConnectOptions options;
    options.game = game;
    options.slot_name = slot;
    options.password = arg_value(args, "--password");
    options.uuid = arg_value(args, "--uuid", "sekailink-sklmi-live-probe");
    options.tags = {"AP", "SekaiLink", "SKLMI", "Probe"};

    auto transport = std::make_unique<TcpWebSocketArchipelagoTransport>(host, port, path);
    ArchipelagoRoomClient client(std::move(transport), options);

    std::string error;
    if (!client.connect(&error)) {
        std::cerr << "connect_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto metadata = client.metadata_snapshot();
    std::cout << "connected=1"
              << " game=" << metadata.at("game")
              << " slot_name=" << metadata.at("slot_name")
              << " team=" << metadata.at("team")
              << " slot=" << metadata.at("slot");
    if (const auto seed = metadata.find("seed_name"); seed != metadata.end()) {
        std::cout << " seed_name=" << seed->second;
    }
    std::cout << "\n";

    const auto location_id = arg_u64(args, "--location");
    if (location_id != 0) {
        Event check;
        check.type = EventType::location_checked;
        check.key = std::to_string(location_id);
        check.canonical_id = location_id;
        if (!client.report_location_checked(check, &error)) {
            std::cerr << "location_failed:" << error << "\n";
            return EXIT_FAILURE;
        }
        std::cout << "location_checked=" << location_id << "\n";
    }

    if (has_arg(args, "--poll-items")) {
        const auto items = client.poll_pending_items(&error);
        std::cout << "pending_items=" << items.size() << "\n";
        for (const auto& item : items) {
            std::cout << "item=" << item.item_id
                      << " ap_item_id=" << item.ap_item_id
                      << " name=" << item.item_name
                      << "\n";
        }
    }

    return EXIT_SUCCESS;
}
