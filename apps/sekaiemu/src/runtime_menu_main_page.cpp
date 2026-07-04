#include "runtime_menu_main_page.hpp"

#include <algorithm>
#include <string_view>

namespace sekaiemu::spike {
namespace {

std::string Truncate(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
}

void DrawSlotCard(OverlayCanvas& canvas,
                  const SaveStateSlotMenuInfo& slot,
                  int x,
                  int y,
                  int width,
                  int height,
                  bool highlighted) {
  const UiColor border = highlighted ? UiColor{255, 215, 130, 255} : UiColor{65, 85, 120, 255};
  canvas.FillRect(x, y, width, height, highlighted ? UiColor{34, 42, 58, 230} : UiColor{16, 22, 34, 210});
  canvas.DrawRect(x, y, width, height, border);
  const int shot_w = std::max(56, std::min(112, width / 3));
  const int shot_h = std::max(42, height - 12);
  const int shot_x = x + 6;
  const int shot_y = y + 6;
  canvas.FillRect(shot_x, shot_y, shot_w, shot_h, UiColor{4, 6, 10, 255});
  canvas.DrawRect(shot_x, shot_y, shot_w, shot_h, UiColor{42, 54, 70, 255});
  if (slot.has_screenshot && !slot.screenshot_rgba.empty()) {
    canvas.DrawImage(shot_x + 1,
                     shot_y + 1,
                     shot_w - 2,
                     shot_h - 2,
                     slot.screenshot_rgba.data(),
                     slot.screenshot_width,
                     slot.screenshot_height);
  } else {
    canvas.DrawText(shot_x + 10, shot_y + shot_h / 2 - 4, "EMPTY", UiColor{100, 120, 150, 255}, 1);
  }
  const int text_x = shot_x + shot_w + 10;
  const int text_width = std::max(8, (x + width - text_x - 8) / 6);
  canvas.DrawText(text_x, y + 8, Truncate(slot.label, static_cast<std::size_t>(text_width)),
                  UiColor{255, 245, 225, 255}, 1);
  canvas.DrawText(text_x,
                  y + 22,
                  slot.has_state ? "READY" : "EMPTY",
                  slot.has_state ? UiColor{170, 230, 180, 255} : UiColor{150, 160, 178, 255},
                  1);
  canvas.DrawText(text_x,
                  y + 36,
                  Truncate(slot.completion.empty() ? "-" : slot.completion, static_cast<std::size_t>(text_width)),
                  UiColor{180, 205, 255, 255},
                  1);
  canvas.DrawText(text_x,
                  y + 50,
                  Truncate(slot.created_at.empty() ? "-" : slot.created_at, static_cast<std::size_t>(text_width)),
                  UiColor{150, 170, 205, 255},
                  1);
}

}  // namespace

void DrawRuntimeMenuMainPage(OverlayCanvas& canvas,
                             int list_x,
                             int list_y,
                             int list_width,
                             int list_bottom,
                             int selected_index,
                             std::span<const RuntimeMenuMainActionRow> rows,
                             std::span<const SaveStateSlotMenuInfo> save_slots,
                             int highlighted_slot,
                             std::string& tooltip) {
  const int action_width = std::max(300, list_width * 38 / 100);
  const int slots_x = list_x + action_width + 16;
  const int slots_width = std::max(360, list_width - action_width - 16);
  const int row_height = 26;

  canvas.DrawText(list_x + 4, list_y, "SELECTION", UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(slots_x + 4, list_y, "SAVE STATES", UiColor{180, 205, 255, 255}, 1);

  const int content_y = list_y + 20;
  for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
    const bool selected = index == selected_index;
    const int y = content_y + index * row_height;
    if (y + row_height > list_bottom) {
      break;
    }
    canvas.FillRect(list_x, y, action_width, row_height - 2,
                    selected ? UiColor{49, 70, 115, 255} : UiColor{24, 30, 42, 190});
    canvas.DrawText(list_x + 12, y + 6, rows[static_cast<std::size_t>(index)].label,
                    UiColor{255, 245, 225, 255}, 2);
    if (selected) {
      tooltip = rows[static_cast<std::size_t>(index)].tooltip;
    }
  }

  const int card_gap = 8;
  const int card_count = std::max(1, static_cast<int>(save_slots.size()));
  const int card_height = std::max(62, (list_bottom - content_y - card_gap * (card_count - 1)) / card_count);
  for (int index = 0; index < static_cast<int>(save_slots.size()); ++index) {
    const int y = content_y + index * (card_height + card_gap);
    if (y + card_height > list_bottom) {
      break;
    }
    const auto& slot = save_slots[static_cast<std::size_t>(index)];
    DrawSlotCard(canvas, slot, slots_x, y, slots_width, card_height, highlighted_slot == slot.slot_index);
  }
}

}  // namespace sekaiemu::spike
