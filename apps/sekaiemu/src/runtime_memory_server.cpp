#include "runtime_memory_server.hpp"

#include "host_io_utils.hpp"

#include <fcntl.h>
#include <libretro.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <sstream>

namespace sekaiemu::spike {

namespace {

void CloseFd(int fd) {
  if (fd >= 0) {
    close(fd);
  }
}

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

std::string EscapeJson(std::string_view value) {
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

std::optional<std::uint64_t> ParseUnsigned(std::string_view text) {
  auto trimmed = std::string(text);
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
  trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
  if (trimmed.empty()) {
    return std::nullopt;
  }
  std::uint64_t value = 0;
  auto begin = trimmed.data();
  auto end = trimmed.data() + trimmed.size();
  const bool hex = trimmed.size() > 2 && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X');
  if (hex) {
    begin += 2;
  }
  const auto result = std::from_chars(begin, end, value, hex ? 16 : 10);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return value;
}

std::string LowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::optional<std::string> ExtractStringField(std::string_view text, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":\"";
  const auto begin = text.find(needle);
  if (begin == std::string_view::npos) {
    return std::nullopt;
  }
  const auto start = begin + needle.size();
  const auto end = text.find('"', start);
  if (end == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(text.substr(start, end - start));
}

std::optional<std::uint64_t> ExtractUIntField(std::string_view text, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const auto begin = text.find(needle);
  if (begin == std::string_view::npos) {
    return std::nullopt;
  }
  const auto start = begin + needle.size();
  auto end = text.find_first_of(",}]", start);
  if (end == std::string_view::npos) {
    end = text.size();
  }
  return ParseUnsigned(text.substr(start, end - start));
}

std::optional<std::vector<std::byte>> Base64Decode(std::string_view input) {
  static constexpr std::string_view kAlphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  auto decode_char = [](char c) -> std::optional<unsigned> {
    const auto pos = kAlphabet.find(c);
    if (pos == std::string_view::npos) return std::nullopt;
    return static_cast<unsigned>(pos);
  };

  if (input.size() % 4 != 0) {
    return std::nullopt;
  }

  std::vector<std::byte> out;
  out.reserve((input.size() / 4) * 3);
  for (std::size_t i = 0; i < input.size(); i += 4) {
    const auto d0 = decode_char(input[i]);
    const auto d1 = decode_char(input[i + 1]);
    if (!d0.has_value() || !d1.has_value()) {
      return std::nullopt;
    }
    const auto d2 = input[i + 2] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 2]);
    const auto d3 = input[i + 3] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 3]);
    if (!d2.has_value() || !d3.has_value()) {
      return std::nullopt;
    }
    const auto combined = (*d0 << 18U) | (*d1 << 12U) | (*d2 << 6U) | *d3;
    out.push_back(static_cast<std::byte>((combined >> 16U) & 0xFFU));
    if (input[i + 2] != '=') out.push_back(static_cast<std::byte>((combined >> 8U) & 0xFFU));
    if (input[i + 3] != '=') out.push_back(static_cast<std::byte>(combined & 0xFFU));
  }
  return out;
}

std::string Base64Encode(const std::byte* data, std::size_t size) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

bool SendAll(int fd, const std::string& payload) {
  std::size_t sent = 0;
  while (sent < payload.size()) {
    const auto written = send(fd, payload.data() + sent, payload.size() - sent, 0);
    if (written <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(written);
  }
  return true;
}

}  // namespace

RuntimeMemoryServer::~RuntimeMemoryServer() {
  Shutdown();
}

bool RuntimeMemoryServer::Initialize(const std::filesystem::path& save_directory,
                                     std::optional<std::filesystem::path> override_socket_path,
                                     std::string system_name,
                                     CoreApi* core,
                                     MemoryDomainRegistry* memory_domains,
                                     std::string& error) {
  Shutdown();
  core_ = core;
  memory_domains_ = memory_domains;
  system_name_ = std::move(system_name);
  socket_path_ = override_socket_path.value_or(save_directory / "runtime" / "sekaiemu-memory.sock");

  std::error_code ec;
  std::filesystem::create_directories(socket_path_.parent_path(), ec);
  if (ec) {
    error = "Failed to create runtime memory socket directory.";
    return false;
  }
  std::filesystem::remove(socket_path_, ec);

  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    error = "Failed to create runtime memory Unix socket.";
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  const auto socket_string = socket_path_.string();
  if (socket_string.size() >= sizeof(addr.sun_path)) {
    error = "Runtime memory socket path is too long.";
    Shutdown();
    return false;
  }
  std::strncpy(addr.sun_path, socket_string.c_str(), sizeof(addr.sun_path) - 1);
  const auto addr_len =
      static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + socket_string.size() + 1);

  if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
    error = "Failed to bind runtime memory Unix socket: ";
    error += std::strerror(errno);
    Shutdown();
    return false;
  }
  if (!SetNonBlocking(server_fd_)) {
    error = "Failed to mark runtime memory socket non-blocking.";
    Shutdown();
    return false;
  }
  if (listen(server_fd_, 4) != 0) {
    error = "Failed to listen on runtime memory Unix socket.";
    Shutdown();
    return false;
  }
  return true;
}

void RuntimeMemoryServer::Shutdown() {
  for (const int fd : client_fds_) {
    CloseFd(fd);
  }
  client_fds_.clear();
  CloseFd(server_fd_);
  server_fd_ = -1;
  if (!socket_path_.empty()) {
    std::error_code ec;
    std::filesystem::remove(socket_path_, ec);
    socket_path_.clear();
  }
}

void RuntimeMemoryServer::Poll() {
  if (server_fd_ < 0) {
    return;
  }

  while (true) {
    const int client = accept(server_fd_, nullptr, nullptr);
    if (client < 0) {
      break;
    }
    if (!SetNonBlocking(client)) {
      CloseFd(client);
      continue;
    }
    client_fds_.push_back(client);
  }

  std::vector<int> alive_clients;
  alive_clients.reserve(client_fds_.size());
  std::array<char, 4096> buffer{};
  for (const int fd : client_fds_) {
    const auto read_size = recv(fd, buffer.data(), buffer.size() - 1, MSG_DONTWAIT);
    if (read_size == 0) {
      CloseFd(fd);
      continue;
    }
    if (read_size < 0) {
      alive_clients.push_back(fd);
      continue;
    }
    buffer[static_cast<std::size_t>(read_size)] = '\0';
    std::string payload(buffer.data(), static_cast<std::size_t>(read_size));
    std::size_t start = 0;
    bool keep = true;
    while (start < payload.size()) {
      const auto end = payload.find('\n', start);
      if (end == std::string::npos) {
        break;
      }
      std::string line = payload.substr(start, end - start);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      const auto response = HandleRequestLine(line);
      if (!response.empty() && !SendAll(fd, response + "\n")) {
        keep = false;
        break;
      }
      start = end + 1;
    }
    if (keep) {
      alive_clients.push_back(fd);
    } else {
      CloseFd(fd);
    }
  }
  client_fds_ = std::move(alive_clients);
}

std::vector<RuntimeMemoryDomainInfo> RuntimeMemoryServer::BuildDomainList() const {
  std::vector<RuntimeMemoryDomainInfo> domains;
  if (!core_ || !core_->retro_get_memory_data || !core_->retro_get_memory_size) {
    return domains;
  }

  auto maybe_add = [&](std::string id, unsigned memory_id) {
    void* ptr = core_->retro_get_memory_data(memory_id);
    const auto size = core_->retro_get_memory_size(memory_id);
    if (!ptr || size == 0) {
      return;
    }
    domains.push_back(RuntimeMemoryDomainInfo{
        .id = std::move(id),
        .display_name = domains.empty() ? "Memory" : domains.back().id,
        .size_bytes = size,
        .writable = true,
        .endianness = "little",
    });
    domains.back().display_name = domains.back().id;
  };

  maybe_add("system_ram", RETRO_MEMORY_SYSTEM_RAM);
  maybe_add("save_ram", RETRO_MEMORY_SAVE_RAM);
  maybe_add("video_ram", RETRO_MEMORY_VIDEO_RAM);

  if (memory_domains_) {
    for (const auto& descriptor : memory_domains_->Descriptors()) {
      if (!descriptor.addrspace || descriptor.len == 0) {
        continue;
      }
      const auto name = LowercaseCopy(descriptor.addrspace);
      const bool duplicate = std::any_of(domains.begin(), domains.end(), [&](const auto& domain) {
        return domain.id == name;
      });
      if (duplicate) {
        continue;
      }
      domains.push_back(RuntimeMemoryDomainInfo{
          .id = name,
          .display_name = name,
          .size_bytes = static_cast<std::size_t>(descriptor.len),
          .writable = true,
          .endianness = "little",
      });
    }
  }
  return domains;
}

std::string RuntimeMemoryServer::HandleRequestLine(const std::string& line) {
  if (line == "VERSION") {
    return "1";
  }
  if (!line.empty() && line.front() == '[') {
    return HandleJsonRequest(line);
  }
  return {};
}

std::string RuntimeMemoryServer::HandleJsonRequest(const std::string& line) {
  std::vector<std::string> responses;
  if (line.find("\"SYSTEM\"") != std::string::npos) {
    responses.push_back(BuildSystemResponse());
  }
  if (line.find("\"DOMAINS\"") != std::string::npos) {
    responses.push_back(BuildDomainsResponse());
  }
  if (line.find("\"READ\"") != std::string::npos) {
    responses.push_back(BuildReadResponse(line));
  }
  if (line.find("\"WRITE\"") != std::string::npos) {
    responses.push_back(BuildWriteResponse(line));
  }
  std::ostringstream json;
  json << "[";
  for (std::size_t i = 0; i < responses.size(); ++i) {
    if (i > 0) json << ",";
    json << responses[i];
  }
  json << "]";
  return json.str();
}

std::string RuntimeMemoryServer::BuildSystemResponse() const {
  return "{\"type\":\"SYSTEM_RESPONSE\",\"value\":\"" + EscapeJson(system_name_) + "\"}";
}

std::string RuntimeMemoryServer::BuildDomainsResponse() const {
  std::ostringstream json;
  json << "{\"type\":\"DOMAINS_RESPONSE\",\"value\":[";
  const auto domains = BuildDomainList();
  for (std::size_t i = 0; i < domains.size(); ++i) {
    if (i > 0) json << ",";
    const auto& domain = domains[i];
    json << "{\"name\":\"" << EscapeJson(domain.id)
         << "\",\"size\":" << domain.size_bytes
         << ",\"writable\":" << (domain.writable ? "true" : "false")
         << ",\"endianness\":\"" << domain.endianness
         << "\"}";
  }
  json << "]}";
  return json.str();
}

std::string RuntimeMemoryServer::BuildReadResponse(const std::string& line) const {
  const auto domain = ExtractStringField(line, "domain");
  const auto address = ExtractUIntField(line, "address");
  const auto size = ExtractUIntField(line, "size");
  if (!domain.has_value() || !address.has_value() || !size.has_value()) {
    return "{\"type\":\"ERROR\",\"value\":\"invalid_read_request\"}";
  }
  const auto* resolved = ResolveReadOnly(*domain,
                                         static_cast<std::uint32_t>(*address),
                                         static_cast<std::uint32_t>(*size));
  if (!resolved) {
    return "{\"type\":\"ERROR\",\"value\":\"read_failed\"}";
  }
  const auto encoded = Base64Encode(reinterpret_cast<const std::byte*>(resolved),
                                    static_cast<std::size_t>(*size));
  return "{\"type\":\"READ_RESPONSE\",\"value\":\"" + encoded + "\"}";
}

std::string RuntimeMemoryServer::BuildWriteResponse(const std::string& line) {
  const auto domain = ExtractStringField(line, "domain");
  const auto address = ExtractUIntField(line, "address");
  const auto value = ExtractStringField(line, "value");
  if (!domain.has_value() || !address.has_value() || !value.has_value()) {
    return "{\"type\":\"ERROR\",\"value\":\"invalid_write_request\"}";
  }
  const auto decoded = Base64Decode(*value);
  if (!decoded.has_value()) {
    return "{\"type\":\"ERROR\",\"value\":\"invalid_write_value\"}";
  }
  auto* resolved = ResolveWritable(*domain,
                                   static_cast<std::uint32_t>(*address),
                                   static_cast<std::uint32_t>(decoded->size()));
  if (!resolved) {
    return "{\"type\":\"ERROR\",\"value\":\"write_failed\"}";
  }
  std::memcpy(resolved, decoded->data(), decoded->size());
  return "{\"type\":\"WRITE_RESPONSE\"}";
}

const std::uint8_t* RuntimeMemoryServer::ResolveReadOnly(std::string_view domain_id,
                                                         std::uint32_t address,
                                                         std::uint32_t size) const {
  if (!memory_domains_ || !core_) {
    return nullptr;
  }
  return memory_domains_->Resolve(*core_, domain_id, address, size);
}

std::uint8_t* RuntimeMemoryServer::ResolveWritable(std::string_view domain_id,
                                                   std::uint32_t address,
                                                   std::uint32_t size) const {
  if (!memory_domains_ || !core_) {
    return nullptr;
  }
  return memory_domains_->ResolveMutable(*core_, domain_id, address, size);
}

std::string InferRuntimeSystemName(const std::filesystem::path& core_path) {
  const auto name = LowercaseCopy(core_path.filename().string());
  if (name.find("bsnes") != std::string::npos || name.find("snes") != std::string::npos) {
    return "SNES";
  }
  if (name.find("mgba") != std::string::npos) {
    return "GBA";
  }
  if (name.find("gambatte") != std::string::npos) {
    return "GBGBC";
  }
  if (name.find("nestopia") != std::string::npos || name.find("fce") != std::string::npos) {
    return "NES";
  }
  if (name.find("mupen") != std::string::npos || name.find("parallel") != std::string::npos) {
    return "N64";
  }
  return "LIBRETRO";
}

}  // namespace sekaiemu::spike
