#include "bridge_terminal_presenter.hpp"

#include "opengl_loader.hpp"
#include "overlay_canvas.hpp"
#include "tracker_overlay_render_state.hpp"
#include "tracker_runtime.hpp"

#include <SDL.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {
namespace {

void RestoreGlContext(SDL_Window* window, SDL_GLContext context) {
  SDL_GL_MakeCurrent(context != nullptr ? window : nullptr, context);
}

std::string Truncate(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
}

const nlohmann::json* JsonAt(const nlohmann::json& root, std::string_view key) {
  if (!root.is_object()) {
    return nullptr;
  }
  const auto found = root.find(std::string(key));
  return found == root.end() ? nullptr : &*found;
}

std::string JsonStringAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr) {
    return {};
  }
  if (value->is_string()) {
    return value->get<std::string>();
  }
  if (value->is_boolean()) {
    return value->get<bool>() ? "YES" : "NO";
  }
  if (value->is_number_integer()) {
    return std::to_string(value->get<long long>());
  }
  if (value->is_number_unsigned()) {
    return std::to_string(value->get<unsigned long long>());
  }
  return {};
}

std::string SnapshotOrMeta(const nlohmann::json& snapshot, std::string_view key) {
  auto value = JsonStringAt(snapshot, key);
  if (!value.empty()) {
    return value;
  }
  if (const auto* metadata = JsonAt(snapshot, "room_metadata"); metadata != nullptr && metadata->is_object()) {
    value = JsonStringAt(*metadata, key);
  }
  return value;
}

unsigned JsonUnsignedAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr) {
    return 0;
  }
  if (value->is_number_unsigned()) {
    return static_cast<unsigned>(value->get<unsigned long long>());
  }
  if (value->is_number_integer()) {
    return static_cast<unsigned>(std::max<long long>(0, value->get<long long>()));
  }
  return 0;
}

std::string EndpointLabel(const BridgeRuntimeStatus& status) {
  std::string endpoint = status.ap_host.empty() ? "LOCAL ROOM" : status.ap_host;
  if (status.ap_port != 0) {
    endpoint += ":" + std::to_string(status.ap_port);
  }
  if (!status.ap_path.empty() && status.ap_path != "/") {
    endpoint += status.ap_path;
  }
  return endpoint;
}

void AppendChatLines(const nlohmann::json& snapshot, std::vector<std::string>& lines) {
  const auto* messages = JsonAt(snapshot, "chat_messages");
  if (messages == nullptr || !messages->is_array()) {
    return;
  }
  const std::size_t start = messages->size() > 18 ? messages->size() - 18 : 0;
  for (std::size_t index = start; index < messages->size(); ++index) {
    const auto& message = (*messages)[index];
    if (!message.is_object()) {
      continue;
    }
    auto author = message.value("author", "ROOM");
    auto text = message.value("text", "");
    if (!text.empty()) {
      lines.push_back(author + ": " + text);
    }
  }
}

void AppendReceivedItemLines(const nlohmann::json& snapshot, std::vector<std::string>& lines) {
  const auto* items = JsonAt(snapshot, "received_items");
  if (items == nullptr || !items->is_array() || items->empty()) {
    return;
  }
  lines.push_back("");
  lines.push_back("ITEM SUMMARY");
  const std::size_t start = items->size() > 10 ? items->size() - 10 : 0;
  for (std::size_t index = start; index < items->size(); ++index) {
    const auto& item = (*items)[index];
    if (!item.is_object()) {
      continue;
    }
    const auto name = item.value("item_name", item.value("slot_id", std::string("UNKNOWN ITEM")));
    const unsigned count = JsonUnsignedAt(item, "count");
    const unsigned stage = JsonUnsignedAt(item, "stage");
    std::string suffix;
    if (count > 0) {
      suffix = " x" + std::to_string(count);
    } else if (stage > 0) {
      suffix = " +" + std::to_string(stage);
    }
    lines.push_back("ITEM: " + name + suffix);
  }
}

}  // namespace

BridgeTerminalPresenter::~BridgeTerminalPresenter() {
  Shutdown();
}

void BridgeTerminalPresenter::SetEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled_) {
    Shutdown();
  }
}

bool BridgeTerminalPresenter::EnsureWindow(unsigned width, unsigned height) {
  if (!enabled_) {
    return false;
  }
  if (!window_) {
    window_ = SDL_CreateWindow("SekaiLink Bridge Terminal",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               static_cast<int>(width),
                               static_cast<int>(height),
                               SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) {
      return false;
    }
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
      RestoreGlContext(previous_window, previous_context);
      return false;
    }
    SDL_GL_SetSwapInterval(0);
    RestoreGlContext(previous_window, previous_context);
  }
  return true;
}

void BridgeTerminalPresenter::Render(const BridgeRuntimeStatus& bridge_status,
                                     const TrackerRuntime* tracker_runtime) {
  constexpr unsigned kWindowWidth = 820;
  constexpr unsigned kWindowHeight = 560;
  if (!EnsureWindow(kWindowWidth, kWindowHeight)) {
    return;
  }

  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  const unsigned render_width =
      static_cast<unsigned>(std::max<int>(520, drawable_width > 0 ? drawable_width : kWindowWidth));
  const unsigned render_height =
      static_cast<unsigned>(std::max<int>(360, drawable_height > 0 ? drawable_height : kWindowHeight));

  const nlohmann::json snapshot = tracker_runtime != nullptr
                                      ? tracker_runtime->AuthoritativeState().snapshot
                                      : nlohmann::json::object();
  const bool connected = SnapshotOrMeta(snapshot, "connected") == "YES" ||
                         SnapshotOrMeta(snapshot, "connected") == "1" ||
                         SnapshotOrMeta(snapshot, "ap_connected") == "YES";
  const auto summary = JsonAt(snapshot, "summary");
  const unsigned checked = summary != nullptr ? JsonUnsignedAt(*summary, "checked") : 0;
  const unsigned total = summary != nullptr ? JsonUnsignedAt(*summary, "total") : 0;

  OverlayCanvas canvas(render_width, render_height);
  canvas.Clear({6, 8, 12, 255});
  canvas.FillRect(0, 0, static_cast<int>(render_width), 52, UiColor{18, 29, 44, 255});
  canvas.DrawText(16, 15, "SEKAILINK BRIDGE TERMINAL", UiColor{255, 215, 130, 255}, 2);
  canvas.DrawText(static_cast<int>(render_width) - 128,
                  18,
                  connected ? "CONNECTED" : "WAITING",
                  connected ? UiColor{170, 230, 180, 255} : UiColor{255, 215, 150, 255},
                  1);

  const int info_y = 68;
  const std::string player = tracker_runtime != nullptr
                                 ? SnapshotDisplayPlayerName(*tracker_runtime, bridge_status.ap_slot_name)
                                 : bridge_status.ap_slot_name;
  const std::string game = SnapshotOrMeta(snapshot, "game").empty()
                               ? bridge_status.game_name
                               : SnapshotOrMeta(snapshot, "game");
  const std::string players = SnapshotOrMeta(snapshot, "player_count").empty()
                                  ? "1 known"
                                  : SnapshotOrMeta(snapshot, "player_count");
  const std::string sync_id = SnapshotOrMeta(snapshot, "seed_name").empty()
                                  ? SnapshotOrMeta(snapshot, "seed")
                                  : SnapshotOrMeta(snapshot, "seed_name");

  canvas.DrawText(16, info_y, "ROOM  " + Truncate(EndpointLabel(bridge_status), 72),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(16, info_y + 18, "PLAYER  " + Truncate(player.empty() ? "-" : player, 36),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(300, info_y + 18, "GAME  " + Truncate(game.empty() ? "-" : game, 36),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(16, info_y + 36, "PLAYERS  " + players,
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(300, info_y + 36,
                  "CHECKS  " + std::to_string(checked) + "/" + std::to_string(total),
                  UiColor{180, 205, 255, 255},
                  1);
  canvas.DrawText(16, info_y + 54, "SYNC  " + Truncate(sync_id.empty() ? "-" : sync_id, 80),
                  UiColor{180, 205, 255, 255}, 1);

  const int log_x = 16;
  const int log_y = 150;
  const int log_width = static_cast<int>(render_width) - 32;
  const int log_height = static_cast<int>(render_height) - log_y - 16;
  canvas.FillRect(log_x, log_y, log_width, log_height, UiColor{10, 14, 22, 255});
  canvas.DrawRect(log_x, log_y, log_width, log_height, UiColor{65, 85, 120, 255});
  canvas.DrawText(log_x + 10, log_y + 10, "ROOM EVENTS", UiColor{255, 245, 225, 255}, 1);

  std::vector<std::string> lines;
  AppendChatLines(snapshot, lines);
  AppendReceivedItemLines(snapshot, lines);
  if (lines.empty()) {
    lines.push_back("WAITING FOR ROOM EVENTS...");
  }

  const int line_height = 15;
  const int max_lines = std::max(1, (log_height - 36) / line_height);
  const std::size_t start = lines.size() > static_cast<std::size_t>(max_lines)
                                ? lines.size() - static_cast<std::size_t>(max_lines)
                                : 0;
  int y = log_y + 30;
  for (std::size_t index = start; index < lines.size(); ++index) {
    canvas.DrawText(log_x + 10,
                    y,
                    Truncate(lines[index], static_cast<std::size_t>(std::max(20, (log_width - 20) / 6))),
                    lines[index].starts_with("ITEM") ? UiColor{170, 230, 180, 255}
                                                     : UiColor{205, 215, 235, 255},
                    1);
    y += line_height;
  }

  pixels_.assign(canvas.Data(),
                 canvas.Data() + static_cast<std::size_t>(canvas.Width()) * canvas.Height() * 4u);
  width_ = render_width;
  height_ = render_height;
  dirty_ = true;
}

void BridgeTerminalPresenter::Present() {
  if (!enabled_ || !window_ || !gl_context_ || pixels_.empty() || !dirty_) {
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

void BridgeTerminalPresenter::Shutdown() {
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
  enabled_ = false;
  if (!previous_context_is_ours) {
    RestoreGlContext(previous_window, previous_context);
  } else {
    RestoreGlContext(nullptr, nullptr);
  }
}

}  // namespace sekaiemu::spike
