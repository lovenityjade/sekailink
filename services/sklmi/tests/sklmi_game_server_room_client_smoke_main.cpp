#include "sekailink_sklmi/api.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace sekailink::sklmi;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

#ifndef _WIN32

void close_socket_fd(int fd) {
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

#endif

}  // namespace

int main() {
#ifdef _WIN32
    return 0;
#else
    try {
        std::atomic<bool> control_ready{false};
        std::atomic<bool> runtime_ready{false};
        std::string issued_ticket_request;
        std::string runtime_event_request;
        std::string pending_request;
        std::string ack_request;

        const int control_srv = socket(AF_INET, SOCK_STREAM, 0);
        require(control_srv >= 0, "control_server_socket_failed");
        int reuse = 1;
        setsockopt(control_srv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in control_addr{};
        control_addr.sin_family = AF_INET;
        control_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        control_addr.sin_port = htons(0);
        require(bind(control_srv, reinterpret_cast<sockaddr*>(&control_addr), sizeof(control_addr)) == 0, "control_bind_failed");
        require(listen(control_srv, 4) == 0, "control_listen_failed");
        socklen_t control_len = sizeof(control_addr);
        require(getsockname(control_srv, reinterpret_cast<sockaddr*>(&control_addr), &control_len) == 0, "control_getsockname_failed");
        const auto control_port = ntohs(control_addr.sin_port);

        const int runtime_srv = socket(AF_INET, SOCK_STREAM, 0);
        require(runtime_srv >= 0, "runtime_server_socket_failed");
        setsockopt(runtime_srv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in runtime_addr{};
        runtime_addr.sin_family = AF_INET;
        runtime_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        runtime_addr.sin_port = htons(0);
        require(bind(runtime_srv, reinterpret_cast<sockaddr*>(&runtime_addr), sizeof(runtime_addr)) == 0, "runtime_bind_failed");
        require(listen(runtime_srv, 8) == 0, "runtime_listen_failed");
        socklen_t runtime_len = sizeof(runtime_addr);
        require(getsockname(runtime_srv, reinterpret_cast<sockaddr*>(&runtime_addr), &runtime_len) == 0, "runtime_getsockname_failed");
        const auto runtime_port = ntohs(runtime_addr.sin_port);

        std::thread control_server([&]() {
            control_ready.store(true);
            const int client = accept(control_srv, nullptr, nullptr);
            if (client < 0) return;
            issued_ticket_request = read_line(client);
            write_all(client, "{\"ok\":true,\"ticket\":{\"session_token\":\"runtime-ticket-1\"}}\n");
            close_socket_fd(client);
            close_socket_fd(control_srv);
        });

        std::thread runtime_server([&]() {
            runtime_ready.store(true);
            for (int index = 0; index < 3; ++index) {
                const int client = accept(runtime_srv, nullptr, nullptr);
                if (client < 0) return;
                const auto line = read_line(client);
                if (index == 0) {
                    runtime_event_request = line;
                    write_all(client, "{\"ok\":true,\"accepted\":true,\"duplicate\":false,\"reason\":\"accepted\"}\n");
                } else if (index == 1) {
                    pending_request = line;
                    write_all(client, "{\"ok\":true,\"pending_items\":[{\"delivery_id\":7,\"item_id\":55001,\"tracker_semantic_id\":\"item.sound_stone\",\"mapped_value\":\"Sound Stone\"}]}\n");
                } else {
                    ack_request = line;
                    write_all(client, "{\"ok\":true}\n");
                }
                close_socket_fd(client);
            }
            close_socket_fd(runtime_srv);
        });

        while (!control_ready.load() || !runtime_ready.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        GameServerRoomClient client(
            "localhost",
            control_port,
            runtime_port,
            "earthbound-session",
            1,
            "core",
            "core-secret",
            "runtime-secret",
            "driver-eb-1",
            "earthbound",
            "snes_v1");
        std::string error;
        require(client.connect(&error), error);
        require(client.connected(), "not_connected");
        require(client.report_location_checked(
                    Event{EventType::location_checked, "44001", "Test Location", "driver-eb-1", "earthbound", "snes_v1", 44001},
                    &error),
                error);
        const auto items = client.poll_pending_items(&error);
        require(error.empty(), error);
        require(items.size() == 1, "pending_items_size");
        require(items[0].item_id == "delivery:7", "delivery_id_mapping");
        require(items[0].ap_item_id == 55001, "ap_item_id_mapping");
        require(items[0].item_name.empty(), "item_name_expected_empty");
        require(items[0].event_key == "item.sound_stone", "event_key_mapping");
        require(items[0].mapped_value == "Sound Stone", "mapped_value_mapping");
        require(client.acknowledge_item(items[0].item_id, &error), error);

        control_server.join();
        runtime_server.join();

        require(issued_ticket_request.find("\"cmd\":\"issue_ticket\"") != std::string::npos, "missing_issue_ticket");
        require(issued_ticket_request.find("\"channel\":\"core\"") != std::string::npos, "missing_control_channel");
        require(issued_ticket_request.find("\"driver_instance_id\":\"driver-eb-1\"") != std::string::npos, "missing_driver_instance");
        require(runtime_event_request.find("\"cmd\":\"runtime_event\"") != std::string::npos, "missing_runtime_event");
        require(runtime_event_request.find("\"canonical_id\":44001") != std::string::npos, "missing_canonical_id");
        require(runtime_event_request.find("\"session_token\":\"runtime-ticket-1\"") != std::string::npos, "missing_runtime_ticket");
        require(runtime_event_request.find("\"channel\":\"runtime\"") != std::string::npos, "missing_runtime_channel");
        require(pending_request.find("\"cmd\":\"pending_items\"") != std::string::npos, "missing_pending_items");
        require(ack_request.find("\"cmd\":\"acknowledge_delivery\"") != std::string::npos, "missing_ack");
        require(ack_request.find("\"delivery_id\":7") != std::string::npos, "missing_delivery_id");

        std::cout << "sklmi_game_server_room_client_smoke_ok\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "sklmi_game_server_room_client_smoke failed: " << exception.what() << "\n";
        return EXIT_FAILURE;
    }
#endif
}
