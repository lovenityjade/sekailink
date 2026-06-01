#include "sekailink_sklmi/api.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
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

#ifndef _WIN32

void close_socket(int fd) {
    if (fd >= 0) close(fd);
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

bool write_ws_text(int fd, const std::string& payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0x81));
    if (payload.size() <= 125) {
        frame.push_back(static_cast<char>(payload.size()));
    } else {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((payload.size() >> 8U) & 0xFFU));
        frame.push_back(static_cast<char>(payload.size() & 0xFFU));
    }
    frame += payload;
    return write_all(fd, frame);
}

std::optional<std::string> read_ws_text(int fd) {
    unsigned char header[2]{};
    if (recv(fd, header, 2, MSG_WAITALL) != 2) return std::nullopt;
    std::uint64_t length = header[1] & 0x7FU;
    if (length == 126) {
        unsigned char ext[2]{};
        if (recv(fd, ext, 2, MSG_WAITALL) != 2) return std::nullopt;
        length = (static_cast<std::uint64_t>(ext[0]) << 8U) | ext[1];
    }
    unsigned char mask[4]{};
    if ((header[1] & 0x80U) != 0) {
        if (recv(fd, mask, 4, MSG_WAITALL) != 4) return std::nullopt;
    }
    std::string payload(length, '\0');
    if (length > 0 && recv(fd, payload.data(), length, MSG_WAITALL) != static_cast<ssize_t>(length)) return std::nullopt;
    if ((header[1] & 0x80U) != 0) {
        for (std::size_t index = 0; index < payload.size(); ++index) {
            payload[index] = static_cast<char>(static_cast<unsigned char>(payload[index]) ^ mask[index % 4]);
        }
    }
    return payload;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

#endif

}  // namespace

int main() {
#ifndef _WIN32
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        std::cerr << "server_socket_failed\n";
        return EXIT_FAILURE;
    }
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(server, 1) != 0) {
        std::cerr << "server_bind_failed\n";
        close_socket(server);
        return EXIT_FAILURE;
    }
    socklen_t addr_len = sizeof(addr);
    getsockname(server, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    const auto port = ntohs(addr.sin_port);

    std::atomic<bool> ready{false};
    std::string data_package_packet;
    std::string connect_packet;
    std::string location_packet;
    std::thread server_thread([&]() {
        ready.store(true);
        const int client = accept(server, nullptr, nullptr);
        if (client < 0) return;
        std::string request;
        char ch = '\0';
        while (request.find("\r\n\r\n") == std::string::npos && recv(client, &ch, 1, 0) == 1) {
            request.push_back(ch);
        }
        write_all(client,
                  "HTTP/1.1 101 Switching Protocols\r\n"
                  "Upgrade: websocket\r\n"
                  "Connection: Upgrade\r\n"
                  "Sec-WebSocket-Accept: test\r\n\r\n");
        write_ws_text(client, "[{\"cmd\":\"RoomInfo\",\"seed_name\":\"WS-SEED\"}]");
        data_package_packet = read_ws_text(client).value_or("");
        write_ws_text(client,
                      "[{\"cmd\":\"DataPackage\",\"data\":{\"games\":{\"A Link to the Past\":{"
                      "\"item_name_to_id\":{\"Hookshot\":10},"
                      "\"location_name_to_id\":{\"Sanctuary\":60025}}}}}]");
        connect_packet = read_ws_text(client).value_or("");
        write_ws_text(client, "[{\"cmd\":\"Connected\",\"team\":0,\"slot\":1,\"slot_data\":{\"goal\":\"ganon\"}}]");
        write_ws_text(client, "[{\"cmd\":\"ReceivedItems\",\"index\":0,\"items\":[{\"item\":10,\"location\":60025,\"player\":1,\"flags\":0}]}]");
        location_packet = read_ws_text(client).value_or("");
        close_socket(client);
    });

    while (!ready.load()) {
        std::this_thread::yield();
    }

    ArchipelagoConnectOptions options;
    options.game = "A Link to the Past";
    options.slot_name = "Jade";
    options.uuid = "sklmi-websocket-smoke";
    options.tags = {"AP", "SekaiLink"};

    ArchipelagoRoomClient client(std::make_unique<TcpWebSocketArchipelagoTransport>("127.0.0.1", port), options);
    std::string error;
    if (!client.connect(&error)) {
        std::cerr << "client_connect_failed:" << error << "\n";
        close_socket(server);
        server_thread.join();
        return EXIT_FAILURE;
    }

    Event check;
    check.type = EventType::location_checked;
    check.canonical_id = 60025;
    if (!client.report_location_checked(check, &error)) {
        std::cerr << "location_send_failed:" << error << "\n";
        close_socket(server);
        server_thread.join();
        return EXIT_FAILURE;
    }
    const auto items = client.poll_pending_items(&error);
    close_socket(server);
    server_thread.join();

    if (!contains(data_package_packet, "\"cmd\":\"GetDataPackage\"") ||
        !contains(data_package_packet, "\"A Link to the Past\"")) {
        std::cerr << "data_package_request_missing\n";
        return EXIT_FAILURE;
    }
    if (!contains(connect_packet, "\"cmd\":\"Connect\"") ||
        !contains(connect_packet, "\"game\":\"A Link to the Past\"") ||
        !contains(connect_packet, "\"name\":\"Jade\"")) {
        std::cerr << "connect_packet_missing\n";
        return EXIT_FAILURE;
    }
    if (!contains(location_packet, "\"cmd\":\"LocationChecks\"") ||
        !contains(location_packet, "\"locations\":[60025]")) {
        std::cerr << "location_packet_missing\n";
        return EXIT_FAILURE;
    }
    if (items.size() != 1 || items[0].ap_item_id != 10 || items[0].ap_location_id != 60025 ||
        items[0].ap_player_id != 1 || items[0].value_u64 != 10 ||
        items[0].item_name != "Hookshot") {
        std::cerr << "received_items_missing\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_archipelago_websocket_smoke_ok\n";
    return EXIT_SUCCESS;
#else
    std::cout << "sklmi_archipelago_websocket_smoke_skipped_windows\n";
    return EXIT_SUCCESS;
#endif
}
