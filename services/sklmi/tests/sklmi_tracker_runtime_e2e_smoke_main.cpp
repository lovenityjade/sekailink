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

void CloseSocket(int fd) {
    if (fd >= 0) close(fd);
}

std::string ReadLine(int fd) {
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

bool WriteAll(int fd, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto written = send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (written <= 0) return false;
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

std::optional<std::uint64_t> ExtractUint(const std::string& text, const std::string& key) {
    const auto begin = text.find("\"" + key + "\":");
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + key.size() + 3;
    auto end = text.find_first_of(",}]", start);
    if (end == std::string::npos) end = text.size();
    return static_cast<std::uint64_t>(std::stoull(text.substr(start, end - start)));
}

std::optional<std::string> ExtractString(const std::string& text, const std::string& key) {
    const auto begin = text.find("\"" + key + "\":\"");
    if (begin == std::string::npos) return std::nullopt;
    const auto start = begin + key.size() + 4;
    const auto end = text.find('"', start);
    if (end == std::string::npos) return std::nullopt;
    return text.substr(start, end - start);
}

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64EncodeBytes(const std::vector<std::byte>& data) {
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

std::optional<std::vector<std::byte>> Base64DecodeBytes(const std::string& input) {
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

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

std::string Slurp(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::filesystem::path MakeTempDir() {
    const auto dir = std::filesystem::temp_directory_path() / ("sklmi-tracker-e2e-" + std::to_string(::getpid()));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir);
    return dir;
}

#endif

}  // namespace

#ifndef _WIN32
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: sklmi_tracker_runtime_e2e_smoke <runtime_binary>\n";
        return EXIT_FAILURE;
    }

    const auto runtime_binary = std::filesystem::path(argv[1]);
    if (!std::filesystem::exists(runtime_binary)) {
        std::cerr << "runtime_binary_missing\n";
        return EXIT_FAILURE;
    }

    const auto temp_dir = MakeTempDir();
    const auto socket_path = std::filesystem::temp_directory_path() / ("sklmi-tracker-e2e-" + std::to_string(::getpid()) + ".sock");
    const auto manifest_path = temp_dir / "bridge.json";
    const auto room_state_path = temp_dir / "room.state";
    const auto runtime_state_root = temp_dir / "runtime-state";
    const auto trace_log_path = temp_dir / "trace.jsonl";
    const auto tracker_snapshot_path = temp_dir / "tracker.snapshot.json";
    const auto tracker_command_log_path = temp_dir / "tracker.commands.jsonl";
    const auto tracker_pack_path = temp_dir / "bundle";
    std::filesystem::create_directories(runtime_state_root);
    std::filesystem::create_directories(tracker_pack_path);

    WriteText(tracker_pack_path / "manifest.json", R"JSON({
  "linkedworld_id": "demo",
  "display_name": "Demo Tracker",
  "default_tab_id": "items",
  "default_map_id": "world",
  "tabs": [
    {"id": "items", "label": "Items"},
    {"id": "world-tab", "label": "World", "map_id": "world"}
  ],
  "maps": [
    {"id": "world", "label": "World"}
  ]
})JSON");
    WriteText(tracker_pack_path / "surface.complete.json", R"JSON({
  "presentation": {
    "tab_order": ["items", "world-tab"]
  }
})JSON");
    WriteText(tracker_pack_path / "item-slots.complete.json", R"JSON({
  "slots": [
    {
      "slot_id": "sword",
      "label": "Sword",
      "group_id": "combat",
      "behavior": "progressive",
      "max_stage": 4,
      "items": [{"item_id": 55001, "item_name": "Test Item", "event_key": "item.test_item"}]
    }
  ]
})JSON");
    WriteText(tracker_pack_path / "location-groups.complete.json", R"JSON({
  "groups": [
    {
      "group_id": "starter",
      "label": "Starter Area",
      "preferred_tab": "world-tab",
      "locations": [
        {"location_id": 44001, "location_name": "Test Location", "event_key": "44001"}
      ]
    }
  ]
})JSON");
    WriteText(tracker_pack_path / "map-pin-metadata.json", R"JSON({
  "pin_layers": [
    {"group_id": "starter", "preferred_tab": "world-tab", "map_id": "world", "pin_count": 1, "pin_kind": "check"}
  ]
})JSON");
    WriteText(tracker_pack_path / "item-icon-metadata.json", R"JSON({
  "icon_groups": [
    {"group_id": "combat", "default_palette": "red-gold"}
  ],
  "slot_icon_bindings": [
    {"slot_id": "sword", "icon_key": "item.sword", "render_hint": "progressive-stage"}
  ]
})JSON");
    WriteText(tracker_pack_path / "poptracker-adapted" / "maps" / "maps.json", R"JSON([
  {"name": "overworld", "img": "maps/overworld.png"}
])JSON");
    WriteText(tracker_pack_path / "poptracker-adapted" / "locations" / "world.json", R"JSON([
  {
    "name": "Starter Area",
    "children": [
      {
        "name": "Test Location",
        "map_locations": [{"map": "overworld", "x": 48, "y": 64, "size": 12}]
      }
    ]
  }
])JSON");
    WriteText(tracker_pack_path / "poptracker-adapted" / "images" / "items" / "Sword.png", "png");

    WriteText(manifest_path, R"JSON({
  "id": "demo",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "demo-tracker-runtime",
    "driver_instance_id": "tracker-runtime-1",
    "core_profile": "demo_v1",
    "module": "sekaiemu",
    "state_file": "demo.bridge.state",
    "poll_interval_ms": 1,
    "checks": [
      {
        "domain_id": "system_ram",
        "address": 4,
        "size": 1,
        "compare": "equals",
        "operand_u64": 90,
        "event_type": "location_checked",
        "location_id": 44001,
        "location_name": "Test Location"
      }
    ],
    "actions": [
      {
        "domain_id": "system_ram",
        "address": 8,
        "size": 1,
        "value_u64": 33,
        "item_id": 55001,
        "item_name": "Test Item",
        "event_key": "item.test_item",
        "room_controlled": true
      }
    ]
  }
})JSON");

    WriteText(room_state_path,
              "meta|connected|1\n"
              "meta|slot_id|Demo Slot\n"
              "meta|seed_id|Demo Seed\n"
              "meta|tracker_pack|demo-pack\n"
              "meta|tracker_variant|Demo Variant\n"
              "meta|slot_data|{\"goal\":\"demo\"}\n"
              "pending|item-1|item.test_item|Test Item|33|55001|Test Item\n");
    WriteText(tracker_command_log_path,
              "{\"cmd\":\"tracker.click_item\",\"code\":\"sword\",\"button\":\"left\"}\n");

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
            CloseSocket(srv);
            return;
        }
        if (listen(srv, 1) < 0) {
            CloseSocket(srv);
            return;
        }
        ready.store(true);
        const int client = accept(srv, nullptr, nullptr);
        if (client < 0) {
            CloseSocket(srv);
            return;
        }
        while (!stop.load()) {
            const auto line = ReadLine(client);
            if (line.empty()) break;
            if (line == "VERSION") {
                if (!WriteAll(client, "1\n")) break;
                continue;
            }
            if (line.find("\"SYSTEM\"") != std::string::npos && line.find("\"DOMAINS\"") != std::string::npos) {
                if (!WriteAll(client,
                              "[{\"type\":\"SYSTEM_RESPONSE\",\"value\":\"SNES\"},"
                              "{\"type\":\"DOMAINS_RESPONSE\",\"value\":[{\"name\":\"system_ram\",\"size\":16,\"writable\":true,\"endianness\":\"little\"}]}]\n")) {
                    break;
                }
                continue;
            }
            if (line.find("\"READ\"") != std::string::npos) {
                const auto address = ExtractUint(line, "address").value_or(0);
                const auto size = ExtractUint(line, "size").value_or(0);
                std::vector<std::byte> out(size, std::byte{0});
                for (std::size_t i = 0; i < size && address + i < memory.size(); ++i) out[i] = memory[address + i];
                const auto encoded = Base64EncodeBytes(out);
                if (!WriteAll(client, "[{\"type\":\"READ_RESPONSE\",\"value\":\"" + encoded + "\"}]\n")) break;
                continue;
            }
            if (line.find("\"WRITE\"") != std::string::npos) {
                const auto address = ExtractUint(line, "address").value_or(0);
                const auto encoded = ExtractString(line, "value").value_or("");
                const auto decoded = Base64DecodeBytes(encoded);
                if (decoded.has_value()) {
                    for (std::size_t i = 0; i < decoded->size() && address + i < memory.size(); ++i) {
                        memory[address + i] = (*decoded)[i];
                    }
                }
                if (!WriteAll(client, "[{\"type\":\"WRITE_RESPONSE\"}]\n")) break;
                continue;
            }
        }
        CloseSocket(client);
        CloseSocket(srv);
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
              "--tracker-pack", tracker_pack_path.c_str(),
              "--tracker-variant", "Demo Variant",
              "--tracker-snapshot", tracker_snapshot_path.c_str(),
              "--tracker-command-log", tracker_command_log_path.c_str(),
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
        std::cerr << "tracker_runtime_failed\n";
        return EXIT_FAILURE;
    }
    if (memory[8] != std::byte{0x21}) {
        std::cerr << "tracker_runtime_no_injection\n";
        return EXIT_FAILURE;
    }

    const auto trace = Slurp(trace_log_path);
    if (!Contains(trace, "\"event\":\"tracker_initial_snapshot\"")) {
        std::cerr << "tracker_runtime_initial_snapshot_missing\n";
        return EXIT_FAILURE;
    }

    const auto snapshot = Slurp(tracker_snapshot_path);
    if (!Contains(snapshot, "\"schema\":\"sekailink.tracker.snapshot.v1\"") ||
        !Contains(snapshot, "\"game\":\"Demo Tracker\"") ||
        !Contains(snapshot, "\"connected\":true") ||
        !Contains(snapshot, "\"ap_connected\":true") ||
        !Contains(snapshot, "\"slot\":\"Demo Slot\"") ||
        !Contains(snapshot, "\"slot_id\":\"Demo Slot\"") ||
        !Contains(snapshot, "\"seed\":\"Demo Seed\"") ||
        !Contains(snapshot, "\"seed_id\":\"Demo Seed\"") ||
        !Contains(snapshot, "\"active_tab\":\"items\"") ||
        !Contains(snapshot, "\"active_map\":\"world\"") ||
        !Contains(snapshot, "\"slot_data\":{\"goal\":\"demo\"}") ||
        !Contains(snapshot, "\"pack_maps\":[{\"name\":\"overworld\",\"image\":\"maps/overworld.png\"}]") ||
        !Contains(snapshot, "\"item_icon_groups\":[{\"group_id\":\"combat\",\"default_palette\":\"red-gold\"}]") ||
        !Contains(snapshot, "\"id\":\"sword\"") ||
        !Contains(snapshot, "\"icon_key\":\"item.sword\"") ||
        !Contains(snapshot, "\"asset_candidates\":[\"poptracker-adapted/images/items/Sword.png\"]") ||
        !Contains(snapshot, "\"stage\":2") ||
        !Contains(snapshot, "\"id\":\"starter\"") ||
        !Contains(snapshot, "\"color\":\"black\"") ||
        !Contains(snapshot, "\"pins_detailed\":[") ||
        !Contains(snapshot, "\"location_id\":44001") ||
        !Contains(snapshot, "\"map_asset\":\"poptracker-adapted/maps/overworld.png\"") ||
        !Contains(snapshot, "\"checked\":1") ||
        !Contains(snapshot, "\"total\":1") ||
        !Contains(snapshot, "\"status\":{\"ap_connected\":true,") ||
        !Contains(snapshot, "\"pack\":\"demo-pack\"") ||
        !Contains(snapshot, "\"variant\":\"Demo Variant\"")) {
        std::cerr << "tracker_runtime_snapshot_invalid\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_tracker_runtime_e2e_smoke_ok\n";
    return EXIT_SUCCESS;
}
#else
int main() {
    return 0;
}
#endif
