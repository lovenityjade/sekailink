#include "runtime_menu_core_settings.hpp"

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

}  // namespace

void DrawRuntimeMenuCoreSettingsPage(OverlayCanvas& canvas,
                                     int list_x,
                                     int list_y,
                                     int list_width,
                                     int list_bottom,
                                     int selected_index,
                                     int scroll_offset,
                                     int& visible_rows,
                                     const CoreOptionManager& core_options,
                                     std::span<const RuntimeMenuCoreActionRow> action_rows,
                                     std::string& tooltip) {
  const int row_height = 20;
  const int label_width = static_cast<int>(list_width * 0.58f);
  const int value_x = list_x + label_width + 16;
  const int value_width = list_width - label_width - 28;
  visible_rows = std::max(1, (list_bottom - list_y) / row_height);
  const int option_count = static_cast<int>(core_options.DefinitionCount());
  const int total_rows = option_count + static_cast<int>(action_rows.size());
  const int end = std::min(total_rows, scroll_offset + visible_rows);
  for (int index = scroll_offset; index < end; ++index) {
    const bool selected = index == selected_index;
    const int y = list_y + (index - scroll_offset) * row_height;
    if (y + row_height > list_bottom) {
      break;
    }
    canvas.FillRect(list_x, y, list_width, row_height - 2,
                    selected ? UiColor{49, 70, 115, 255} : UiColor{24, 30, 42, 190});

    if (index < option_count) {
      const auto* definition = core_options.DefinitionAt(static_cast<std::size_t>(index));
      if (!definition) {
        continue;
      }
      auto label = definition->desc.empty() ? definition->key : definition->desc;
      if (definition->requires_restart) {
        label += " *";
      }
      canvas.DrawText(list_x + 8,
                      y + 6,
                      Truncate(label, static_cast<std::size_t>(std::max(8, label_width / 6))),
                      UiColor{255, 245, 225, 255},
                      1);
      const auto value = core_options.CurrentValueFor(definition->key);
      canvas.DrawText(value_x,
                      y + 6,
                      Truncate(value, static_cast<std::size_t>(std::max(6, value_width / 6))),
                      definition->requires_restart ? UiColor{255, 215, 150, 255}
                                                   : UiColor{170, 230, 180, 255},
                      1);
      if (selected) {
        tooltip = definition->info.empty() ? definition->key : definition->info;
        if (definition->requires_restart) {
          tooltip += " REQUIRES CORE RESTART TO FULLY APPLY.";
        }
      }
      continue;
    }

    const int action_index = index - option_count;
    const auto& action_row = action_rows[static_cast<std::size_t>(action_index)];
    canvas.DrawText(list_x + 8, y + 6, action_row.label, UiColor{255, 245, 225, 255}, 1);
    if (selected) {
      tooltip = action_row.tooltip;
    }
  }

  if (total_rows > visible_rows) {
    const int track_x = list_x + list_width - 8;
    const int track_height = visible_rows * row_height - 4;
    canvas.FillRect(track_x, list_y, 4, track_height, UiColor{40, 52, 74, 180});
    const float ratio = static_cast<float>(visible_rows) / static_cast<float>(total_rows);
    const int thumb_height = std::max(14, static_cast<int>(track_height * ratio));
    const float offset_ratio =
        total_rows <= visible_rows
            ? 0.0f
            : static_cast<float>(scroll_offset) / static_cast<float>(total_rows - visible_rows);
    const int thumb_y = list_y + static_cast<int>((track_height - thumb_height) * offset_ratio);
    canvas.FillRect(track_x, thumb_y, 4, thumb_height, UiColor{130, 150, 190, 255});
  }
}

}  // namespace sekaiemu::spike
