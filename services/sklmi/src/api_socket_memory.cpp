#include "api_internal.hpp"

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

#include <cstring>
#include <regex>
#include <sstream>

namespace sekailink::sklmi {

namespace {

bool ensure_socket_runtime() {
#ifdef _WIN32
    static bool initialized = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return initialized;
#else
    return true;
#endif
}

void close_socket(socket_handle handle) {
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

std::string read_socket_line(socket_handle handle) {
    std::string line;
    char ch = '\0';
    while (true) {
        const auto received = recv(handle, &ch, 1, 0);
        if (received <= 0) return {};
        if (ch == '\n') break;
        if (ch != '\r') line.push_back(ch);
    }
    return line;
}

bool write_socket_all(socket_handle handle, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto written = send(handle,
                                  payload.data() + sent,
                                  static_cast<int>(payload.size() - sent),
                                  0);
        if (written <= 0) return false;
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

std::optional<std::string> extract_response_value_string(const std::string& text, const std::string& type) {
    const std::regex pattern("\"type\"\\s*:\\s*\"" + type + "\".*?\"value\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(text, match, pattern) && match.size() > 1) return match[1].str();
    return std::nullopt;
}

std::vector<std::string> extract_domains_array_objects(const std::string& text) {
    const auto pos = text.find("\"DOMAINS_RESPONSE\"");
    if (pos == std::string::npos) return {};
    return detail::extract_object_blocks(text.substr(pos), "value");
}

}  // namespace

RuntimeSocketMemoryProvider::RuntimeSocketMemoryProvider(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

RuntimeSocketMemoryProvider::~RuntimeSocketMemoryProvider() {
    disconnect();
}

bool RuntimeSocketMemoryProvider::connect(std::string* error) {
    if (connected()) return true;
    if (!ensure_socket_runtime()) {
        if (error) *error = "socket_runtime_init_failed";
        return false;
    }
    socket_handle handle = socket(AF_INET, SOCK_STREAM, 0);
    if (handle == kInvalidSocket) {
        if (error) *error = "socket_create_failed";
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        close_socket(handle);
        if (error) *error = "invalid_runtime_address";
        return false;
    }
    if (::connect(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(handle);
        if (error) *error = "runtime_connect_failed";
        return false;
    }
    socket_ = static_cast<std::intptr_t>(handle);
    if (!write_socket_all(handle, "VERSION\n")) {
        disconnect();
        if (error) *error = "runtime_version_write_failed";
        return false;
    }
    const auto version_line = read_socket_line(handle);
    if (version_line.empty()) {
        disconnect();
        if (error) *error = "runtime_version_read_failed";
        return false;
    }
    const auto version = detail::parse_u64(version_line);
    if (!version.has_value()) {
        disconnect();
        if (error) *error = "runtime_version_invalid";
        return false;
    }
    protocol_version_ = static_cast<int>(*version);
    return refresh_remote_metadata(error);
}

void RuntimeSocketMemoryProvider::disconnect() {
    if (!connected()) return;
    close_socket(static_cast<socket_handle>(socket_));
    socket_ = -1;
    protocol_version_.reset();
    system_name_.clear();
    descriptors_.clear();
}

bool RuntimeSocketMemoryProvider::connected() const {
    return socket_ >= 0;
}

std::optional<int> RuntimeSocketMemoryProvider::protocol_version() const {
    return protocol_version_;
}

const std::string& RuntimeSocketMemoryProvider::system_name() const {
    return system_name_;
}

std::vector<MemoryDomainDescriptor> RuntimeSocketMemoryProvider::domains() const {
    std::vector<MemoryDomainDescriptor> out;
    out.reserve(descriptors_.size());
    for (const auto& [_, descriptor] : descriptors_) out.push_back(descriptor);
    return out;
}

bool RuntimeSocketMemoryProvider::has_domain(std::string_view domain_id) const {
    return descriptors_.contains(std::string(domain_id));
}

bool RuntimeSocketMemoryProvider::is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const {
    const auto it = descriptors_.find(std::string(domain_id));
    if (it == descriptors_.end()) return false;
    return address <= it->second.size_bytes && size <= it->second.size_bytes - static_cast<std::size_t>(address);
}

bool RuntimeSocketMemoryProvider::read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const {
    if (!connected() || buffer == nullptr || !is_address_valid(domain_id, address, size)) return false;
    std::ostringstream request;
    request << "[{\"type\":\"READ\",\"domain\":\"" << detail::escape_json(std::string(domain_id))
            << "\",\"address\":" << address
            << ",\"size\":" << size
            << "}]\n";
    if (!write_socket_all(static_cast<socket_handle>(socket_), request.str())) return false;
    const auto response = read_socket_line(static_cast<socket_handle>(socket_));
    const auto encoded = extract_response_value_string(response, "READ_RESPONSE");
    if (!encoded.has_value()) return false;
    const auto decoded = detail::base64_decode_bytes(*encoded);
    if (!decoded.has_value() || decoded->size() != size) return false;
    std::memcpy(buffer, decoded->data(), size);
    return true;
}

bool RuntimeSocketMemoryProvider::write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) {
    const auto it = descriptors_.find(std::string(domain_id));
    if (!connected() || buffer == nullptr || it == descriptors_.end() || !it->second.writable || !is_address_valid(domain_id, address, size)) return false;
    const auto encoded = detail::base64_encode_bytes(buffer, size);
    std::ostringstream request;
    request << "[{\"type\":\"WRITE\",\"domain\":\"" << detail::escape_json(std::string(domain_id))
            << "\",\"address\":" << address
            << ",\"value\":\"" << encoded
            << "\"}]\n";
    if (!write_socket_all(static_cast<socket_handle>(socket_), request.str())) return false;
    const auto response = read_socket_line(static_cast<socket_handle>(socket_));
    return response.find("\"WRITE_RESPONSE\"") != std::string::npos;
}

bool RuntimeSocketMemoryProvider::refresh_remote_metadata(std::string* error) {
    if (!connected()) {
        if (error) *error = "runtime_not_connected";
        return false;
    }
    if (!write_socket_all(static_cast<socket_handle>(socket_), "[{\"type\":\"SYSTEM\"},{\"type\":\"DOMAINS\"}]\n")) {
        if (error) *error = "runtime_metadata_write_failed";
        return false;
    }
    const auto response = read_socket_line(static_cast<socket_handle>(socket_));
    const auto system = extract_response_value_string(response, "SYSTEM_RESPONSE");
    if (!system.has_value()) {
        if (error) *error = "runtime_system_missing";
        return false;
    }
    system_name_ = *system;
    descriptors_.clear();
    for (const auto& block : extract_domains_array_objects(response)) {
        MemoryDomainDescriptor descriptor;
        descriptor.id = detail::extract_string_field(block, "name").value_or("");
        descriptor.display_name = descriptor.id;
        descriptor.size_bytes = static_cast<std::size_t>(detail::extract_uint_field(block, "size").value_or(0));
        descriptor.writable = detail::extract_bool_field(block, "writable").value_or(false);
        const auto endianness = detail::extract_string_field(block, "endianness").value_or("little");
        descriptor.endianness = endianness == "big" ? Endianness::big : Endianness::little;
        if (!descriptor.id.empty()) descriptors_[descriptor.id] = descriptor;
    }
    if (descriptors_.empty()) {
        if (error) *error = "runtime_domains_missing";
        return false;
    }
    return true;
}

}  // namespace sekailink::sklmi
