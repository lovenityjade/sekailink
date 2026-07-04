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
#include <arpa/inet.h>
#include <netinet/in.h>
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
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::array<std::uint8_t, 0x00048000> g_system_ram{};
std::array<std::uint8_t, 0x00008000> g_iwram{};
std::array<std::uint8_t, 0x00020000> g_save_ram{};
bool g_split_gba_iwram = false;
std::size_t g_reported_save_ram_size = g_save_ram.size();

void* RetroGetMemoryData(unsigned id) {
  if (id == RETRO_MEMORY_SYSTEM_RAM) return g_system_ram.data();
  if (id == RETRO_MEMORY_SAVE_RAM) return g_save_ram.data();
  return nullptr;
}

std::size_t RetroGetMemorySize(unsigned id) {
  if (id == RETRO_MEMORY_SYSTEM_RAM) return g_split_gba_iwram ? 0x00040000 : g_system_ram.size();
  if (id == RETRO_MEMORY_SAVE_RAM) return g_reported_save_ram_size;
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
#endif
  const std::string raw = endpoint.string();
  constexpr std::string_view prefix = "tcp://127.0.0.1:";
  if (raw.rfind(std::string(prefix), 0) == 0) {
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
  }
#if defined(_WIN32)
  return kInvalidSocket;
#else
  NativeSocket socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket == kInvalidSocket) return kInvalidSocket;
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
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
  g_reported_save_ram_size = g_save_ram.size();
  g_save_ram[0x1FFFE] = 0x56;
  g_save_ram[0x1FFFF] = 0x78;

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
  const auto rom_path = temp_dir / "fake-zelda.nes";
  {
    std::array<std::uint8_t, 32> fake_rom{};
    fake_rom[0] = 'N';
    fake_rom[1] = 'E';
    fake_rom[2] = 'S';
    fake_rom[3] = 0x1A;
    fake_rom[16] = 'L';
    fake_rom[17] = 'O';
    fake_rom[18] = 'Z';
    std::ofstream rom(rom_path, std::ios::binary);
    rom.write(reinterpret_cast<const char*>(fake_rom.data()), static_cast<std::streamsize>(fake_rom.size()));
  }
  if (!server.Initialize(temp_dir,
                         std::filesystem::path("tcp://127.0.0.1:0"),
                         "NES",
                         &core,
                         &domains,
                         error,
                         rom_path)) {
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
  ok = ok && ExpectContains(handshake, "\"SYSTEM_RESPONSE\",\"value\":\"NES\"");
  ok = ok && ExpectContains(handshake, "\"PREFERRED_CORES_RESPONSE\"");
  ok = ok && ExpectContains(handshake, "\"HASH_RESPONSE\"");
  ok = ok && ExpectContains(handshake, "\"name\":\"System Bus\"");
  ok = ok && ExpectContains(handshake, "\"name\":\"PRG ROM\"");

  const auto prg_rom = transact("[{\"type\":\"READ\",\"domain\":\"PRG ROM\",\"address\":0,\"size\":3}]");
  ok = ok && ExpectContains(prg_rom, "\"READ_RESPONSE\",\"value\":\"TE9a\"");

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

  const auto read_error = transact("[{\"type\":\"READ\",\"domain\":\"IWRAM\",\"address\":999999,\"size\":1}]");
  ok = ok && ExpectContains(read_error, "\"type\":\"ERROR\"");
  ok = ok && ExpectContains(read_error, "\"err\":\"read_failed\"");

  server.Shutdown();
  g_split_gba_iwram = true;
  g_iwram[0x0BDE] = 0x42;
  retro_memory_descriptor gba_descriptors[2]{};
  gba_descriptors[0].ptr = g_system_ram.data();
  gba_descriptors[0].start = 0x02000000u;
  gba_descriptors[0].select = 0xFF000000u;
  gba_descriptors[0].disconnect = 0;
  gba_descriptors[0].len = 0x00040000u;
  gba_descriptors[0].addrspace = "System Bus";
  gba_descriptors[1].ptr = g_iwram.data();
  gba_descriptors[1].start = 0x03000000u;
  gba_descriptors[1].select = 0xFF000000u;
  gba_descriptors[1].disconnect = 0;
  gba_descriptors[1].len = g_iwram.size();
  gba_descriptors[1].addrspace = "System Bus";

  retro_memory_map gba_map{};
  gba_map.descriptors = gba_descriptors;
  gba_map.num_descriptors = 2;
  domains.SetMemoryMaps(&gba_map);

  if (!server.Initialize(temp_dir,
                         std::filesystem::path("tcp://127.0.0.1:0"),
                         "GBA",
                         &core,
                         &domains,
                         error,
                         std::nullopt)) {
    stop.store(true);
    poller.join();
    std::cerr << "gba_server_init_failed:" << error << "\n";
    return 1;
  }

  CloseSocket(client);
  client = kInvalidSocket;
  for (int attempt = 0; attempt < 100; ++attempt) {
    client = ConnectToEndpoint(server.SocketPath());
    if (client != kInvalidSocket) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (client == kInvalidSocket) {
    stop.store(true);
    poller.join();
    std::cerr << "gba_connect_failed\n";
    return 1;
  }

  const auto gba_domains = transact("[{\"type\":\"DOMAINS\"}]");
  ok = ok && ExpectContains(gba_domains, "\"name\":\"System Bus\"");
  ok = ok && ExpectContains(gba_domains, "\"name\":\"SRAM\"");

  const auto fusion_game_mode = transact(
      "[{\"type\":\"READ\",\"domain\":\"IWRAM\",\"address\":3038,\"size\":1}]");
  ok = ok && ExpectContains(fusion_game_mode, "\"READ_RESPONSE\",\"value\":\"Qg==\"");

  const auto gba_iwram_write = transact(
      "[{\"type\":\"WRITE\",\"domain\":\"IWRAM\",\"address\":3038,\"value\":\"Qw==\"}]");
  ok = ok && ExpectContains(gba_iwram_write, "\"WRITE_RESPONSE\"");
  ok = ok && g_iwram[0x0BDE] == 0x43;

  const auto fusion_sram_counter = transact(
      "[{\"type\":\"READ\",\"domain\":\"System Bus\",\"address\":235012094,\"size\":2}]");
  ok = ok && ExpectContains(fusion_sram_counter, "\"READ_RESPONSE\",\"value\":\"Vng=\"");

  const auto gba_sram_direct = transact(
      "[{\"type\":\"READ\",\"domain\":\"SRAM\",\"address\":131070,\"size\":2}]");
  ok = ok && ExpectContains(gba_sram_direct, "\"READ_RESPONSE\",\"value\":\"Vng=\"");

  const auto gba_sram_write = transact(
      "[{\"type\":\"WRITE\",\"domain\":\"System Bus\",\"address\":235012094,\"value\":\"mps=\"}]");
  ok = ok && ExpectContains(gba_sram_write, "\"WRITE_RESPONSE\"");
  ok = ok && g_save_ram[0x1FFFE] == 0x9A && g_save_ram[0x1FFFF] == 0x9B;

  server.Shutdown();
  g_save_ram.fill(0);
  g_reported_save_ram_size = 0x00008000u;
  g_save_ram[0x7FFE] = 0xA1;
  g_save_ram[0x7FFF] = 0xB2;
  domains.SetMemoryMaps(&gba_map);
  if (!server.Initialize(temp_dir,
                         std::filesystem::path("tcp://127.0.0.1:0"),
                         "GBA",
                         &core,
                         &domains,
                         error,
                         std::nullopt)) {
    stop.store(true);
    poller.join();
    std::cerr << "gba_mirrored_sram_server_init_failed:" << error << "\n";
    return 1;
  }
  CloseSocket(client);
  client = kInvalidSocket;
  for (int attempt = 0; attempt < 100; ++attempt) {
    client = ConnectToEndpoint(server.SocketPath());
    if (client != kInvalidSocket) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (client == kInvalidSocket) {
    stop.store(true);
    poller.join();
    std::cerr << "gba_mirrored_sram_connect_failed\n";
    return 1;
  }

  const auto gba_mirrored_sram_counter = transact(
      "[{\"type\":\"READ\",\"domain\":\"System Bus\",\"address\":235012094,\"size\":2}]");
  ok = ok && ExpectContains(gba_mirrored_sram_counter, "\"READ_RESPONSE\",\"value\":\"obI=\"");

  const auto gba_mirrored_sram_direct = transact(
      "[{\"type\":\"READ\",\"domain\":\"SRAM\",\"address\":131070,\"size\":2}]");
  ok = ok && ExpectContains(gba_mirrored_sram_direct, "\"READ_RESPONSE\",\"value\":\"obI=\"");

  const auto gba_mirrored_sram_write = transact(
      "[{\"type\":\"WRITE\",\"domain\":\"System Bus\",\"address\":235012094,\"value\":\"zN0=\"}]");
  ok = ok && ExpectContains(gba_mirrored_sram_write, "\"WRITE_RESPONSE\"");
  ok = ok && g_save_ram[0x7FFE] == 0xCC && g_save_ram[0x7FFF] == 0xDD;
  g_reported_save_ram_size = g_save_ram.size();

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
