#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {

struct RuntimeChatBridgeMessage {
  std::uint64_t id = 0;
  std::string author;
  std::string text;
  std::string kind;
};

class RuntimeChatBridge {
 public:
  void Configure(std::filesystem::path inbox_path, std::filesystem::path outbox_path);
  bool Active() const;
  bool HasOutbox() const;
  void Tick(std::uint64_t frame);
  std::vector<RuntimeChatBridgeMessage> DrainIncoming();
  bool SendChat(std::string_view text);

 private:
  static constexpr std::uint64_t kPollFrameInterval = 15;
  static constexpr std::size_t kMaxSeenIds = 512;

  void PollInbox();
  bool RememberId(std::uint64_t id);

  std::filesystem::path inbox_path_;
  std::filesystem::path outbox_path_;
  std::uintmax_t inbox_offset_ = 0;
  std::uintmax_t last_inbox_size_ = 0;
  std::uint64_t last_poll_frame_ = 0;
  std::vector<RuntimeChatBridgeMessage> incoming_;
  std::deque<std::uint64_t> seen_id_order_;
  std::unordered_set<std::uint64_t> seen_ids_;
};

}  // namespace sekaiemu::spike
