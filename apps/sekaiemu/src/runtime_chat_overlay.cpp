#include "runtime_chat_overlay.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace sekaiemu::spike {
namespace {

constexpr std::uint64_t kMessageTtlFrames = 60 * 12;
constexpr std::uint64_t kTypingRefreshFrames = 5;
constexpr std::size_t kMaxStoredMessages = 64;
constexpr std::size_t kMaxExternalMessageIds = 256;
constexpr std::size_t kMaxVisibleLines = 8;
constexpr std::size_t kMaxInputChars = 160;

std::uint8_t AlphaForAge(std::uint64_t age) {
  if (age >= kMessageTtlFrames) {
    return 0;
  }
  return 235;
}

std::string Trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  return value;
}

std::vector<std::string> WrapText(std::string_view text, std::size_t max_chars) {
  std::vector<std::string> lines;
  std::string current;
  std::string word;
  const auto flush_word = [&]() {
    if (word.empty()) {
      return;
    }
    if (word.size() > max_chars) {
      if (!current.empty()) {
        lines.push_back(current);
        current.clear();
      }
      for (std::size_t offset = 0; offset < word.size(); offset += max_chars) {
        lines.push_back(word.substr(offset, max_chars));
      }
      word.clear();
      return;
    }
    const std::size_t projected = current.size() + (current.empty() ? 0 : 1) + word.size();
    if (projected > max_chars && !current.empty()) {
      lines.push_back(current);
      current.clear();
    }
    if (!current.empty()) {
      current.push_back(' ');
    }
    current += word;
    word.clear();
  };

  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      flush_word();
    } else {
      word.push_back(ch);
    }
  }
  flush_word();
  if (!current.empty()) {
    lines.push_back(current);
  }
  if (lines.empty()) {
    lines.emplace_back();
  }
  return lines;
}

void DrawShadowedText(OverlayCanvas& canvas,
                      int x,
                      int y,
                      std::string_view text,
                      UiColor color,
                      int scale) {
  const UiColor shadow{0, 0, 0, color.a};
  canvas.DrawText(x + scale, y + scale, text, shadow, scale);
  canvas.DrawText(x, y, text, color, scale);
}

}  // namespace

void RuntimeChatOverlay::ToggleEnabled() {
  SetEnabled(!enabled_);
}

void RuntimeChatOverlay::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_) {
    typing_ = false;
    input_.clear();
  }
  dirty_ = true;
}

void RuntimeChatOverlay::MarkRendered(std::uint64_t frame) {
  last_render_frame_ = frame;
  last_rendered_visible_ = Visible(frame);
  dirty_ = false;
}

bool RuntimeChatOverlay::ShouldRender(std::uint64_t frame, std::uint64_t interval) const {
  (void)interval;
  const bool visible = Visible(frame);
  if (typing_ && dirty_ && last_render_frame_ != 0 && frame > last_render_frame_ &&
      frame - last_render_frame_ < kTypingRefreshFrames) {
    return false;
  }
  if (dirty_ || visible != last_rendered_visible_) {
    return true;
  }
  if (!visible) {
    return false;
  }
  if (last_render_frame_ == 0 || frame <= last_render_frame_) {
    return true;
  }
  return std::any_of(messages_.begin(), messages_.end(), [&](const Message& message) {
    const auto expiry_frame = message.frame + kMessageTtlFrames;
    return last_render_frame_ < expiry_frame && frame >= expiry_frame;
  });
}

bool RuntimeChatOverlay::Visible(std::uint64_t frame) const {
  if (!enabled_) {
    return false;
  }
  if (typing_) {
    return true;
  }
  return std::any_of(messages_.begin(), messages_.end(), [&](const Message& message) {
    return frame <= message.frame || frame - message.frame < kMessageTtlFrames;
  });
}

void RuntimeChatOverlay::BeginTyping(std::uint64_t) {
  if (!enabled_) {
    return;
  }
  typing_ = true;
  dirty_ = true;
}

void RuntimeChatOverlay::CancelTyping() {
  if (!typing_) {
    return;
  }
  typing_ = false;
  input_.clear();
  dirty_ = true;
}

void RuntimeChatOverlay::AppendText(std::string_view text) {
  if (!typing_) {
    return;
  }
  const auto sanitized = Sanitize(text, kMaxInputChars, false);
  if (sanitized.empty()) {
    return;
  }
  const auto available = kMaxInputChars > input_.size() ? kMaxInputChars - input_.size() : 0;
  input_.append(sanitized.substr(0, available));
  dirty_ = true;
}

void RuntimeChatOverlay::Backspace() {
  if (!typing_ || input_.empty()) {
    return;
  }
  input_.pop_back();
  dirty_ = true;
}

std::optional<std::string> RuntimeChatOverlay::Submit(std::uint64_t frame, std::string_view author) {
  if (!typing_) {
    return std::nullopt;
  }
  const auto text = Trim(input_);
  typing_ = false;
  input_.clear();
  dirty_ = true;
  if (text.empty()) {
    return std::nullopt;
  }
  last_submitted_author_ = Sanitize(author.empty() ? "ME" : author, 24);
  last_submitted_text_ = Sanitize(text, 400);
  last_submit_frame_ = frame;
  AddMessage(last_submitted_author_, last_submitted_text_, frame);
  return text;
}

void RuntimeChatOverlay::AddMessage(std::string_view author, std::string_view text, std::uint64_t frame) {
  const auto clean_text = Sanitize(text, 400);
  if (clean_text.empty()) {
    return;
  }
  messages_.push_back(Message{Sanitize(author.empty() ? "ME" : author, 24), clean_text, frame});
  while (messages_.size() > kMaxStoredMessages) {
    messages_.erase(messages_.begin());
  }
  dirty_ = true;
}

void RuntimeChatOverlay::AddExternalMessage(std::uint64_t id,
                                            std::string_view author,
                                            std::string_view text,
                                            std::uint64_t frame) {
  if (id != 0) {
    if (external_message_ids_.find(id) != external_message_ids_.end()) {
      return;
    }
    external_message_ids_.insert(id);
    external_message_order_.push_back(id);
    while (external_message_order_.size() > kMaxExternalMessageIds) {
      external_message_ids_.erase(external_message_order_.front());
      external_message_order_.pop_front();
    }
  }
  const auto clean_author = Sanitize(author.empty() ? "ROOM" : author, 24);
  const auto clean_text = Sanitize(text, 400);
  if (clean_text.empty()) {
    return;
  }
  const bool likely_echo = frame >= last_submit_frame_ &&
                           frame - last_submit_frame_ < 60 * 10 &&
                           clean_text == last_submitted_text_;
  if (likely_echo) {
    return;
  }
  AddMessage(clean_author, clean_text, frame);
}

void RuntimeChatOverlay::Render(OverlayCanvas& canvas,
                                std::uint64_t frame,
                                int game_area_width,
                                int game_area_height) const {
  if (!Visible(frame)) {
    return;
  }

  const int scale = game_area_width >= 900 ? 3 : 2;
  const int margin = std::max(8, game_area_width / 80);
  const int line_height = 8 * scale + 3;
  const int input_height = typing_ ? line_height + 9 : 0;
  const int max_width = std::max(180, std::min(game_area_width - margin * 2, game_area_width * 58 / 100));
  const std::size_t max_chars = static_cast<std::size_t>(std::max(12, (max_width - 14) / (6 * scale)));

  struct RenderLine {
    std::string text;
    std::uint8_t alpha = 255;
  };
  std::vector<RenderLine> lines;
  for (const auto& message : messages_) {
    const std::uint64_t age = frame > message.frame ? frame - message.frame : 0;
    const auto alpha = AlphaForAge(age);
    if (alpha == 0) {
      continue;
    }
    const auto wrapped = WrapText(message.author + ": " + message.text, max_chars);
    for (const auto& line : wrapped) {
      lines.push_back(RenderLine{line, alpha});
    }
  }
  if (lines.size() > kMaxVisibleLines) {
    lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(kMaxVisibleLines));
  }

  const int line_count = static_cast<int>(lines.size());
  int bottom = std::max(margin + input_height, game_area_height - margin);
  if (typing_) {
    const int input_y = bottom - input_height + 2;
    canvas.FillRect(margin, input_y, max_width, input_height - 2, UiColor{0, 0, 0, 185});
    canvas.DrawRect(margin, input_y, max_width, input_height - 2, UiColor{190, 205, 230, 210});
    const auto prompt = "> " + input_;
    DrawShadowedText(canvas,
                     margin + 7,
                     input_y + 6,
                     prompt.size() > max_chars ? prompt.substr(prompt.size() - max_chars) : prompt,
                     UiColor{245, 245, 245, 255},
                     scale);
    bottom = input_y - 3;
  }

  int y = bottom - line_count * line_height;
  for (const auto& line : lines) {
    const UiColor background{0, 0, 0, static_cast<std::uint8_t>(line.alpha * 145 / 255)};
    canvas.FillRect(margin, y - 2, max_width, line_height, background);
    DrawShadowedText(canvas, margin + 7, y + 2, line.text, UiColor{245, 245, 245, line.alpha}, scale);
    y += line_height;
  }
}

std::string RuntimeChatOverlay::Sanitize(std::string_view text, std::size_t max_size, bool trim) {
  std::string out;
  out.reserve(std::min(max_size, text.size()));
  for (unsigned char ch : text) {
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
  return trim ? Trim(out) : out;
}

}  // namespace sekaiemu::spike
