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
constexpr std::size_t kMaxAuthorChars = 48;
constexpr std::size_t kMaxSuggestionLines = 5;

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

std::string ToLowerAscii(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (unsigned char ch : text) {
    out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string CleanSuggestion(std::string_view text, std::size_t max_size) {
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
  return Trim(std::move(out));
}

std::vector<std::string> UniqueCleanSuggestions(std::vector<std::string> values,
                                                std::size_t max_value_size,
                                                std::size_t max_count) {
  std::vector<std::string> out;
  out.reserve(std::min(values.size(), max_count));
  for (const auto& value : values) {
    auto clean = CleanSuggestion(value, max_value_size);
    if (clean.empty()) {
      continue;
    }
    if (std::find(out.begin(), out.end(), clean) != out.end()) {
      continue;
    }
    out.push_back(std::move(clean));
    if (out.size() >= max_count) {
      break;
    }
  }
  return out;
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

std::string TruncateText(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 1) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 1)) + "~";
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

bool RuntimeChatOverlay::ApplyAutocomplete() {
  if (!typing_) {
    return false;
  }
  const auto suggestions = BuildInputSuggestions();
  if (suggestions.empty()) {
    return false;
  }
  const auto& suggestion = suggestions.front();
  if (StartsWith(input_, "/hint ")) {
    input_ = "/hint " + suggestion;
  } else if (suggestion == "/hint <itemname>") {
    input_ = "/hint ";
  } else {
    input_ = suggestion;
    if (input_ == "/hint") {
      input_ += ' ';
    }
  }
  dirty_ = true;
  return true;
}

void RuntimeChatOverlay::SetCommandSuggestions(std::vector<std::string> commands) {
  command_suggestions_ = UniqueCleanSuggestions(std::move(commands), 64, 16);
  if (command_suggestions_.empty()) {
    command_suggestions_ = {"/hint <itemname>", "/collect", "/remaining"};
  }
  dirty_ = true;
}

void RuntimeChatOverlay::SetItemNameSuggestions(std::vector<std::string> item_names) {
  item_name_suggestions_ = UniqueCleanSuggestions(std::move(item_names), 96, 512);
  dirty_ = true;
}

std::optional<std::string> RuntimeChatOverlay::Submit(std::uint64_t frame,
                                                      std::string_view author,
                                                      bool echo) {
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
  last_submitted_author_ = Sanitize(author.empty() ? "ME" : author, kMaxAuthorChars);
  last_submitted_text_ = Sanitize(text, 400);
  last_submit_frame_ = frame;
  if (echo) {
    AddMessage(last_submitted_author_, last_submitted_text_, frame);
  }
  return text;
}

void RuntimeChatOverlay::AddMessage(std::string_view author, std::string_view text, std::uint64_t frame) {
  const auto clean_text = Sanitize(text, 400);
  if (clean_text.empty()) {
    return;
  }
  messages_.push_back(Message{Sanitize(author.empty() ? "ME" : author, kMaxAuthorChars), clean_text, frame});
  while (messages_.size() > kMaxStoredMessages) {
    messages_.erase(messages_.begin());
  }
  dirty_ = true;
}

void RuntimeChatOverlay::AddExternalMessage(std::uint64_t id,
                                            std::string_view author,
                                            std::string_view text,
                                            std::string_view kind,
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
  const auto clean_author = Sanitize(author.empty() ? "ROOM" : author, kMaxAuthorChars);
  const auto clean_text = Sanitize(text, 400);
  if (clean_text.empty()) {
    return;
  }
  const bool system_like = kind == "system" || kind == "connection" || kind == "defeat";
  const bool likely_echo = frame >= last_submit_frame_ &&
                           frame - last_submit_frame_ < 60 * 10 &&
                           !system_like &&
                           clean_text == last_submitted_text_;
  if (likely_echo) {
    return;
  }
  AddMessage(clean_author, clean_text, frame);
}

std::vector<std::string> RuntimeChatOverlay::BuildInputSuggestions() const {
  if (input_.empty() || input_.front() != '/') {
    return {};
  }
  std::vector<std::string> suggestions;
  const auto commands = command_suggestions_.empty()
                            ? std::vector<std::string>{"/hint <itemname>", "/collect", "/remaining"}
                            : command_suggestions_;
  if (StartsWith(input_, "/hint ")) {
    const auto raw_query = input_.substr(6);
    const auto query = ToLowerAscii(raw_query);
    for (const auto& item : item_name_suggestions_) {
      const auto lowered = ToLowerAscii(item);
      if (query.empty() || lowered.find(query) != std::string::npos) {
        suggestions.push_back(item);
      }
      if (suggestions.size() >= kMaxSuggestionLines) {
        break;
      }
    }
    return suggestions;
  }

  const auto query = ToLowerAscii(input_);
  for (const auto& command : commands) {
    const auto lowered = ToLowerAscii(command);
    if (query == "/" || lowered.find(query) == 0) {
      suggestions.push_back(command);
    }
    if (suggestions.size() >= kMaxSuggestionLines) {
      break;
    }
  }
  return suggestions;
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
  const int max_width = std::max(180, std::min(game_area_width - margin * 2, game_area_width * 74 / 100));
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
    const auto suggestions = BuildInputSuggestions();
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
    if (!suggestions.empty()) {
      const int suggestion_height =
          static_cast<int>(suggestions.size()) * line_height + 8;
      const int suggestion_y = std::max(margin, input_y - suggestion_height - 4);
      canvas.FillRect(margin, suggestion_y, max_width, suggestion_height, UiColor{0, 0, 0, 205});
      canvas.DrawRect(margin, suggestion_y, max_width, suggestion_height, UiColor{120, 225, 210, 220});
      int suggestion_line_y = suggestion_y + 5;
      for (const auto& suggestion : suggestions) {
        DrawShadowedText(canvas,
                         margin + 8,
                         suggestion_line_y,
                         TruncateText(suggestion, max_chars),
                         UiColor{210, 255, 248, 245},
                         scale);
        suggestion_line_y += line_height;
      }
    }
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
