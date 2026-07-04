#include "activity_feed_window_presenter.hpp"

#include "window_chrome.hpp"

#include "opengl_loader.hpp"
#include "overlay_canvas.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <regex>
#include <string_view>

namespace sekaiemu::spike {
namespace {

struct FeedEntry {
  std::uint64_t id = 0;
  std::string kind;
  std::string title;
  std::string detail;
};

void RestoreGlContext(SDL_Window* window, SDL_GLContext context) {
  SDL_GL_MakeCurrent(context != nullptr ? window : nullptr, context);
}

std::string Trim(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.erase(text.begin());
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  return text;
}

bool IsFeedKind(std::string_view kind) {
  return kind == "item" || kind == "hint" || kind == "connection" || kind == "defeat";
}

std::string SnapshotString(const nlohmann::json& snapshot, std::initializer_list<std::string_view> keys) {
  if (!snapshot.is_object()) {
    return {};
  }
  for (const auto key : keys) {
    const auto found = snapshot.find(std::string(key));
    if (found != snapshot.end() && found->is_string()) {
      const auto value = Trim(found->get<std::string>());
      if (!value.empty()) {
        return value;
      }
    }
  }
  return {};
}

std::string JsonScalarToText(const nlohmann::json& value) {
  if (value.is_string()) {
    return Trim(value.get<std::string>());
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<std::int64_t>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<std::uint64_t>());
  }
  return {};
}

std::string SnapshotObjectValueText(const nlohmann::json& snapshot,
                                    std::string_view object_key,
                                    const std::string& key) {
  if (!snapshot.is_object() || key.empty()) {
    return {};
  }
  const auto object = snapshot.find(std::string(object_key));
  if (object == snapshot.end() || !object->is_object()) {
    return {};
  }
  const auto value = object->find(key);
  return value == object->end() ? std::string{} : JsonScalarToText(*value);
}

std::string PlayerName(const nlohmann::json& snapshot) {
  auto player = SnapshotString(snapshot, {"player_alias", "player_display_name", "slot_alias", "slot_name"});
  if (player.empty() && snapshot.contains("room_metadata")) {
    player = SnapshotString(snapshot["room_metadata"], {"player_alias", "player_display_name", "slot_alias", "slot_name"});
  }
  return player.empty() ? std::string{"Player"} : player;
}

std::uint64_t StableEntryId(std::string_view kind, std::string_view detail) {
  const auto mixed = std::hash<std::string>{}(std::string(kind) + "\x1f" + std::string(detail));
  return 3000000000000ULL + static_cast<std::uint64_t>(mixed % 900000000000ULL);
}

std::string HumanizeItemText(std::string_view raw_text, const nlohmann::json& snapshot) {
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

  auto game = SnapshotString(snapshot, {"game", "linkedworld_id", "world_id", "room_game"});
  if (game.empty()) {
    game = "this world";
  }

  if (std::regex_match(text, match, found_recipient_item)) {
    const auto sender = Trim(match[1].str());
    const auto recipient = Trim(match[2].str());
    const auto item = Trim(match[3].str());
    const auto location = match.size() > 4 ? Trim(match[4].str()) : std::string{};
    if (!sender.empty() && !recipient.empty() && !item.empty()) {
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
      return recipient + " got their " + item + " from Sekailink";
    }
  }

  if (std::regex_match(text, match, sent_item_with_game)) {
    const auto sender = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto recipient = Trim(match[3].str());
    const auto recipient_game = Trim(match[4].str());
    const auto location = match.size() > 5 ? Trim(match[5].str()) : std::string{};
    if (!sender.empty() && !item.empty() && !recipient.empty() && !recipient_game.empty()) {
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
      auto formatted = sender + " found their " + item;
      if (!location.empty()) {
        formatted += " in " + location;
      }
      return formatted;
    }
  }

  return text;
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

std::string HumanizeHintText(std::string_view raw_text, const nlohmann::json& snapshot) {
  const auto text = Trim(std::string(raw_text));
  if (text.empty()) {
    return {};
  }

  std::smatch match;
  static const std::regex player_item_at_location(
      R"(^(.+?)'s\s+(.+?)\s+is\s+at\s+(.+?)(?:\s+in\s+(.+?))?\.?$)",
      std::regex_constants::icase);
  auto game = SnapshotString(snapshot, {"game", "linkedworld_id", "world_id", "room_game"});
  if (game.empty()) {
    game = "this world";
  }

  if (std::regex_match(text, match, player_item_at_location)) {
    const auto player = Trim(match[1].str());
    const auto item = Trim(match[2].str());
    const auto location = Trim(match[3].str());
    const auto hinted_game = match.size() > 4 && !Trim(match[4].str()).empty() ? Trim(match[4].str()) : game;
    if (!player.empty() && !item.empty() && !location.empty()) {
      return player + " has their " + item + " in " + location + " (" + hinted_game + ")";
    }
  }
  return text;
}

UiColor AccentForKind(std::string_view kind) {
  if (kind == "item") {
    return {55, 235, 210, 255};
  }
  if (kind == "hint") {
    return {184, 158, 255, 255};
  }
  if (kind == "connection") {
    return {255, 176, 70, 255};
  }
  if (kind == "defeat") {
    return {255, 92, 92, 255};
  }
  return {180, 205, 214, 255};
}

std::vector<FeedEntry> CollectFeedEntries(const nlohmann::json& snapshot, std::size_t limit) {
  std::vector<FeedEntry> entries;
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
      if (!IsFeedKind(kind)) {
        continue;
      }
      FeedEntry entry;
      entry.id = message.value("id", 0ULL);
      entry.kind = kind;
      if (kind == "item") {
        entry.title = "Item";
        entry.detail = HumanizeItemText(message.value("text", ""), snapshot);
      } else if (kind == "hint") {
        entry.title = "Hint";
        entry.detail = HumanizeHintText(message.value("text", ""), snapshot);
      } else if (kind == "connection") {
        entry.title = "Connection";
        entry.detail = Trim(message.value("text", ""));
      } else if (kind == "defeat") {
        entry.title = "DeathLink";
        entry.detail = Trim(message.value("text", ""));
      }
      if (!entry.detail.empty() && !IsLegacyLowLevelActivity(entry.kind, entry.detail)) {
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

void DrawFeedPanel(OverlayCanvas& canvas, const std::vector<FeedEntry>& entries) {
  canvas.Clear({8, 15, 19, 255});
  canvas.FillRect(0, 0, static_cast<int>(canvas.Width()), 56, {12, 47, 62, 255});
  canvas.DrawRect(0, 0, static_cast<int>(canvas.Width()), static_cast<int>(canvas.Height()), {40, 150, 172, 255});
  canvas.DrawText(18, 17, "SekaiLink Activity", {235, 247, 249, 255}, 2);
  canvas.DrawText(static_cast<int>(canvas.Width()) - 84, 19, std::to_string(entries.size()), {55, 235, 210, 255}, 2);

  int y = 76;
  if (entries.empty()) {
    canvas.FillRect(18, y, static_cast<int>(canvas.Width()) - 36, 64, {19, 49, 64, 230});
    canvas.DrawRect(18, y, static_cast<int>(canvas.Width()) - 36, 64, {44, 112, 132, 255});
    canvas.DrawText(32, y + 18, "No activity yet.", {155, 191, 201, 255}, 2);
    return;
  }

  const int card_width = static_cast<int>(canvas.Width()) - 36;
  const int card_height = 88;
  const int max_cards = std::max(1, (static_cast<int>(canvas.Height()) - y - 20) / (card_height + 10));
  const std::size_t count = std::min<std::size_t>(entries.size(), static_cast<std::size_t>(max_cards));
  const std::size_t first = entries.size() - count;
  for (std::size_t offset = 0; offset < count; ++offset) {
    const auto& entry = entries[first + count - 1 - offset];
    const auto accent = AccentForKind(entry.kind);
    canvas.FillRect(18, y, card_width, card_height, {19, 49, 64, 230});
    canvas.DrawRect(18, y, card_width, card_height, {44, 112, 132, 255});
    canvas.FillRect(18, y, 4, card_height, accent);
    canvas.DrawText(32, y + 12, entry.title, accent, 2);
    canvas.DrawWrappedText(32, y + 38, card_width - 28, entry.detail, {226, 237, 240, 255}, 2, 3);
    y += card_height + 10;
  }
}

}  // namespace

ActivityFeedWindowPresenter::~ActivityFeedWindowPresenter() {
  Shutdown();
}

bool ActivityFeedWindowPresenter::EnsureWindow(unsigned width, unsigned height) {
  bool created_window = false;
  if (!window_) {
    window_ = SDL_CreateWindow("Sekaiemu Activity Feed",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               static_cast<int>(width),
                               static_cast<int>(height),
                               SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_BORDERLESS);
    if (!window_) {
      return false;
    }
    EnableBorderlessTitlebarDrag(window_);
    created_window = true;
  }
  if (!gl_context_) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
      return false;
    }
    auto* previous_window = SDL_GL_GetCurrentWindow();
    const auto previous_context = SDL_GL_GetCurrentContext();
    if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
      return false;
    }
    std::string error;
    if (!LoadOpenGlFunctions(error)) {
      std::cerr << "[sekaiemu-libretro-spike] activity feed OpenGL loader failed: " << error << "\n";
      RestoreGlContext(previous_window, previous_context);
      return false;
    }
    SDL_GL_SetSwapInterval(0);
    if (!context_logged_) {
      std::cerr << "[sekaiemu-libretro-spike] activity feed separate window active\n";
      context_logged_ = true;
    }
    RestoreGlContext(previous_window, previous_context);
  }
  if (created_window) {
    width_ = width;
    height_ = height;
  }
  return true;
}

void ActivityFeedWindowPresenter::Render(const nlohmann::json& snapshot) {
  constexpr unsigned kWindowWidth = 460;
  constexpr unsigned kWindowHeight = 560;
  if (!EnsureWindow(kWindowWidth, kWindowHeight)) {
    return;
  }
  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  const unsigned render_width =
      static_cast<unsigned>(std::max<int>(360, drawable_width > 0 ? drawable_width : kWindowWidth));
  const unsigned render_height =
      static_cast<unsigned>(std::max<int>(320, drawable_height > 0 ? drawable_height : kWindowHeight));

  OverlayCanvas canvas(render_width, render_height);
  DrawFeedPanel(canvas, CollectFeedEntries(snapshot, 32));
  pixels_.assign(canvas.Data(), canvas.Data() + static_cast<std::size_t>(canvas.Width()) * canvas.Height() * 4u);
  width_ = render_width;
  height_ = render_height;
  dirty_ = true;
}

void ActivityFeedWindowPresenter::Present() {
  if (!window_ || !gl_context_ || pixels_.empty() || !dirty_) {
    return;
  }
  auto* previous_window = SDL_GL_GetCurrentWindow();
  const auto previous_context = SDL_GL_GetCurrentContext();
  SDL_GL_MakeCurrent(window_, gl_context_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  glViewport(0, 0, std::max(1, drawable_width), std::max(1, drawable_height));
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  std::string error;
  renderer_.Draw(RETRO_HW_CONTEXT_OPENGL_CORE,
                 pixels_,
                 width_,
                 height_,
                 std::max(1, drawable_width),
                 std::max(1, drawable_height),
                 true,
                 error);
  SDL_GL_SwapWindow(window_);
  RestoreGlContext(previous_window, previous_context);
  dirty_ = false;
}

void ActivityFeedWindowPresenter::Shutdown() {
  auto* previous_window = SDL_GL_GetCurrentWindow();
  const auto previous_context = SDL_GL_GetCurrentContext();
  const bool previous_context_is_ours = previous_context == gl_context_;
  if (window_ && gl_context_) {
    SDL_GL_MakeCurrent(window_, gl_context_);
  }
  renderer_.Destroy();
  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  pixels_.clear();
  width_ = 0;
  height_ = 0;
  dirty_ = false;
  context_logged_ = false;
  if (!previous_context_is_ours) {
    RestoreGlContext(previous_window, previous_context);
  } else {
    RestoreGlContext(nullptr, nullptr);
  }
}

}  // namespace sekaiemu::spike
