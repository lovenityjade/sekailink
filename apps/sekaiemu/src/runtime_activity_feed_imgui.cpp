#include "runtime_activity_feed_imgui.hpp"

#include "runtime_menu.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {
namespace {

struct ActivityEntry {
  std::uint64_t id = 0;
  std::string kind;
  std::string title;
  std::string detail;
  std::string sender;
  std::string item;
  std::string recipient;
  std::string recipient_game;
  std::string location;
  bool admin_grant = false;
};

struct ToastEntry {
  std::uint64_t id = 0;
  std::string text;
  double created_at = 0.0;
};

std::string Trim(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.erase(text.begin());
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  return text;
}

std::string FirstSnapshotString(const nlohmann::json& snapshot,
                                std::initializer_list<std::string_view> keys,
                                std::string_view fallback) {
  if (snapshot.is_object()) {
    for (const auto key : keys) {
      const auto found = snapshot.find(std::string(key));
      if (found != snapshot.end() && found->is_string()) {
        const auto value = Trim(found->get<std::string>());
        if (!value.empty()) {
          return value;
        }
      }
    }
  }
  return std::string(fallback);
}

bool IsActivityKind(std::string_view kind) {
  return kind == "item" || kind == "queued-item" || kind == "connection" || kind == "defeat" || kind == "hint";
}

bool IsLegacyLowLevelActivity(std::string_view kind, std::string_view detail) {
  const auto text = Trim(std::string(detail));
  if (text.empty()) {
    return true;
  }
  if (kind == "check") {
    return true;
  }
  std::smatch match;
  static const std::regex received_their(R"(^player\s+received\s+their\s+.+$)",
                                         std::regex_constants::icase);
  static const std::regex checked_location(R"(^player\s+checked\s+.+$)", std::regex_constants::icase);
  return std::regex_match(text, match, received_their) || std::regex_match(text, match, checked_location);
}

std::string Lower(std::string_view text) {
  std::string output(text);
  std::transform(output.begin(), output.end(), output.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return output;
}

std::vector<std::string> JsonStringArray(const nlohmann::json& root, std::string_view key) {
  const auto found = root.is_object() ? root.find(std::string(key)) : root.end();
  if (found == root.end() || !found->is_array()) {
    return {};
  }
  std::vector<std::string> values;
  values.reserve(found->size());
  for (const auto& value : *found) {
    if (value.is_string()) {
      const auto clean = Trim(value.get<std::string>());
      if (!clean.empty()) {
        values.push_back(clean);
      }
    }
  }
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

std::string SnapshotScalarText(const nlohmann::json& snapshot,
                               std::initializer_list<std::string_view> keys) {
  if (!snapshot.is_object()) {
    return {};
  }
  for (const auto key : keys) {
    const auto found = snapshot.find(std::string(key));
    if (found == snapshot.end()) {
      continue;
    }
    if (found->is_string()) {
      const auto value = Trim(found->get<std::string>());
      if (!value.empty()) {
        return value;
      }
    }
    if (found->is_number_integer() || found->is_number_unsigned()) {
      return std::to_string(found->get<std::int64_t>());
    }
  }
  return {};
}

std::string HumanizeItemText(std::string_view raw_text,
                             const nlohmann::json& snapshot,
                             ActivityEntry* entry) {
  const auto text = Trim(std::string(raw_text));
  if (text.empty()) {
    return {};
  }

  std::smatch match;
  static const std::regex found_recipient_item(
      R"(^(.+?)\s+found\s+(.+?)'s\s+(.+?)(?:\s+\((.+)\))?\.?$)",
      std::regex_constants::icase);
  static const std::regex found_their_item(
      R"(^(.+?)\s+found\s+their\s+(.+?)(?:\s+\((.+)\))?\.?$)",
      std::regex_constants::icase);
  static const std::regex sent_item(
      R"(^(.+?)\s+sent\s+(.+?)\s+to\s+(.+?)(?:\s+\((.+)\))?\.?$)",
      std::regex_constants::icase);
  static const std::regex sent_item_with_game(
      R"(^(.+?)\s+sent\s+(.+?)\s+to\s+(.+?)'s\s+(.+?)(?:\s+\((.+)\))?\.?$)",
      std::regex_constants::icase);
  static const std::regex admin_sent_item(
      R"(^Cheat\s+console:\s+sending\s+\"?(.+?)\"?\s+to\s+(.+?)\.?$)",
      std::regex_constants::icase);

  const auto game = FirstSnapshotString(snapshot,
                                        {"game", "linkedworld_id", "world_id", "room_game"},
                                        "this world");
  if (std::regex_match(text, match, found_recipient_item)) {
    const auto sender = Trim(match[1].str());
    const auto recipient = Trim(match[2].str());
    const auto item = Trim(match[3].str());
    const auto location = match.size() > 4 ? Trim(match[4].str()) : std::string{};
    if (!sender.empty() && !recipient.empty() && !item.empty()) {
      if (entry != nullptr) {
        entry->sender = sender;
        entry->recipient = recipient;
        entry->item = item;
        entry->recipient_game = game;
        entry->location = location;
      }
      auto formatted = sender + " sent " + item + " to " + recipient + "'s " + game;
      if (!location.empty()) {
        formatted += " (" + location + ")";
      }
      return formatted;
    }
  }

  if (std::regex_match(text, match, admin_sent_item)) {
    const auto item = Trim(match[1].str());
    const auto recipient = Trim(match[2].str());
    if (!item.empty() && !recipient.empty()) {
      if (entry != nullptr) {
        entry->sender = "Sekailink";
        entry->recipient = recipient;
        entry->item = item;
        entry->recipient_game = game;
        entry->admin_grant = true;
      }
      return recipient + " got " + item + " from Sekailink";
    }
  }

  if (std::regex_match(text, match, sent_item_with_game)) {
    const auto sender = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto recipient = Trim(match[3].str());
    const auto recipient_game = Trim(match[4].str());
    const auto location = match.size() > 5 ? Trim(match[5].str()) : std::string{};
    if (!sender.empty() && !item.empty() && !recipient.empty() && !recipient_game.empty()) {
      if (entry != nullptr) {
        entry->sender = sender;
        entry->recipient = recipient;
        entry->item = item;
        entry->recipient_game = recipient_game;
        entry->location = location;
      }
      auto formatted = sender + " sent " + item + " to " + recipient + "'s " + recipient_game;
      if (!location.empty()) {
        formatted += " (" + location + ")";
      }
      return formatted;
    }
  }

  if (std::regex_match(text, match, sent_item)) {
    const auto sender = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto recipient = Trim(match[3].str());
    const auto location = match.size() > 4 ? Trim(match[4].str()) : std::string{};
    if (!sender.empty() && !item.empty() && !recipient.empty()) {
      if (entry != nullptr) {
        entry->sender = sender;
        entry->recipient = recipient;
        entry->item = item;
        entry->recipient_game = game;
        entry->location = location;
      }
      auto formatted = sender + " sent " + item + " to " + recipient + "'s " + game;
      if (!location.empty()) {
        formatted += " (" + location + ")";
      }
      return formatted;
    }
  }

  if (std::regex_match(text, match, found_their_item)) {
    const auto sender = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto location = match.size() > 3 ? Trim(match[3].str()) : std::string{};
    if (!sender.empty() && !item.empty()) {
      if (entry != nullptr) {
        entry->sender = sender;
        entry->recipient = sender;
        entry->item = item;
        entry->recipient_game = game;
        entry->location = location;
      }
      auto formatted = sender + " found their " + item;
      if (!location.empty()) {
        formatted += " in " + location;
      }
      return formatted;
    }
  }

  return text;
}

std::string HumanizeHintText(std::string_view raw_text,
                             const nlohmann::json& snapshot,
                             ActivityEntry* entry) {
  const auto text = Trim(std::string(raw_text));
  if (text.empty()) {
    return {};
  }

  std::smatch match;
  static const std::regex player_item_at_owner_location(
      R"(^(.+?)'s\s+(.+?)\s+is\s+at\s+(.+?)'s\s+(.+?)(?:\s+in\s+(.+?))?\.?$)",
      std::regex_constants::icase);
  static const std::regex player_item_at_location(
      R"(^(.+?)'s\s+(.+?)\s+is\s+at\s+(.+?)(?:\s+in\s+(.+?))?\.?$)",
      std::regex_constants::icase);
  const auto fallback_game = FirstSnapshotString(snapshot,
                                                 {"game", "linkedworld_id", "world_id", "room_game"},
                                                 "this world");

  if (std::regex_match(text, match, player_item_at_owner_location)) {
    const auto player = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto location = Trim(match[4].str());
    const auto game = match.size() > 5 && !Trim(match[5].str()).empty()
                          ? Trim(match[5].str())
                          : fallback_game;
    if (!player.empty() && !item.empty() && !location.empty()) {
      if (entry != nullptr) {
        entry->recipient = player;
        entry->item = item;
        entry->location = location;
        entry->recipient_game = game;
      }
      return player + " has their " + item + " in " + location + " (" + game + ")";
    }
  }

  if (std::regex_match(text, match, player_item_at_location)) {
    const auto player = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto location = Trim(match[3].str());
    const auto game = match.size() > 4 && !Trim(match[4].str()).empty()
                          ? Trim(match[4].str())
                          : fallback_game;
    if (!player.empty() && !item.empty() && !location.empty()) {
      if (entry != nullptr) {
        entry->recipient = player;
        entry->item = item;
        entry->location = location;
        entry->recipient_game = game;
      }
      return player + " has their " + item + " in " + location + " (" + game + ")";
    }
  }

  return text;
}

ActivityEntry MakeActivityEntry(const nlohmann::json& message, const nlohmann::json& snapshot) {
  ActivityEntry entry;
  entry.id = message.value("id", 0ULL);
  entry.kind = message.value("kind", "system");
  auto text = message.value("text", "");
  if (entry.kind == "item" || entry.kind == "queued-item") {
    text = HumanizeItemText(text, snapshot, &entry);
    entry.title = entry.kind == "queued-item" ? "Received item" : "Item transfer";
  } else if (entry.kind == "connection") {
    entry.title = "Connection";
  } else if (entry.kind == "defeat") {
    entry.title = "DeathLink";
  } else if (entry.kind == "hint") {
    text = HumanizeHintText(text, snapshot, &entry);
    entry.title = "Hint";
  } else {
    entry.title = "Activity";
  }
  entry.detail = Trim(std::move(text));
  return entry;
}

std::string CompactToastText(const ActivityEntry& entry) {
  if ((entry.kind == "item" || entry.kind == "queued-item") && !entry.sender.empty() && !entry.item.empty()) {
    const auto recipient = entry.recipient.empty() ? std::string{"?"} : entry.recipient;
    const auto game = entry.recipient_game.empty() ? std::string{"?"} : entry.recipient_game;
    if (entry.admin_grant) {
      return "Sekailink -> " + entry.item + " (" + recipient + " - " + game + ")";
    }
    return entry.sender + " -> " + entry.item + " (" + recipient + " - " + game + ")";
  }
  if (entry.kind == "hint") {
    return entry.detail;
  }
  return entry.detail;
}

std::vector<ActivityEntry> CollectActivityEntries(const nlohmann::json& snapshot, std::size_t limit) {
  std::vector<ActivityEntry> entries;
  if (!snapshot.is_object()) {
    return entries;
  }
  const auto messages = snapshot.find("chat_messages");
  if (messages != snapshot.end() && messages->is_array()) {
    for (const auto& message : *messages) {
      if (!message.is_object()) {
        continue;
      }
      const auto kind = message.value("kind", "chat");
      if (!IsActivityKind(kind)) {
        continue;
      }
      auto entry = MakeActivityEntry(message, snapshot);
      if (!entry.detail.empty() && !IsLegacyLowLevelActivity(entry.kind, entry.detail)) {
        entries.push_back(std::move(entry));
      }
    }
  }
  const auto pending_received = snapshot.find("pending_received_items");
  if (pending_received != snapshot.end() && pending_received->is_array()) {
    for (const auto& item : *pending_received) {
      if (!item.is_object()) {
        continue;
      }
      const auto text = item.value("text", std::string{});
      if (text.empty()) {
        continue;
      }
      nlohmann::json message{
          {"id", item.value("id", 0ULL)},
          {"kind", "queued-item"},
          {"text", text},
          {"author", "SEKAILINK"},
      };
      if (item.contains("key")) {
        message["key"] = item["key"];
      }
      auto entry = MakeActivityEntry(message, snapshot);
      if (!entry.detail.empty()) {
        entries.push_back(std::move(entry));
      }
    }
  }
  std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  entries.erase(std::unique(entries.begin(),
                            entries.end(),
                            [](const auto& left, const auto& right) {
                              return left.kind == right.kind && left.detail == right.detail;
                            }),
                entries.end());
  if (entries.size() > limit) {
    entries.erase(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(entries.size() - limit));
  }
  return entries;
}

ImVec4 ActivityAccent(std::string_view kind) {
  if (kind == "item") {
    return ImVec4(0.22f, 0.92f, 0.82f, 1.0f);
  }
  if (kind == "connection") {
    return ImVec4(1.0f, 0.69f, 0.27f, 1.0f);
  }
  if (kind == "defeat") {
    return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
  }
  if (kind == "hint") {
    return ImVec4(0.72f, 0.62f, 1.0f, 1.0f);
  }
  return ImVec4(0.72f, 0.62f, 1.0f, 1.0f);
}

struct QueuedReceivedItem {
  std::uint64_t id = 0;
  std::string text;
  double created_at = 0.0;
};

void ApplySekailinkActivityStyle() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.047f, 0.058f, 0.92f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.82f, 0.78f, 0.55f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.96f, 0.96f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.58f, 0.62f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
}

void PopSekailinkActivityStyle() {
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(4);
}

std::vector<ToastEntry>& ToastQueue() {
  static std::vector<ToastEntry> queue;
  return queue;
}

std::unordered_set<std::string>& SeenToastSignatures() {
  static std::unordered_set<std::string> seen;
  return seen;
}

std::uint64_t& UnreadNotificationCount() {
  static std::uint64_t count = 0;
  return count;
}

std::uint64_t NextSyntheticToastId() {
  static std::uint64_t next_id = 1000000000ULL;
  return next_id++;
}

std::vector<QueuedReceivedItem>& ReceivedItemQueue() {
  static std::vector<QueuedReceivedItem> queue;
  return queue;
}

std::unordered_set<std::string>& SeenReceivedItemQueueSignatures() {
  static std::unordered_set<std::string> seen;
  return seen;
}

double& ReceivedItemsModalOpenedAt() {
  static double opened_at = 0.0;
  return opened_at;
}

bool& ReceivedItemsModalVisible() {
  static bool visible = true;
  return visible;
}

bool& ReceivedItemsModalInitialized() {
  static bool initialized = false;
  return initialized;
}

bool ReceivedItemsModalSyncPending() {
  constexpr double kSyncGraceSeconds = 3.0;
  return ReceivedItemsModalVisible() && ReceivedItemQueue().empty() &&
         (ImGui::GetTime() - ReceivedItemsModalOpenedAt()) < kSyncGraceSeconds;
}

void PushLocalTestNotification() {
  ToastQueue().push_back(ToastEntry{
      NextSyntheticToastId(),
      "Test notification",
      ImGui::GetTime(),
  });
}

void UpdateToastQueue(const std::vector<ActivityEntry>& entries, bool game_loaded) {
  auto& queue = ToastQueue();
  auto& unread = UnreadNotificationCount();
  auto& seen = SeenToastSignatures();
  const double now = ImGui::GetTime();
  if (game_loaded) {
    unread = 0;
  }
  for (const auto& entry : entries) {
    if ((entry.kind != "item" && entry.kind != "queued-item" && entry.kind != "hint") || entry.detail.empty()) {
      continue;
    }
    const auto signature = entry.kind + "\x1f" + entry.detail;
    if (!seen.insert(signature).second) {
      continue;
    }
    if (game_loaded) {
      queue.push_back(ToastEntry{entry.id, CompactToastText(entry), now});
    } else {
      ++unread;
    }
  }
  queue.erase(std::remove_if(queue.begin(),
                             queue.end(),
                             [now](const ToastEntry& toast) {
                               return now - toast.created_at > 4.2;
                             }),
              queue.end());
  if (queue.size() > 5) {
    queue.erase(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(queue.size() - 5));
  }
}

void UpdateReceivedItemQueue(const std::vector<ActivityEntry>& entries) {
  auto& initialized = ReceivedItemsModalInitialized();
  auto& visible = ReceivedItemsModalVisible();
  if (!initialized) {
    initialized = true;
    visible = true;
    ReceivedItemsModalOpenedAt() = ImGui::GetTime();
  }
  const bool collecting_return_items = visible;
  auto& queue = ReceivedItemQueue();
  auto& seen = SeenReceivedItemQueueSignatures();
  const double now = ImGui::GetTime();
  for (const auto& entry : entries) {
    if (entry.kind != "queued-item" || entry.detail.empty()) {
      continue;
    }
    const auto signature = entry.kind + "\x1f" + entry.detail;
    if (!seen.insert(signature).second) {
      continue;
    }
    if (collecting_return_items) {
      queue.push_back(QueuedReceivedItem{entry.id, entry.detail, now});
    }
  }
}

void DrawReceivedItemsModal() {
  if (!ReceivedItemsModalVisible()) {
    return;
  }

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(display, ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.62f);
  if (ImGui::Begin("##sekailink-received-items-dim",
                   nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {
  }
  ImGui::End();

  const float width = std::clamp(display.x * 0.54f, 420.0f, 680.0f);
  const float height = std::clamp(display.y * 0.50f, 300.0f, 520.0f);
  ImGui::SetNextWindowPos(ImVec2((display.x - width) * 0.5f, (display.y - height) * 0.5f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.98f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.090f, 0.115f, 0.98f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.82f, 0.78f, 0.85f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.145f, 0.185f, 0.96f));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.74f, 0.84f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.84f, 0.93f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.54f, 0.62f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 22.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
  if (ImGui::Begin("While you were gone...",
                   nullptr,
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::SetWindowFontScale(1.08f);
    ImGui::TextColored(ImVec4(0.92f, 0.97f, 0.98f, 1.0f), "While you were gone...");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Sekailink caught up with the room server.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float button_height = 42.0f;
    const float list_height = std::max(104.0f, ImGui::GetContentRegionAvail().y - button_height - 24.0f);
    ImGui::BeginChild("##received-items-list", ImVec2(0.0f, list_height), true);
    const auto& queue = ReceivedItemQueue();
    if (queue.empty()) {
      ImGui::SetWindowFontScale(1.02f);
      if (ReceivedItemsModalSyncPending()) {
        ImGui::TextDisabled("Checking received items...");
      } else {
        ImGui::TextDisabled("No item received.");
      }
      ImGui::SetWindowFontScale(1.0f);
    } else {
      const double now = ImGui::GetTime();
      for (const auto& item : queue) {
        const float alpha = std::clamp(static_cast<float>((now - item.created_at) / 0.35), 0.18f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.98f, 0.98f, alpha));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 10.0f);
        ImGui::BulletText("%s", item.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Spacing();
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 6.0f) {
        ImGui::SetScrollHereY(1.0f);
      }
    }
    ImGui::EndChild();

    const float button_width = 190.0f;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
    const bool can_continue = !ReceivedItemsModalSyncPending();
    if (!can_continue) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(can_continue ? "Continue Playing" : "Checking...", ImVec2(button_width, button_height))) {
      ReceivedItemsModalVisible() = false;
      ReceivedItemQueue().clear();
    }
    if (!can_continue) {
      ImGui::EndDisabled();
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(6);
}

void DrawToast(const ToastEntry& toast, std::size_t stack_index) {
  const double age = ImGui::GetTime() - toast.created_at;
  if (age < 0.0 || age > 4.2) {
    return;
  }
  float alpha = 1.0f;
  if (age > 3.0) {
    alpha = std::max(0.0f, 1.0f - static_cast<float>((age - 3.0) / 1.2));
  }
  const float slide = age < 0.38 ? static_cast<float>(age / 0.38) : 1.0f;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  const float width = std::clamp(display.x * 0.30f, 280.0f, 360.0f);
  const float height = 42.0f;
  const float target_x = std::max(16.0f, display.x - width - 18.0f);
  const float start_x = display.x + 24.0f;
  const float x = start_x + ((target_x - start_x) * slide);
  const float target_y =
      display.y - 18.0f - height - static_cast<float>(stack_index) * (height + 8.0f);
  const float y = std::max(18.0f, target_y);

  ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.78f * alpha);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.92f, 0.82f, 0.36f * alpha));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.97f, 0.98f, alpha));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(11.0f, 7.0f));
  const auto name = "##sekailink-toast-" + std::to_string(toast.id);
  if (ImGui::Begin(name.c_str(),
                   nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
    ImGui::SetWindowFontScale(0.88f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 22.0f);
    ImGui::TextUnformatted(toast.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetWindowFontScale(1.0f);
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void DrawBellIcon(std::uint64_t unread_count) {
  if (unread_count == 0) {
    return;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(display.x - 78.0f, 18.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(58.0f, 42.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.76f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
  if (ImGui::Begin("##sekailink-notification-bell",
                   nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
    ImGui::TextColored(ImVec4(0.22f, 0.92f, 0.82f, 1.0f), "!");
    ImGui::SameLine();
    ImGui::TextUnformatted(std::to_string(unread_count).c_str());
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

bool& HintPopupOpen() {
  static bool open = false;
  return open;
}

std::string& HintSearchText() {
  static std::string value;
  return value;
}

int& HintSelectedIndex() {
  static int index = 0;
  return index;
}

std::vector<std::string> FilterHintItems(const nlohmann::json& snapshot, std::string_view filter) {
  const auto items = JsonStringArray(snapshot, "hint_item_names");
  const auto lowered_filter = Lower(filter);
  if (lowered_filter.empty()) {
    return items;
  }
  std::vector<std::string> filtered;
  for (const auto& item : items) {
    if (Lower(item).find(lowered_filter) != std::string::npos) {
      filtered.push_back(item);
    }
  }
  return filtered;
}

std::string HintPointSummary(const nlohmann::json& snapshot) {
  const auto points = SnapshotScalarText(snapshot, {"hint_points", "hintPoints", "points"});
  const auto cost = SnapshotScalarText(snapshot, {"hint_cost", "hintCost"});
  const auto check_points = SnapshotScalarText(snapshot, {"location_check_points", "locationCheckPoints"});
  std::ostringstream out;
  out << "Hint points: " << (points.empty() ? "unknown" : points);
  if (!cost.empty()) {
    out << "  Cost: " << cost;
  }
  if (!check_points.empty()) {
    out << "  Check points: " << check_points;
  }
  return out.str();
}

void DrawHintPopup(const TrackerRuntime* tracker_runtime,
                   const std::function<void(std::string_view)>& send_chat_command) {
  auto& popup_open = HintPopupOpen();
  if (!popup_open || tracker_runtime == nullptr) {
    return;
  }

  ImGui::OpenPopup("Hint");
  const auto& snapshot = tracker_runtime->AuthoritativeState().snapshot;
  auto& search = HintSearchText();
  auto& selected = HintSelectedIndex();
  auto filtered = FilterHintItems(snapshot, search);
  selected = filtered.empty() ? 0 : std::clamp(selected, 0, static_cast<int>(filtered.size()) - 1);

  ImGui::SetNextWindowSize(ImVec2(520.0f, 520.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Hint", &popup_open, ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::TextColored(ImVec4(0.22f, 0.92f, 0.82f, 1.0f), "Request a hint");
    ImGui::TextDisabled("%s", HintPointSummary(snapshot).c_str());
    ImGui::Separator();

    char buffer[160]{};
    std::snprintf(buffer, sizeof(buffer), "%s", search.c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##hint-search", "Search item...", buffer, sizeof(buffer))) {
      search = buffer;
      selected = 0;
    }

    ImGui::BeginChild("hint-items", ImVec2(0.0f, 340.0f), true);
    if (filtered.empty()) {
      ImGui::TextDisabled("No matching items.");
    }
    for (int index = 0; index < static_cast<int>(filtered.size()); ++index) {
      const bool is_selected = index == selected;
      if (ImGui::Selectable(filtered[static_cast<std::size_t>(index)].c_str(), is_selected)) {
        selected = index;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndChild();

    const bool can_send = !filtered.empty() && send_chat_command;
    if (ImGui::Button("Ask Hint", ImVec2(150.0f, 0.0f)) && can_send) {
      const auto& item = filtered[static_cast<std::size_t>(selected)];
      send_chat_command("/hint " + item);
      popup_open = false;
      search.clear();
      selected = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
      popup_open = false;
    }
    if (can_send && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
      const auto& item = filtered[static_cast<std::size_t>(selected)];
      send_chat_command("/hint " + item);
      popup_open = false;
      search.clear();
      selected = 0;
    }
    ImGui::EndPopup();
  }
}

}  // namespace

void RenderRuntimeActivityFeedImGui(const TrackerRuntime& tracker_runtime) {
  // Runtime-social surfaces are owned by Client Core. Keep this path inert so
  // item/feed bursts cannot affect emulator frame pacing.
  (void)tracker_runtime;
}

bool RuntimeReceivedItemsModalOpen() {
  return false;
}

void RenderRuntimeContextMenuImGui(RuntimeMenu& runtime_menu,
                                   const TrackerRuntime* tracker_runtime,
                                   const std::function<void(std::string_view)>& send_chat_command) {
  ApplySekailinkActivityStyle();
  PopSekailinkActivityStyle();

  (void)tracker_runtime;
  (void)send_chat_command;
  if (runtime_menu.Visible()) {
    return;
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup("sekaiemu-context-menu");
  }

  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.035f, 0.047f, 0.058f, 0.98f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.82f, 0.78f, 0.70f));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
  if (ImGui::BeginPopup("sekaiemu-context-menu")) {
    if (ImGui::MenuItem("Test Notification")) {
      PushLocalTestNotification();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Game")) {
      runtime_menu.QueueAction(RuntimeMenuAction::ResetCore);
    }
    if (ImGui::MenuItem("Save State")) {
      runtime_menu.QueueAction(RuntimeMenuAction::SaveState);
    }
    if (ImGui::MenuItem("Change Game")) {
      runtime_menu.QueueAction(RuntimeMenuAction::ChangeGame);
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

}  // namespace sekaiemu::spike
