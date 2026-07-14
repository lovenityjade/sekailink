#include "sekailink_sklmi/api.hpp"

#include <utility>

#ifndef _WIN32

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <charconv>
#include <cstddef>
#include <cstring>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <vector>

namespace sekailink::sklmi {

namespace {

void close_socket_fd(int fd) {
    if (fd >= 0) {
        close(fd);
    }
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

std::optional<std::uint64_t> parse_u64(std::string_view value) {
    if (value.empty()) return std::nullopt;
    std::uint64_t parsed = 0;
    auto begin = value.data();
    auto end = value.data() + value.size();
    const int base = (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) ? 16 : 10;
    if (base == 16) begin += 2;
    const auto result = std::from_chars(begin, end, parsed, base);
    if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
    return parsed;
}

std::string escape_json(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::optional<std::string> extract_response_value_string(const std::string& text, const std::string& type) {
    const std::regex pattern("\"type\"\\s*:\\s*\"" + type + "\".*?\"value\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(text, match, pattern) && match.size() > 1) return match[1].str();
    return std::nullopt;
}

std::vector<std::string> extract_object_blocks(const std::string& text, const std::string& key) {
    std::vector<std::string> blocks;
    const auto key_pos = text.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return blocks;
    const auto open = text.find('[', key_pos);
    if (open == std::string::npos) return blocks;
    int depth = 0;
    std::size_t block_start = std::string::npos;
    for (std::size_t index = open; index < text.size(); ++index) {
        const auto ch = text[index];
        if (ch == '{') {
            if (depth == 0) block_start = index;
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && block_start != std::string::npos) {
                blocks.push_back(text.substr(block_start, index - block_start + 1));
                block_start = std::string::npos;
            }
        } else if (ch == ']' && depth == 0) {
            break;
        }
    }
    return blocks;
}

std::vector<std::string> extract_domains_array_objects(const std::string& text) {
    const auto pos = text.find("\"DOMAINS_RESPONSE\"");
    if (pos == std::string::npos) return {};
    return extract_object_blocks(text.substr(pos), "value");
}

std::optional<std::string> extract_string_field(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(text, match, pattern) && match.size() > 1) return match[1].str();
    return std::nullopt;
}

std::optional<bool> extract_bool_field(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() < 2) return std::nullopt;
    return match[1].str() == "true";
}

std::optional<std::uint64_t> extract_uint_field(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(\"?(0x[0-9A-Fa-f]+|[0-9]+)\"?)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() < 2) return std::nullopt;
    return parse_u64(match[1].str());
}

std::optional<std::vector<std::byte>> base64_decode_bytes(const std::string& input) {
    static constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto decode_char = [](char c) -> std::optional<unsigned> {
        const auto pos = kAlphabet.find(c);
        if (pos == std::string_view::npos) return std::nullopt;
        return static_cast<unsigned>(pos);
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

std::string base64_encode_bytes(const std::byte* data, std::size_t size) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const auto b0 = static_cast<unsigned>(std::to_integer<unsigned char>(data[i]));
        const auto b1 = (i + 1 < size) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 1])) : 0U;
        const auto b2 = (i + 2 < size) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 2])) : 0U;
        const auto combined = (b0 << 16U) | (b1 << 8U) | b2;
        out.push_back(kAlphabet[(combined >> 18U) & 0x3FU]);
        out.push_back(kAlphabet[(combined >> 12U) & 0x3FU]);
        out.push_back(i + 1 < size ? kAlphabet[(combined >> 6U) & 0x3FU] : '=');
        out.push_back(i + 2 < size ? kAlphabet[combined & 0x3FU] : '=');
    }
    return out;
}

}  // namespace

UnixSocketMemoryProvider::UnixSocketMemoryProvider(std::filesystem::path socket_path)
    : socket_path_(std::move(socket_path)) {}

UnixSocketMemoryProvider::~UnixSocketMemoryProvider() {
    disconnect();
}

bool UnixSocketMemoryProvider::connect(std::string* error) {
    if (connected()) return true;
    const int handle = socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle < 0) {
        if (error) *error = "socket_create_failed";
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const auto socket_string = socket_path_.string();
    if (socket_string.size() >= sizeof(addr.sun_path)) {
        close_socket_fd(handle);
        if (error) *error = "socket_path_too_long";
        return false;
    }
    std::strncpy(addr.sun_path, socket_string.c_str(), sizeof(addr.sun_path) - 1);
    const auto addr_len =
        static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + socket_string.size() + 1);
    if (::connect(handle, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
        close_socket_fd(handle);
        if (error) *error = "runtime_connect_failed";
        return false;
    }
    socket_ = handle;
    if (!write_all(socket_, "VERSION\n")) {
        disconnect();
        if (error) *error = "runtime_version_write_failed";
        return false;
    }
    const auto version_line = read_line(socket_);
    const auto version = parse_u64(version_line);
    if (!version.has_value()) {
        disconnect();
        if (error) *error = "runtime_version_invalid";
        return false;
    }
    protocol_version_ = static_cast<int>(*version);
    return refresh_remote_metadata(error);
}

void UnixSocketMemoryProvider::disconnect() {
    if (!connected()) return;
    close_socket_fd(socket_);
    socket_ = -1;
    protocol_version_.reset();
    system_name_.clear();
    descriptors_.clear();
}

bool UnixSocketMemoryProvider::connected() const {
    return socket_ >= 0;
}

std::optional<int> UnixSocketMemoryProvider::protocol_version() const {
    return protocol_version_;
}

const std::string& UnixSocketMemoryProvider::system_name() const {
    return system_name_;
}

std::vector<MemoryDomainDescriptor> UnixSocketMemoryProvider::domains() const {
    std::vector<MemoryDomainDescriptor> out;
    out.reserve(descriptors_.size());
    for (const auto& [_, descriptor] : descriptors_) out.push_back(descriptor);
    return out;
}

bool UnixSocketMemoryProvider::has_domain(std::string_view domain_id) const {
    return descriptors_.contains(std::string(domain_id));
}

bool UnixSocketMemoryProvider::is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const {
    const auto it = descriptors_.find(std::string(domain_id));
    if (it == descriptors_.end()) return false;
    return address <= it->second.size_bytes && size <= it->second.size_bytes - static_cast<std::size_t>(address);
}

bool UnixSocketMemoryProvider::read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const {
    if (!connected() || buffer == nullptr || !is_address_valid(domain_id, address, size)) return false;
    std::ostringstream request;
    request << "[{\"type\":\"READ\",\"domain\":\"" << escape_json(std::string(domain_id))
            << "\",\"address\":" << address
            << ",\"size\":" << size
            << "}]\n";
    if (!write_all(socket_, request.str())) return false;
    const auto response = read_line(socket_);
    const auto encoded = extract_response_value_string(response, "READ_RESPONSE");
    if (!encoded.has_value()) return false;
    const auto decoded = base64_decode_bytes(*encoded);
    if (!decoded.has_value() || decoded->size() != size) return false;
    std::memcpy(buffer, decoded->data(), size);
    return true;
}

bool UnixSocketMemoryProvider::write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) {
    const auto it = descriptors_.find(std::string(domain_id));
    if (!connected() || buffer == nullptr || it == descriptors_.end() || !it->second.writable || !is_address_valid(domain_id, address, size)) return false;
    const auto encoded = base64_encode_bytes(buffer, size);
    std::ostringstream request;
    request << "[{\"type\":\"WRITE\",\"domain\":\"" << escape_json(std::string(domain_id))
            << "\",\"address\":" << address
            << ",\"value\":\"" << encoded
            << "\"}]\n";
    if (!write_all(socket_, request.str())) return false;
    const auto response = read_line(socket_);
    return response.find("\"WRITE_RESPONSE\"") != std::string::npos;
}

bool UnixSocketMemoryProvider::refresh_remote_metadata(std::string* error) {
    if (!connected()) {
        if (error) *error = "runtime_not_connected";
        return false;
    }
    if (!write_all(socket_, "[{\"type\":\"SYSTEM\"},{\"type\":\"DOMAINS\"}]\n")) {
        if (error) *error = "runtime_metadata_write_failed";
        return false;
    }
    const auto response = read_line(socket_);
    const auto system = extract_response_value_string(response, "SYSTEM_RESPONSE");
    if (!system.has_value()) {
        if (error) *error = "runtime_system_missing";
        return false;
    }
    system_name_ = *system;
    descriptors_.clear();
    for (const auto& block : extract_domains_array_objects(response)) {
        MemoryDomainDescriptor descriptor;
        descriptor.id = extract_string_field(block, "name").value_or("");
        descriptor.display_name = descriptor.id;
        descriptor.size_bytes = static_cast<std::size_t>(extract_uint_field(block, "size").value_or(0));
        descriptor.writable = extract_bool_field(block, "writable").value_or(false);
        const auto endianness = extract_string_field(block, "endianness").value_or("little");
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

#else

namespace sekailink::sklmi {

UnixSocketMemoryProvider::UnixSocketMemoryProvider(std::filesystem::path socket_path)
    : socket_path_(std::move(socket_path)) {}

UnixSocketMemoryProvider::~UnixSocketMemoryProvider() = default;

bool UnixSocketMemoryProvider::connect(std::string* error) {
    if (error) *error = "unix_socket_memory_provider_unsupported_on_windows";
    return false;
}

void UnixSocketMemoryProvider::disconnect() {
    socket_ = -1;
    protocol_version_.reset();
    system_name_.clear();
    descriptors_.clear();
}

bool UnixSocketMemoryProvider::connected() const {
    return false;
}

std::optional<int> UnixSocketMemoryProvider::protocol_version() const {
    return std::nullopt;
}

const std::string& UnixSocketMemoryProvider::system_name() const {
    return system_name_;
}

std::vector<MemoryDomainDescriptor> UnixSocketMemoryProvider::domains() const {
    return {};
}

bool UnixSocketMemoryProvider::has_domain(std::string_view domain_id) const {
    (void)domain_id;
    return false;
}

bool UnixSocketMemoryProvider::is_address_valid(std::string_view domain_id,
                                                std::uint64_t address,
                                                std::size_t size) const {
    (void)domain_id;
    (void)address;
    (void)size;
    return false;
}

bool UnixSocketMemoryProvider::read(std::string_view domain_id,
                                    std::uint64_t address,
                                    std::byte* buffer,
                                    std::size_t size) const {
    (void)domain_id;
    (void)address;
    (void)buffer;
    (void)size;
    return false;
}

bool UnixSocketMemoryProvider::write(std::string_view domain_id,
                                     std::uint64_t address,
                                     const std::byte* buffer,
                                     std::size_t size) {
    (void)domain_id;
    (void)address;
    (void)buffer;
    (void)size;
    return false;
}

bool UnixSocketMemoryProvider::refresh_remote_metadata(std::string* error) {
    if (error) *error = "unix_socket_memory_provider_unsupported_on_windows";
    return false;
}

}  // namespace sekailink::sklmi

#endif
