#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike {

class BridgeIpc {
 public:
  BridgeIpc() = default;
  ~BridgeIpc();

  BridgeIpc(const BridgeIpc&) = delete;
  BridgeIpc& operator=(const BridgeIpc&) = delete;

  bool Initialize(const std::filesystem::path& bridge_root, std::string& error);
  void Shutdown();

  std::vector<std::string> DrainCommands();
  void PublishEvent(std::string_view line);

  const std::filesystem::path& SocketPath() const;

 private:
  void CloseClient(int fd);
  void AcceptClients();

  int server_fd_ = -1;
  std::filesystem::path socket_path_;
  std::vector<int> client_fds_;
  std::unordered_map<int, std::string> pending_buffers_;
};

}  // namespace sekaiemu::spike
