#include "api_internal.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <random>
#include <sstream>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_handle = SOCKET;
constexpr socket_handle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using socket_handle = int;
constexpr socket_handle kInvalidSocket = -1;
#endif

namespace sekailink::sklmi {
namespace {

bool EnsureSocketRuntime() {
#if defined(_WIN32)
    static bool initialized = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return initialized;
#else
    return true;
#endif
}

void CloseSocketFd(socket_handle fd) {
    if (fd == kInvalidSocket) return;
#if defined(_WIN32)
    closesocket(fd);
#else
    close(fd);
#endif
}

int SocketSendFlags() {
#if defined(MSG_NOSIGNAL)
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

std::string MakeWebSocketKey() {
    std::array<std::byte, 16> bytes{};
    std::random_device rng;
    for (auto& byte : bytes) {
        byte = static_cast<std::byte>(rng() & 0xFFU);
    }
    return detail::base64_encode_bytes(bytes.data(), bytes.size());
}

bool IsTimeoutError() {
#if defined(_WIN32)
    const auto err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAETIMEDOUT;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT;
#endif
}

bool SocketHasReadableData(socket_handle fd) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    timeval timeout{};
    const auto result = ::select(
#if defined(_WIN32)
        0,
#else
        fd + 1,
#endif
        &read_set,
        nullptr,
        nullptr,
        &timeout);
    return result > 0 && FD_ISSET(fd, &read_set);
}

}  // namespace

TcpWebSocketArchipelagoTransport::TcpWebSocketArchipelagoTransport(std::string host, std::uint16_t port, std::string path)
    : host_(std::move(host)), port_(port), path_(std::move(path)) {
    if (path_.empty()) path_ = "/";
}

TcpWebSocketArchipelagoTransport::~TcpWebSocketArchipelagoTransport() {
    disconnect();
}

bool TcpWebSocketArchipelagoTransport::connect(std::string* error) {
    if (connected()) return true;
    handshake_complete_ = false;
    if (!EnsureSocketRuntime()) {
        if (error) *error = "archipelago_websocket_socket_runtime_failed";
        return false;
    }
    if (host_.empty() || port_ == 0) {
        if (error) *error = "archipelago_websocket_endpoint_missing";
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    const auto port_text = std::to_string(port_);
    const int resolve_status = ::getaddrinfo(host_.c_str(), port_text.c_str(), &hints, &results);
    if (resolve_status != 0 || results == nullptr) {
        if (error) *error = "archipelago_websocket_name_resolution_failed";
        return false;
    }

    socket_handle fd = kInvalidSocket;
    for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
        fd = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd == kInvalidSocket) continue;
        if (::connect(fd, current->ai_addr, static_cast<socklen_t>(current->ai_addrlen)) == 0) break;
        CloseSocketFd(fd);
        fd = kInvalidSocket;
    }
    ::freeaddrinfo(results);

    if (fd == kInvalidSocket) {
        if (error) *error = "archipelago_websocket_connect_failed";
        return false;
    }

#if defined(_WIN32)
    DWORD timeout = 50;
#else
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
#endif
    ::setsockopt(fd,
                 SOL_SOCKET,
                 SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout),
                 sizeof(timeout));

    socket_ = static_cast<std::intptr_t>(fd);
    const auto key = MakeWebSocketKey();
    std::ostringstream request;
    request << "GET " << path_ << " HTTP/1.1\r\n"
            << "Host: " << host_ << ":" << port_ << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << key << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n"
            << "\r\n";
    const auto text = request.str();
    if (!write_all(reinterpret_cast<const std::byte*>(text.data()), text.size(), error)) {
        disconnect();
        return false;
    }

    std::string response;
    char buffer[1]{};
    while (response.find("\r\n\r\n") == std::string::npos && response.size() < 8192) {
        const auto received = ::recv(static_cast<socket_handle>(socket_), buffer, 1, 0);
        if (received <= 0) {
            if (error) *error = "archipelago_websocket_handshake_recv_failed";
            disconnect();
            return false;
        }
        response.push_back(buffer[0]);
    }
    if (response.find(" 101 ") == std::string::npos && response.find(" 101\r") == std::string::npos) {
        if (error) *error = "archipelago_websocket_upgrade_rejected";
        disconnect();
        return false;
    }
    handshake_complete_ = true;
    return true;
}

void TcpWebSocketArchipelagoTransport::disconnect() {
    if (socket_ >= 0 && handshake_complete_) {
        constexpr std::array<std::byte, 4> mask{
            std::byte{0x53}, std::byte{0x4B}, std::byte{0x4C}, std::byte{0x33}};
        constexpr std::array<std::byte, 2> status{
            std::byte{0x03}, std::byte{0xE8}};
        std::array<std::byte, 8> close_frame{
            std::byte{0x88},
            std::byte{0x82},
            mask[0],
            mask[1],
            mask[2],
            mask[3],
            static_cast<std::byte>(std::to_integer<unsigned char>(status[0]) ^
                                   std::to_integer<unsigned char>(mask[0])),
            static_cast<std::byte>(std::to_integer<unsigned char>(status[1]) ^
                                   std::to_integer<unsigned char>(mask[1]))};
        write_all(close_frame.data(), close_frame.size(), nullptr);
    }
    CloseSocketFd(static_cast<socket_handle>(socket_));
    socket_ = -1;
    handshake_complete_ = false;
}

bool TcpWebSocketArchipelagoTransport::connected() const {
    return socket_ >= 0 && handshake_complete_;
}

bool TcpWebSocketArchipelagoTransport::send_text(std::string_view payload, std::string* error) {
    if (!ensure_connected(error)) return false;
    std::vector<std::byte> frame;
    frame.push_back(std::byte{0x81});
    const auto size = payload.size();
    if (size <= 125) {
        frame.push_back(static_cast<std::byte>(0x80U | size));
    } else if (size <= 0xFFFF) {
        frame.push_back(std::byte{0xFE});
        frame.push_back(static_cast<std::byte>((size >> 8U) & 0xFFU));
        frame.push_back(static_cast<std::byte>(size & 0xFFU));
    } else {
        frame.push_back(std::byte{0xFF});
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<std::byte>((static_cast<std::uint64_t>(size) >> shift) & 0xFFU));
        }
    }

    std::array<std::byte, 4> mask{};
    std::random_device rng;
    for (auto& byte : mask) byte = static_cast<std::byte>(rng() & 0xFFU);
    frame.insert(frame.end(), mask.begin(), mask.end());
    for (std::size_t index = 0; index < payload.size(); ++index) {
        const auto raw = static_cast<unsigned char>(payload[index]);
        const auto key = std::to_integer<unsigned char>(mask[index % mask.size()]);
        frame.push_back(static_cast<std::byte>(raw ^ key));
    }
    return write_all(frame.data(), frame.size(), error);
}

std::optional<std::string> TcpWebSocketArchipelagoTransport::receive_text(std::string* error) {
    if (!ensure_connected(error)) return std::nullopt;
    if (!SocketHasReadableData(static_cast<socket_handle>(socket_))) {
        if (error) error->clear();
        return std::nullopt;
    }
    while (true) {
        const auto header = read_exact(2, error);
        if (!header.has_value()) return std::nullopt;
        const auto first = std::to_integer<unsigned char>((*header)[0]);
        const auto second = std::to_integer<unsigned char>((*header)[1]);
        const auto opcode = first & 0x0FU;
        const bool masked = (second & 0x80U) != 0;
        std::uint64_t length = second & 0x7FU;
        if (length == 126) {
            const auto ext = read_exact(2, error);
            if (!ext.has_value()) return std::nullopt;
            length = (static_cast<std::uint64_t>(std::to_integer<unsigned char>((*ext)[0])) << 8U) |
                     std::to_integer<unsigned char>((*ext)[1]);
        } else if (length == 127) {
            const auto ext = read_exact(8, error);
            if (!ext.has_value()) return std::nullopt;
            length = 0;
            for (const auto byte : *ext) {
                length = (length << 8U) | std::to_integer<unsigned char>(byte);
            }
        }
        std::array<std::byte, 4> mask{};
        if (masked) {
            const auto mask_data = read_exact(4, error);
            if (!mask_data.has_value()) return std::nullopt;
            std::copy(mask_data->begin(), mask_data->end(), mask.begin());
        }
        const auto payload = read_exact(static_cast<std::size_t>(length), error);
        if (!payload.has_value()) return std::nullopt;
        std::string text;
        text.resize(payload->size());
        for (std::size_t index = 0; index < payload->size(); ++index) {
            auto value = std::to_integer<unsigned char>((*payload)[index]);
            if (masked) value ^= std::to_integer<unsigned char>(mask[index % mask.size()]);
            text[index] = static_cast<char>(value);
        }
        if (opcode == 0x1) return text;
        if (opcode == 0x8) {
            disconnect();
            if (error) *error = "archipelago_websocket_closed";
            return std::nullopt;
        }
        if (opcode == 0x9) {
            std::vector<std::byte> pong;
            pong.push_back(std::byte{0x8A});
            pong.push_back(static_cast<std::byte>(0x80U | text.size()));
            std::array<std::byte, 4> pong_mask{std::byte{0x53}, std::byte{0x4B}, std::byte{0x4C}, std::byte{0x33}};
            pong.insert(pong.end(), pong_mask.begin(), pong_mask.end());
            for (std::size_t index = 0; index < text.size(); ++index) {
                pong.push_back(static_cast<std::byte>(static_cast<unsigned char>(text[index]) ^
                                                      std::to_integer<unsigned char>(pong_mask[index % pong_mask.size()])));
            }
            write_all(pong.data(), pong.size(), nullptr);
            continue;
        }
    }
}

bool TcpWebSocketArchipelagoTransport::ensure_connected(std::string* error) {
    return connected() || connect(error);
}

bool TcpWebSocketArchipelagoTransport::write_all(const std::byte* data, std::size_t size, std::string* error) {
    std::size_t sent = 0;
    while (sent < size) {
        const auto written = ::send(static_cast<socket_handle>(socket_),
                                    reinterpret_cast<const char*>(data + sent),
                                    static_cast<int>(size - sent),
                                    SocketSendFlags());
        if (written <= 0) {
            if (error) *error = "archipelago_websocket_send_failed";
            return false;
        }
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

std::optional<std::vector<std::byte>> TcpWebSocketArchipelagoTransport::read_exact(std::size_t size, std::string* error) {
    std::vector<std::byte> out(size);
    std::size_t read = 0;
    while (read < size) {
        const auto received = ::recv(static_cast<socket_handle>(socket_),
                                     reinterpret_cast<char*>(out.data() + read),
                                     static_cast<int>(size - read),
                                     0);
        if (received <= 0) {
            if (IsTimeoutError()) return std::nullopt;
            if (error) *error = "archipelago_websocket_recv_failed";
            return std::nullopt;
        }
        read += static_cast<std::size_t>(received);
    }
    return out;
}

}  // namespace sekailink::sklmi
