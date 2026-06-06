#include "runtime_memory_server.hpp"

#include <libretro.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::array<std::uint8_t, 32> g_system_ram{};
std::array<std::uint8_t, 16> g_save_ram{};

void* RetroGetMemoryData(unsigned id) {
  if (id == RETRO_MEMORY_SYSTEM_RAM) return g_system_ram.data();
  if (id == RETRO_MEMORY_SAVE_RAM) return g_save_ram.data();
  return nullptr;
}

std::size_t RetroGetMemorySize(unsigned id) {
  if (id == RETRO_MEMORY_SYSTEM_RAM) return g_system_ram.size();
  if (id == RETRO_MEMORY_SAVE_RAM) return g_save_ram.size();
  return 0;
}

void CloseSocket(NativeSocket socket) {
#if defined(_WIN32)
  closesocket(socket);
#else
  close(socket);
#endif
}

bool SendAll(NativeSocket socket, const std::string& payload) {
  std::size_t sent = 0;
  while (sent < payload.size()) {
    const auto written = send(socket,
                              payload.data() + sent,
                              static_cast<int>(payload.size() - sent),
                              0);
    if (written <= 0) return false;
    sent += static_cast<std::size_t>(written);
  }
  return true;
}

std::string ReadLine(NativeSocket socket) {
  std::string line;
  char ch = '\0';
  while (true) {
    const auto received = recv(socket, &ch, 1, 0);
    if (received <= 0) return {};
    if (ch == '\n') break;
    if (ch != '\r') line.push_back(ch);
  }
  return line;
}

NativeSocket ConnectToEndpoint(const std::filesystem::path& endpoint) {
#if defined(_WIN32)
  static bool winsock_ready = [] {
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  if (!winsock_ready) return kInvalidSocket;
  const std::string raw = endpoint.string();
  constexpr std::string_view prefix = "tcp://127.0.0.1:";
  if (raw.rfind(std::string(prefix), 0) != 0) return kInvalidSocket;
  const auto port = static_cast<unsigned short>(std::stoul(raw.substr(prefix.size())));
  NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == kInvalidSocket) return kInvalidSocket;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (connect(socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    CloseSocket(socket);
    return kInvalidSocket;
  }
  return socket;
#else
  NativeSocket socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket == kInvalidSocket) return kInvalidSocket;
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  const auto raw = endpoint.string();
  if (raw.size() >= sizeof(addr.sun_path)) {
    CloseSocket(socket);
    return kInvalidSocket;
  }
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", raw.c_str());
  const auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + raw.size() + 1);
  if (connect(socket, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
    CloseSocket(socket);
    return kInvalidSocket;
  }
  return socket;
#endif
}

bool ExpectContains(const std::string& haystack, const std::string& needle) {
  if (haystack.find(needle) != std::string::npos) return true;
  std::cerr << "expected substring missing: " << needle << "\nresponse: " << haystack << "\n";
  return false;
}

}  // namespace

int main() {
  g_system_ram.fill(0);
  g_save_ram.fill(0);
  g_system_ram[4] = 0x12;
  g_system_ram[5] = 0x34;

  sekaiemu::spike::CoreApi core{};
  core.retro_get_memory_data = RetroGetMemoryData;
  core.retro_get_memory_size = RetroGetMemorySize;

  retro_memory_descriptor descriptor{};
  descriptor.ptr = g_system_ram.data();
  descriptor.start = 0x02000000u;
  descriptor.select = 0xFF000000u;
  descriptor.disconnect = 0;
  descriptor.len = g_system_ram.size();
  descriptor.addrspace = "System Bus";

  retro_memory_map map{};
  map.descriptors = &descriptor;
  map.num_descriptors = 1;

  sekaiemu::spike::MemoryDomainRegistry domains;
  domains.SetMemoryMaps(&map);

  sekaiemu::spike::RuntimeMemoryServer server;
  std::string error;
  const auto temp_dir = std::filesystem::temp_directory_path() / "sekaiemu-bizhawk-protocol-smoke";
  std::filesystem::create_directories(temp_dir);
  if (!server.Initialize(temp_dir, std::nullopt, "GBA", &core, &domains, error)) {
    std::cerr << "server_init_failed:" << error << "\n";
    return 1;
  }

  std::atomic<bool> stop{false};
  std::thread poller([&]() {
    while (!stop.load()) {
      server.Poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  NativeSocket client = kInvalidSocket;
  for (int attempt = 0; attempt < 100; ++attempt) {
    client = ConnectToEndpoint(server.SocketPath());
    if (client != kInvalidSocket) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (client == kInvalidSocket) {
    stop.store(true);
    poller.join();
    std::cerr << "connect_failed\n";
    return 1;
  }

  auto transact = [&](const std::string& request) {
    if (!SendAll(client, request + "\n")) return std::string{};
    for (int attempt = 0; attempt < 100; ++attempt) {
      server.Poll();
      const auto line = ReadLine(client);
      if (!line.empty()) return line;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return std::string{};
  };

  bool ok = true;
  ok = ok && SendAll(client, "VERSION\n");
  ok = ok && ExpectContains(ReadLine(client), "1");

  const auto handshake = transact("[{\"type\":\"PING\"},{\"type\":\"SYSTEM\"},{\"type\":\"PREFERRED_CORES\"},{\"type\":\"HASH\"},{\"type\":\"DOMAINS\"}]");
  ok = ok && ExpectContains(handshake, "\"PONG\"");
  ok = ok && ExpectContains(handshake, "\"SYSTEM_RESPONSE\",\"value\":\"GBA\"");
  ok = ok && ExpectContains(handshake, "\"PREFERRED_CORES_RESPONSE\"");
  ok = ok && ExpectContains(handshake, "\"HASH_RESPONSE\"");
  ok = ok && ExpectContains(handshake, "\"name\":\"System Bus\"");

  const auto memory_size = transact("[{\"type\":\"MEMORY_SIZE\",\"domain\":\"System Bus\"}]");
  ok = ok && ExpectContains(memory_size, "\"MEMORY_SIZE_RESPONSE\"");

  const auto guard_read = transact(
      "[{\"type\":\"GUARD\",\"domain\":\"System Bus\",\"address\":33554436,\"expected_data\":\"EjQ=\"},"
      "{\"type\":\"READ\",\"domain\":\"System Bus\",\"address\":33554436,\"size\":2}]");
  ok = ok && ExpectContains(guard_read, "\"GUARD_RESPONSE\",\"value\":true");
  ok = ok && ExpectContains(guard_read, "\"READ_RESPONSE\",\"value\":\"EjQ=\"");

  const auto guard_fail = transact(
      "[{\"type\":\"GUARD\",\"domain\":\"System Bus\",\"address\":33554436,\"expected_data\":\"AAAA\"},"
      "{\"type\":\"READ\",\"domain\":\"System Bus\",\"address\":33554436,\"size\":2}]");
  ok = ok && ExpectContains(guard_fail, "\"GUARD_RESPONSE\",\"value\":false");

  const auto lock_only = transact("[{\"type\":\"LOCK\"}]");
  ok = ok && ExpectContains(lock_only, "\"LOCKED\"");
  ok = ok && server.Locked();
  const auto unlock_only = transact("[{\"type\":\"UNLOCK\"}]");
  ok = ok && ExpectContains(unlock_only, "\"UNLOCKED\"");
  ok = ok && !server.Locked();

  const auto write = transact("[{\"type\":\"LOCK\"},{\"type\":\"WRITE\",\"domain\":\"System Bus\",\"address\":33554438,\"value\":\"qrs=\"},{\"type\":\"UNLOCK\"}]");
  ok = ok && ExpectContains(write, "\"LOCKED\"");
  ok = ok && ExpectContains(write, "\"WRITE_RESPONSE\"");
  ok = ok && ExpectContains(write, "\"UNLOCKED\"");
  ok = ok && g_system_ram[6] == 0xAA && g_system_ram[7] == 0xBB;

  const auto messages = transact("[{\"type\":\"DISPLAY_MESSAGE\",\"message\":\"hello\"},{\"type\":\"SET_MESSAGE_INTERVAL\",\"value\":0.25}]");
  ok = ok && ExpectContains(messages, "\"DISPLAY_MESSAGE_RESPONSE\"");
  ok = ok && ExpectContains(messages, "\"SET_MESSAGE_INTERVAL_RESPONSE\"");

  CloseSocket(client);
  stop.store(true);
  poller.join();
  server.Shutdown();

  if (!ok) return 1;
  std::cout << "sekaiemu_runtime_memory_server_bizhawk_protocol_smoke_ok\n";
  return 0;
}
