#include "runtime_chat_bridge.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace sekaiemu::spike {
namespace {

std::uint64_t HashText(std::string_view text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hash == 0 ? 1 : hash;
}

std::string CleanText(std::string_view text, std::size_t max_size, bool trim = true) {
  std::string out;
  out.reserve(std::min(max_size, text.size()));
  for (const unsigned char ch : text) {
    if (ch == '\r' || ch == '\n' || ch == '\0') {
      continue;
    }
    if (ch < 32 || ch > 126) {
      out.push_back('?');
    } else {
      out.push_back(static_cast<char>(ch));
    }
    if (out.size() >= max_size) {
      break;
    }
  }
  if (!trim) {
    return out;
  }
  while (!out.empty() && out.front() == ' ') {
    out.erase(out.begin());
  }
  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

std::string JsonString(const nlohmann::json& value, std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto found = value.find(key);
    if (found != value.end() && found->is_string()) {
      return found->get<std::string>();
    }
  }
  return {};
}

std::uint64_t JsonId(const nlohmann::json& value, std::string_view fallback_line) {
  const auto id = value.find("id");
  if (id != value.end()) {
    if (id->is_number_unsigned()) {
      const auto numeric = id->get<std::uint64_t>();
      return numeric == 0 ? HashText(fallback_line) : numeric;
    }
    if (id->is_string()) {
      return HashText(id->get<std::string>());
    }
  }
  return HashText(fallback_line);
}

}  // namespace

void RuntimeChatBridge::Configure(std::filesystem::path inbox_path,
                                  std::filesystem::path outbox_path) {
  inbox_path_ = std::move(inbox_path);
  outbox_path_ = std::move(outbox_path);
  inbox_offset_ = 0;
  last_inbox_size_ = 0;
  last_poll_frame_ = 0;
  incoming_.clear();
  seen_id_order_.clear();
  seen_ids_.clear();
}

bool RuntimeChatBridge::Active() const {
  return !inbox_path_.empty() || !outbox_path_.empty();
}

bool RuntimeChatBridge::HasOutbox() const {
  return !outbox_path_.empty();
}

void RuntimeChatBridge::Tick(std::uint64_t frame) {
  if (inbox_path_.empty()) {
    return;
  }
  if (last_poll_frame_ != 0 && frame >= last_poll_frame_ &&
      frame - last_poll_frame_ < kPollFrameInterval) {
    return;
  }
  last_poll_frame_ = frame;
  PollInbox();
}

std::vector<RuntimeChatBridgeMessage> RuntimeChatBridge::DrainIncoming() {
  auto out = std::move(incoming_);
  incoming_.clear();
  return out;
}

bool RuntimeChatBridge::SendChat(std::string_view text) {
  if (outbox_path_.empty()) {
    return false;
  }
  const auto clean_text = CleanText(text, 400);
  if (clean_text.empty()) {
    return false;
  }
  try {
    const auto parent = outbox_path_.parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent);
    }
    std::ofstream output(outbox_path_, std::ios::app);
    if (!output) {
      return false;
    }
    output << nlohmann::json{{"cmd", "chat.say"}, {"text", clean_text}}.dump() << "\n";
  } catch (const std::exception& error) {
    std::cerr << "[sekaiemu] chat bridge send failed: " << error.what() << "\n";
    return false;
  }
  return true;
}

void RuntimeChatBridge::PollInbox() {
  std::error_code ec;
  const auto size = std::filesystem::file_size(inbox_path_, ec);
  if (ec) {
    return;
  }
  if (size < inbox_offset_) {
    inbox_offset_ = 0;
    last_inbox_size_ = 0;
  }
  if (size == last_inbox_size_) {
    return;
  }

  std::ifstream input(inbox_path_);
  if (!input) {
    return;
  }
  input.seekg(static_cast<std::streamoff>(inbox_offset_), std::ios::beg);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      const auto parsed = nlohmann::json::parse(line);
      if (!parsed.is_object()) {
        continue;
      }
      const auto id = JsonId(parsed, line);
      if (!RememberId(id)) {
        continue;
      }
      const auto text = CleanText(JsonString(parsed, {"text", "content", "body"}), 400);
      if (text.empty()) {
        continue;
      }
      const auto author = CleanText(JsonString(parsed, {"author", "display_name", "username", "name"}), 24);
      incoming_.push_back(RuntimeChatBridgeMessage{
          .id = id,
          .author = author.empty() ? "ROOM" : author,
          .text = text,
          .kind = CleanText(JsonString(parsed, {"kind", "type"}), 32),
      });
    } catch (const std::exception&) {
      continue;
    }
  }
  if (input.eof()) {
    inbox_offset_ = size;
    last_inbox_size_ = size;
  }
}

bool RuntimeChatBridge::RememberId(std::uint64_t id) {
  if (id == 0) {
    return true;
  }
  if (seen_ids_.find(id) != seen_ids_.end()) {
    return false;
  }
  seen_ids_.insert(id);
  seen_id_order_.push_back(id);
  while (seen_id_order_.size() > kMaxSeenIds) {
    seen_ids_.erase(seen_id_order_.front());
    seen_id_order_.pop_front();
  }
  return true;
}

}  // namespace sekaiemu::spike
