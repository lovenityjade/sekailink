#pragma once

#include "overlay_canvas.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {

class RuntimeChatOverlay {
 public:
  bool Enabled() const { return enabled_; }
  void ToggleEnabled();
  void SetEnabled(bool enabled);

  bool Typing() const { return typing_; }
  bool Dirty() const { return dirty_; }
  void MarkRendered(std::uint64_t frame);
  bool ShouldRender(std::uint64_t frame, std::uint64_t interval) const;
  bool Visible(std::uint64_t frame) const;

  void BeginTyping(std::uint64_t frame);
  void CancelTyping();
  void AppendText(std::string_view text);
  void Backspace();
  std::optional<std::string> Submit(std::uint64_t frame, std::string_view author);
  void AddMessage(std::string_view author, std::string_view text, std::uint64_t frame);
  void AddExternalMessage(std::uint64_t id,
                          std::string_view author,
                          std::string_view text,
                          std::uint64_t frame);

  void Render(OverlayCanvas& canvas,
              std::uint64_t frame,
              int game_area_width,
              int game_area_height) const;

 private:
  struct Message {
    std::string author;
    std::string text;
    std::uint64_t frame = 0;
  };

  static std::string Sanitize(std::string_view text, std::size_t max_size, bool trim = true);

  std::vector<Message> messages_;
  std::string input_;
  bool enabled_ = true;
  bool typing_ = false;
  bool dirty_ = true;
  bool last_rendered_visible_ = false;
  std::uint64_t last_render_frame_ = 0;
  std::deque<std::uint64_t> external_message_order_;
  std::unordered_set<std::uint64_t> external_message_ids_;
  std::uint64_t last_submit_frame_ = 0;
  std::string last_submitted_author_;
  std::string last_submitted_text_;
};

}  // namespace sekaiemu::spike
