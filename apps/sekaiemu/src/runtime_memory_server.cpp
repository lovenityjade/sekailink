#include "runtime_memory_server.hpp"

#include "host_io_utils.hpp"
#include "runtime_memory_socket_utils.hpp"
#include "runtime_memory_utils.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef DrawText
#undef DrawText
#endif
using NativeSocket = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using NativeSocket = int;
#endif

#include <libretro.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace sekaiemu::spike {

RuntimeMemoryServer::~RuntimeMemoryServer() {
  Shutdown();
}

bool RuntimeMemoryServer::Initialize(const std::filesystem::path& save_directory,
                                     std::optional<std::filesystem::path> override_socket_path,
                                     std::string system_name,
                                     CoreApi* core,
                                     MemoryDomainRegistry* memory_domains,
                                     std::string& error,
                                     std::optional<std::filesystem::path> rom_path) {
  Shutdown();
  core_ = core;
  memory_domains_ = memory_domains;
  system_name_ = std::move(system_name);
  rom_data_.clear();
  prg_rom_offset_ = 0;
  if (rom_path.has_value() && !rom_path->empty()) {
    rom_data_ = ReadBinaryFile(*rom_path);
    const bool has_ines_header = rom_data_.size() > 16 &&
        rom_data_[0] == 'N' && rom_data_[1] == 'E' && rom_data_[2] == 'S' && rom_data_[3] == 0x1A;
    if (has_ines_header && LowercaseCopy(system_name_) == "nes") {
      prg_rom_offset_ = 16;
    }
  }

  if (!EnsureSocketRuntime()) {
    error = "Failed to initialize socket runtime.";
    return false;
  }

  std::error_code ec;
  const std::string endpoint = override_socket_path.has_value()
      ? override_socket_path->string()
      : std::string();

  auto parse_loopback_tcp_endpoint = [](const std::string& raw, std::uint16_t& out_port) -> bool {
    constexpr std::string_view prefix = "tcp://127.0.0.1:";
    if (raw.rfind(std::string(prefix), 0) != 0) {
      return false;
    }
    try {
      out_port = static_cast<std::uint16_t>(std::stoul(raw.substr(prefix.size())));
      return true;
    } catch (...) {
      return false;
    }
  };

#if defined(_WIN32)
  (void)save_directory;
  std::uint16_t requested_port = 0;
  if (!endpoint.empty()) {
    if (!parse_loopback_tcp_endpoint(endpoint, requested_port)) {
      error = "Windows runtime memory server requires tcp://127.0.0.1:<port> override.";
      return false;
    }
  }

  SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server == INVALID_SOCKET) {
    error = "Failed to create runtime memory TCP socket.";
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(requested_port);
  if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    error = "Failed to bind runtime memory TCP socket.";
    closesocket(server);
    return false;
  }

  sockaddr_in bound{};
  int bound_len = sizeof(bound);
  if (getsockname(server, reinterpret_cast<sockaddr*>(&bound), &bound_len) != 0) {
    error = "Failed to resolve runtime memory TCP port.";
    closesocket(server);
    return false;
  }
  if (!SetNonBlocking(static_cast<std::intptr_t>(server))) {
    error = "Failed to mark runtime memory TCP socket non-blocking.";
    closesocket(server);
    return false;
  }
  if (listen(server, 4) != 0) {
    error = "Failed to listen on runtime memory TCP socket.";
    closesocket(server);
    return false;
  }

  const auto port = ntohs(bound.sin_port);
  socket_path_ = std::filesystem::path("tcp://127.0.0.1:" + std::to_string(port));
  server_fd_ = static_cast<std::intptr_t>(server);
  return true;
#else
  std::uint16_t requested_port = 0;
  if (!endpoint.empty() && endpoint.rfind("tcp://", 0) == 0) {
    if (!parse_loopback_tcp_endpoint(endpoint, requested_port)) {
      error = "Runtime memory TCP endpoint must be tcp://127.0.0.1:<port>.";
      return false;
    }

    server_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd_ < 0) {
      error = "Failed to create runtime memory TCP socket.";
      return false;
    }

    int reuse = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(requested_port);
    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      error = "Failed to bind runtime memory TCP socket: ";
      error += std::strerror(errno);
      Shutdown();
      return false;
    }

    sockaddr_in bound{};
    socklen_t bound_len = sizeof(bound);
    if (getsockname(server_fd_, reinterpret_cast<sockaddr*>(&bound), &bound_len) != 0) {
      error = "Failed to resolve runtime memory TCP port.";
      Shutdown();
      return false;
    }
    if (!SetNonBlocking(server_fd_)) {
      error = "Failed to mark runtime memory TCP socket non-blocking.";
      Shutdown();
      return false;
    }
    if (listen(server_fd_, 4) != 0) {
      error = "Failed to listen on runtime memory TCP socket.";
      Shutdown();
      return false;
    }

    const auto port = ntohs(bound.sin_port);
    socket_path_ = std::filesystem::path("tcp://127.0.0.1:" + std::to_string(port));
    return true;
  }

  socket_path_ = override_socket_path.value_or(save_directory / "runtime" / "sekaiemu-memory.sock");
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
#endif
}

void RuntimeMemoryServer::Shutdown() {
  for (const auto fd : client_fds_) {
    CloseFd(fd);
  }
  client_fds_.clear();
  client_buffers_.clear();
  CloseFd(server_fd_);
  server_fd_ = -1;
  read_trace_count_ = 0;
  write_trace_count_ = 0;
  guard_trace_count_ = 0;
  if (!socket_path_.empty()) {
#if !defined(_WIN32)
    if (socket_path_.string().rfind("tcp://", 0) != 0) {
      std::error_code ec;
      std::filesystem::remove(socket_path_, ec);
    }
#endif
    socket_path_.clear();
  }
}

void RuntimeMemoryServer::Poll() {
  if (server_fd_ < 0) {
    return;
  }

  while (true) {
    const auto client = accept(static_cast<NativeSocket>(server_fd_), nullptr, nullptr);
    if (client == static_cast<NativeSocket>(-1)
#if defined(_WIN32)
        || client == INVALID_SOCKET
#endif
    ) {
      break;
    }
    const auto client_handle = static_cast<std::intptr_t>(client);
    if (!SetNonBlocking(client_handle)) {
      CloseFd(client_handle);
      continue;
    }
    client_fds_.push_back(client_handle);
  }

  std::vector<std::intptr_t> alive_clients;
  alive_clients.reserve(client_fds_.size());
  std::array<char, 4096> buffer{};
  for (const auto fd : client_fds_) {
    const auto read_size = recv(static_cast<NativeSocket>(fd),
                                buffer.data(),
                                static_cast<int>(buffer.size() - 1),
#if defined(_WIN32)
                                0
#else
                                MSG_DONTWAIT
#endif
                                );
    if (read_size == 0) {
      CloseFd(fd);
      client_buffers_.erase(fd);
      continue;
    }
    if (read_size < 0) {
      if (WouldBlock()) {
        alive_clients.push_back(fd);
      } else {
        CloseFd(fd);
        client_buffers_.erase(fd);
      }
      continue;
    }
    auto& payload = client_buffers_[fd];
    payload.append(buffer.data(), static_cast<std::size_t>(read_size));
    bool keep = true;
    while (true) {
      const auto end = payload.find('\n');
      if (end == std::string::npos) {
        break;
      }
      std::string line = payload.substr(0, end);
      payload.erase(0, end + 1);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      const auto response = HandleRequestLine(line);
      if (!response.empty() && !SendAll(fd, response + "\n")) {
        keep = false;
        break;
      }
    }
    if (payload.size() > 4 * 1024 * 1024) {
      keep = false;
    }
    if (keep) {
      alive_clients.push_back(fd);
    } else {
      CloseFd(fd);
      client_buffers_.erase(fd);
    }
  }
  client_fds_ = std::move(alive_clients);
}

std::vector<RuntimeMemoryDomainInfo> RuntimeMemoryServer::BuildDomainList() const {
  std::vector<RuntimeMemoryDomainInfo> domains;
  if (!core_ || !core_->retro_get_memory_data || !core_->retro_get_memory_size) {
    return domains;
  }

  auto has_domain = [&](std::string_view id) {
    return std::any_of(domains.begin(), domains.end(), [&](const auto& domain) {
      return domain.id == id;
    });
  };

  auto add_domain = [&](std::string id, std::size_t size, bool writable = true) {
    if (id.empty() || size == 0 || has_domain(id)) {
      return;
    }
    domains.push_back(RuntimeMemoryDomainInfo{
        .id = std::move(id),
        .display_name = domains.empty() ? "Memory" : domains.back().id,
        .size_bytes = size,
        .writable = writable,
        .endianness = "little",
    });
    domains.back().display_name = domains.back().id;
  };

  auto maybe_add = [&](std::string id, unsigned memory_id) {
    void* ptr = core_->retro_get_memory_data(memory_id);
    const auto size = core_->retro_get_memory_size(memory_id);
    if (!ptr || size == 0) {
      return;
    }
    add_domain(std::move(id), size);
  };

  maybe_add("system_ram", RETRO_MEMORY_SYSTEM_RAM);
  maybe_add("save_ram", RETRO_MEMORY_SAVE_RAM);
  maybe_add("video_ram", RETRO_MEMORY_VIDEO_RAM);

  const auto system_ram_size = core_->retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
  if (system_ram_size > 0) {
    add_domain("RAM", system_ram_size);
    add_domain("WRAM", system_ram_size);
    if (IsGbaSystem(system_name_) && system_ram_size >= 0x00048000u) {
      add_domain("EWRAM", 0x00040000u);
      add_domain("IWRAM", 0x00008000u);
    }
  }

  const auto save_ram_size = core_->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (save_ram_size > 0) {
    add_domain("Battery RAM", save_ram_size);
    add_domain("SRAM", save_ram_size);
    if (IsGameBoySystem(system_name_)) {
      add_domain("CartRAM", save_ram_size);
      add_domain("Cart RAM", save_ram_size);
    }
  }

  const auto video_ram_size = core_->retro_get_memory_size(RETRO_MEMORY_VIDEO_RAM);
  if (video_ram_size > 0) {
    add_domain("VRAM", video_ram_size);
  }

  if (!rom_data_.empty()) {
    add_domain("ROM", rom_data_.size(), false);
    add_domain("Cart ROM", rom_data_.size(), false);
    if (prg_rom_offset_ < rom_data_.size()) {
      add_domain("PRG ROM", rom_data_.size() - prg_rom_offset_, false);
    }
  }

  // BizHawk generic AP clients commonly address "System Bus" with absolute
  // addresses. Expose a large virtual bus domain; MemoryDomainRegistry resolves
  // it through libretro memory maps at read/write time.
  if (memory_domains_ &&
      (!memory_domains_->Descriptors().empty() || IsGbaSystem(system_name_) || IsGameBoySystem(system_name_))) {
    constexpr std::size_t kVirtualBusSize = static_cast<std::size_t>(0x100000000ull);
    add_domain("System Bus", kVirtualBusSize);
    add_domain("system_bus", kVirtualBusSize);
    add_domain("bus", kVirtualBusSize);
    if (IsGbaSystem(system_name_)) {
      add_domain("gba_system_bus", kVirtualBusSize);
    } else if (IsGameBoySystem(system_name_)) {
      add_domain("gb_system_bus", kVirtualBusSize);
    }
  }

  if (memory_domains_) {
    for (const auto& descriptor : memory_domains_->Descriptors()) {
      if (!descriptor.addrspace || descriptor.len == 0) {
        continue;
      }
      const auto name = LowercaseCopy(descriptor.addrspace);
      add_domain(name, static_cast<std::size_t>(descriptor.len));
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
  try {
    const auto requests = nlohmann::json::parse(line);
    if (!requests.is_array()) {
      return "[" + JsonError("request_must_be_array") + "]";
    }

    std::optional<std::string> failed_guard_response;
    for (const auto& req : requests) {
      if (failed_guard_response.has_value()) {
        responses.push_back(*failed_guard_response);
        continue;
      }

      if (!req.is_object()) {
        responses.push_back(JsonError("request_must_be_object"));
        continue;
      }

      const auto type = req.value("type", std::string{});
      const auto request_line = req.dump();
      if (type == "PING") {
        responses.push_back("{\"type\":\"PONG\"}");
      } else if (type == "SYSTEM") {
        responses.push_back(BuildSystemResponse());
      } else if (type == "PREFERRED_CORES") {
        responses.push_back(BuildPreferredCoresResponse());
      } else if (type == "HASH") {
        responses.push_back(BuildHashResponse());
      } else if (type == "MEMORY_SIZE") {
        responses.push_back(BuildMemorySizeResponse(request_line));
      } else if (type == "DOMAINS") {
        responses.push_back(BuildDomainsResponse());
      } else if (type == "GUARD") {
        const auto response = BuildGuardResponse(request_line);
        responses.push_back(response);
        if (response.find("\"GUARD_RESPONSE\"") != std::string::npos &&
            response.find("\"value\":false") != std::string::npos) {
          failed_guard_response = response;
        }
      } else if (type == "LOCK") {
        locked_ = true;
        responses.push_back("{\"type\":\"LOCKED\"}");
      } else if (type == "UNLOCK") {
        locked_ = false;
        responses.push_back("{\"type\":\"UNLOCKED\"}");
      } else if (type == "READ") {
        responses.push_back(BuildReadResponse(request_line));
      } else if (type == "WRITE") {
        responses.push_back(BuildWriteResponse(request_line));
      } else if (type == "DISPLAY_MESSAGE") {
        responses.push_back(BuildDisplayMessageResponse());
      } else if (type == "SET_MESSAGE_INTERVAL") {
        responses.push_back(BuildSetMessageIntervalResponse(request_line));
      } else {
        responses.push_back(JsonError("Unknown command: " + type));
      }
    }
  } catch (const std::exception& ex) {
    responses.push_back(JsonError(ex.what()));
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

std::string RuntimeMemoryServer::BuildPreferredCoresResponse() const {
  return "{\"type\":\"PREFERRED_CORES_RESPONSE\",\"value\":{}}";
}

std::string RuntimeMemoryServer::BuildHashResponse() const {
  // The libretro host does not currently own ROM hash metadata at this layer.
  // Keep the command available so generic AP clients can complete their handshake.
  return "{\"type\":\"HASH_RESPONSE\",\"value\":\"\"}";
}

std::string RuntimeMemoryServer::BuildMemorySizeResponse(const std::string& line) const {
  const auto domain = ExtractStringField(line, "domain");
  if (!domain.has_value()) {
    return JsonError("invalid_memory_size_request");
  }
  for (const auto& entry : BuildDomainList()) {
    if (entry.id == *domain || entry.display_name == *domain) {
      return "{\"type\":\"MEMORY_SIZE_RESPONSE\",\"value\":" + std::to_string(entry.size_bytes) + "}";
    }
  }
  return JsonError("memory_size_failed");
}

std::string RuntimeMemoryServer::BuildDomainsResponse() const {
  return "{\"type\":\"DOMAINS_RESPONSE\",\"value\":" + DomainListToJson(BuildDomainList()) + "}";
}

std::string RuntimeMemoryServer::BuildGuardResponse(const std::string& line) const {
  const auto domain = ExtractStringField(line, "domain");
  const auto address = ExtractUIntField(line, "address");
  const auto expected = ExtractStringField(line, "expected_data");
  if (!domain.has_value() || !address.has_value() || !expected.has_value()) {
    return JsonError("invalid_guard_request");
  }
  const auto decoded = Base64Decode(*expected);
  if (!decoded.has_value()) {
    return JsonError("invalid_guard_value");
  }
  const auto* resolved = ResolveReadOnly(*domain,
                                         static_cast<std::uint32_t>(*address),
                                         static_cast<std::uint32_t>(decoded->size()));
  bool ok = false;
  if (resolved) {
    ok = std::memcmp(resolved, decoded->data(), decoded->size()) == 0;
  }
  TraceMemoryRequest("GUARD",
                     *domain,
                     static_cast<std::uint32_t>(*address),
                     static_cast<std::uint32_t>(decoded->size()),
                     ok,
                     ok ? "matched" : "mismatch_or_unresolved");
  return "{\"type\":\"GUARD_RESPONSE\",\"value\":" + std::string(ok ? "true" : "false") +
         ",\"address\":" + std::to_string(*address) + "}";
}

std::string RuntimeMemoryServer::BuildReadResponse(const std::string& line) const {
  const auto domain = ExtractStringField(line, "domain");
  const auto address = ExtractUIntField(line, "address");
  const auto size = ExtractUIntField(line, "size");
  if (!domain.has_value() || !address.has_value() || !size.has_value()) {
    return JsonError("invalid_read_request");
  }
  const auto* resolved = ResolveReadOnly(*domain,
                                         static_cast<std::uint32_t>(*address),
                                         static_cast<std::uint32_t>(*size));
  if (!resolved) {
    TraceMemoryRequest("READ",
                       *domain,
                       static_cast<std::uint32_t>(*address),
                       static_cast<std::uint32_t>(*size),
                       false,
                       "read_failed");
    return JsonError("read_failed");
  }
  TraceMemoryRequest("READ",
                     *domain,
                     static_cast<std::uint32_t>(*address),
                     static_cast<std::uint32_t>(*size),
                     true,
                     "ok");
  const auto encoded = Base64Encode(reinterpret_cast<const std::byte*>(resolved),
                                    static_cast<std::size_t>(*size));
  return "{\"type\":\"READ_RESPONSE\",\"value\":\"" + encoded + "\"}";
}

std::string RuntimeMemoryServer::BuildWriteResponse(const std::string& line) {
  const auto domain = ExtractStringField(line, "domain");
  const auto address = ExtractUIntField(line, "address");
  const auto value = ExtractStringField(line, "value");
  if (!domain.has_value() || !address.has_value() || !value.has_value()) {
    return JsonError("invalid_write_request");
  }
  const auto decoded = Base64Decode(*value);
  if (!decoded.has_value()) {
    return JsonError("invalid_write_value");
  }
  const auto normalized_domain = NormalizeDomainName(*domain);
  if (IsGameBoySystem(system_name_) && IsRomDomain(normalized_domain)) {
    if (ApplyGameBoyRomWrite(normalized_domain,
                             static_cast<std::uint32_t>(*address),
                             *decoded)) {
      TraceMemoryRequest("WRITE",
                         *domain,
                         static_cast<std::uint32_t>(*address),
                         static_cast<std::uint32_t>(decoded->size()),
                         true,
                         "gameboy_rom_patch");
      return "{\"type\":\"WRITE_RESPONSE\"}";
    }
    TraceMemoryRequest("WRITE",
                       *domain,
                       static_cast<std::uint32_t>(*address),
                       static_cast<std::uint32_t>(decoded->size()),
                       false,
                       "gameboy_rom_patch_failed");
    return JsonError("write_failed");
  }
  auto* resolved = ResolveWritable(*domain,
                                   static_cast<std::uint32_t>(*address),
                                   static_cast<std::uint32_t>(decoded->size()));
  if (!resolved) {
    TraceMemoryRequest("WRITE",
                       *domain,
                       static_cast<std::uint32_t>(*address),
                       static_cast<std::uint32_t>(decoded->size()),
                       false,
                       "write_failed");
    return JsonError("write_failed");
  }
  std::memcpy(resolved, decoded->data(), decoded->size());
  TraceMemoryRequest("WRITE",
                     *domain,
                     static_cast<std::uint32_t>(*address),
                     static_cast<std::uint32_t>(decoded->size()),
                     true,
                     "ok");
  return "{\"type\":\"WRITE_RESPONSE\"}";
}

bool RuntimeMemoryServer::ApplyGameBoyRomWrite(std::string_view normalized_domain,
                                               std::uint32_t address,
                                               const std::vector<std::byte>& bytes) {
  if (!core_ || !core_->retro_cheat_set || rom_data_.empty() || bytes.empty()) {
    return false;
  }

  const std::size_t base = normalized_domain == "prg_rom" ? prg_rom_offset_ : 0;
  const std::size_t offset = base + static_cast<std::size_t>(address);
  if (offset > rom_data_.size() || bytes.size() > rom_data_.size() - offset) {
    return false;
  }

  for (std::size_t i = 0; i < bytes.size(); ++i) {
    const std::size_t absolute_offset = offset + i;
    const auto cpu_address = GameBoyRomOffsetToCpuAddress(absolute_offset);
    if (!cpu_address.has_value()) {
      return false;
    }

    const auto new_value = static_cast<std::uint8_t>(bytes[i]);
    const auto old_value = rom_data_[absolute_offset];
    const auto code = EncodeGameBoyGameGeniePatch(*cpu_address, new_value, old_value);
    auto [it, inserted] = game_boy_rom_patch_indices_.try_emplace(absolute_offset,
                                                                  next_game_boy_rom_patch_index_);
    if (inserted) {
      ++next_game_boy_rom_patch_index_;
    }
    core_->retro_cheat_set(it->second, true, code.c_str());
    rom_data_[absolute_offset] = new_value;
  }
  return true;
}

std::string RuntimeMemoryServer::BuildDisplayMessageResponse() {
  return "{\"type\":\"DISPLAY_MESSAGE_RESPONSE\"}";
}

std::string RuntimeMemoryServer::BuildSetMessageIntervalResponse(const std::string& line) {
  const auto value = ExtractDoubleField(line, "value");
  if (value.has_value()) {
    message_interval_seconds_ = *value;
  }
  return "{\"type\":\"SET_MESSAGE_INTERVAL_RESPONSE\"}";
}

const std::uint8_t* RuntimeMemoryServer::ResolveReadOnly(std::string_view domain_id,
                                                         std::uint32_t address,
                                                         std::uint32_t size) const {
  const auto normalized_domain = NormalizeDomainName(std::string(domain_id));
  if (!rom_data_.empty() && IsRomDomain(normalized_domain)) {
    const std::size_t base = normalized_domain == "prg_rom" ? prg_rom_offset_ : 0;
    const std::size_t offset = base + static_cast<std::size_t>(address);
    const std::size_t length = static_cast<std::size_t>(size);
    if (offset <= rom_data_.size() && length <= rom_data_.size() - offset) {
      return rom_data_.data() + offset;
    }
    return nullptr;
  }
  if (IsGbaSystem(system_name_) && core_ &&
      (IsGbaSramDomain(normalized_domain) || IsGbaBusDomain(normalized_domain))) {
    if (const auto* gba_resolved =
            ResolveGbaLibretroMemory(*core_, normalized_domain, address, size)) {
      return gba_resolved;
    }
  }
  if (!rom_data_.empty() && IsGbaSystem(system_name_) && IsGbaBusDomain(normalized_domain)) {
    constexpr std::uint32_t kGbaRomBase = 0x08000000u;
    constexpr std::uint32_t kGbaRomMirrorSize = 0x02000000u;
    const std::uint32_t mirror_offset = address >= kGbaRomBase
        ? (address - kGbaRomBase) % kGbaRomMirrorSize
        : address;
    const std::size_t offset = static_cast<std::size_t>(mirror_offset);
    const std::size_t length = static_cast<std::size_t>(size);
    if (offset <= rom_data_.size() && length <= rom_data_.size() - offset) {
      return rom_data_.data() + offset;
    }
  }
  if (!core_) {
    return nullptr;
  }
  if (IsGameBoySystem(system_name_) && IsGameBoyBusDomain(normalized_domain)) {
    if (!rom_data_.empty() && address < 0x8000u) {
      const std::size_t offset = static_cast<std::size_t>(address);
      const std::size_t length = static_cast<std::size_t>(size);
      if (offset <= rom_data_.size() && length <= rom_data_.size() - offset) {
        return rom_data_.data() + offset;
      }
    }
  }
  if (IsGbaSystem(system_name_) &&
      (IsGbaEwramDomain(normalized_domain) || IsGbaIwramDomain(normalized_domain))) {
    if (const auto* gba_resolved =
            ResolveGbaLibretroMemory(*core_, normalized_domain, address, size)) {
      return gba_resolved;
    }
    if (memory_domains_) {
      if (const auto bus_address = GbaRelativeDomainToBusAddress(normalized_domain, address, size)) {
        if (const auto* bus_resolved =
                memory_domains_->Resolve(*core_, "System Bus", *bus_address, size)) {
          return bus_resolved;
        }
      }
    }
  }
  if (!memory_domains_) {
    return nullptr;
  }
  if (IsGbaSystem(system_name_) && IsGbaSramDomain(normalized_domain)) {
    if (const auto bus_address = GbaRelativeDomainToBusAddress(normalized_domain, address, size)) {
      if (const auto* bus_resolved =
              memory_domains_->Resolve(*core_, "System Bus", *bus_address, size)) {
        return bus_resolved;
      }
    }
  }
  if (const auto* resolved = memory_domains_->Resolve(*core_, domain_id, address, size)) {
    return resolved;
  }
  if (IsGbaSystem(system_name_) && IsGbaBusDomain(normalized_domain)) {
    return ResolveGbaLibretroMemory(*core_, normalized_domain, address, size);
  }
  return nullptr;
}

std::uint8_t* RuntimeMemoryServer::ResolveWritable(std::string_view domain_id,
                                                   std::uint32_t address,
                                                   std::uint32_t size) const {
  const auto normalized_domain = NormalizeDomainName(std::string(domain_id));
  if (!core_) {
    return nullptr;
  }
  if (IsRomDomain(normalized_domain)) {
    if (memory_domains_) {
      if (auto* resolved = memory_domains_->ResolveMutable(*core_, domain_id, address, size)) {
        return resolved;
      }
    }
    return nullptr;
  }
  if (IsGbaSystem(system_name_) &&
      (IsGbaEwramDomain(normalized_domain) || IsGbaIwramDomain(normalized_domain) ||
       IsGbaSramDomain(normalized_domain) || IsGbaBusDomain(normalized_domain))) {
    if (auto* gba_resolved =
            ResolveGbaLibretroMemoryMutable(*core_, normalized_domain, address, size)) {
      return gba_resolved;
    }
    if (memory_domains_ && !IsGbaBusDomain(normalized_domain)) {
      if (const auto bus_address = GbaRelativeDomainToBusAddress(normalized_domain, address, size)) {
        if (auto* bus_resolved =
                memory_domains_->ResolveMutable(*core_, "System Bus", *bus_address, size)) {
          return bus_resolved;
        }
      }
    }
  }
  if (!memory_domains_) {
    return nullptr;
  }
  if (auto* resolved = memory_domains_->ResolveMutable(*core_, domain_id, address, size)) {
    return resolved;
  }
  if (IsGbaSystem(system_name_) && IsGbaBusDomain(normalized_domain)) {
    return ResolveGbaLibretroMemoryMutable(*core_, normalized_domain, address, size);
  }
  return nullptr;
}

void RuntimeMemoryServer::TraceMemoryRequest(std::string_view type,
                                             std::string_view domain,
                                             std::uint32_t address,
                                             std::uint32_t size,
                                             bool ok,
                                             std::string_view detail) const {
  std::uint64_t count = 0;
  const auto type_text = std::string(type);
  if (type_text == "READ") {
    count = ++read_trace_count_;
  } else if (type_text == "WRITE") {
    count = ++write_trace_count_;
  } else if (type_text == "GUARD") {
    count = ++guard_trace_count_;
  }

  const bool should_trace =
      !ok ||
      type_text == "WRITE" ||
      type_text == "GUARD" ||
      count <= 24 ||
      count % 1000 == 0;
  if (!should_trace) {
    return;
  }

  std::cerr << "[sekaiemu-memory] " << type
            << " #" << count
            << " system=" << system_name_
            << " domain=" << domain
            << " address=0x" << std::hex << address
            << " size=0x" << size
            << std::dec
            << " ok=" << (ok ? "true" : "false")
            << " detail=" << detail
            << "\n";
}

}  // namespace sekaiemu::spike
