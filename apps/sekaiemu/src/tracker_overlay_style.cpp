#include "tracker_overlay_style.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace sekaiemu::spike {

std::string DisplayModeLabel(TrackerDisplayMode mode, bool compact) {
  if (!compact && mode != TrackerDisplayMode::ToggleScreen) {
    return std::string(ToString(mode));
  }
  switch (mode) {
    case TrackerDisplayMode::SplitScreen:
      return "SPLIT";
    case TrackerDisplayMode::SeparateWindow:
      return "WINDOW";
    case TrackerDisplayMode::PipOverlay:
      return "PIP";
    case TrackerDisplayMode::ToggleScreen:
      return "TOGGLE";
  }
  return "TRACK";
}

std::string BadgeText(const std::string& label, const std::string& fallback) {
  const auto& source = !label.empty() ? label : fallback;
  std::string text;
  for (unsigned char ch : source) {
    if (std::isalnum(ch) == 0) {
      continue;
    }
    text.push_back(static_cast<char>(ch));
    if (text.size() >= 2) {
      break;
    }
  }
  if (text.empty() && !fallback.empty()) {
    text = fallback.substr(0, std::min<std::size_t>(2, fallback.size()));
  }
  return text.empty() ? "?" : text;
}

UiColor ParseHexColor(std::string_view value, UiColor fallback) {
  if (value.size() != 7 || value.front() != '#') {
    return fallback;
  }
  auto decode = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
  };
  const int r0 = decode(value[1]);
  const int r1 = decode(value[2]);
  const int g0 = decode(value[3]);
  const int g1 = decode(value[4]);
  const int b0 = decode(value[5]);
  const int b1 = decode(value[6]);
  if (r0 < 0 || r1 < 0 || g0 < 0 || g1 < 0 || b0 < 0 || b1 < 0) {
    return fallback;
  }
  return UiColor{static_cast<std::uint8_t>(r0 * 16 + r1),
                 static_cast<std::uint8_t>(g0 * 16 + g1),
                 static_cast<std::uint8_t>(b0 * 16 + b1),
                 fallback.a};
}

void DrawSectionBox(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    int x,
                    int y,
                    int width,
                    int height) {
  canvas.FillRect(x, y, width, height, palette.section_background);
  canvas.DrawRect(x, y, width, height, palette.section_border);
}

void DrawSectionHeader(OverlayCanvas& canvas,
                       const UiPalette& palette,
                       int x,
                       int y,
                       int width,
                       std::string_view title) {
  canvas.FillRect(x, y, width, 18, palette.header_background);
  canvas.DrawText(x + 6, y + 5, title, palette.accent, 1);
}

void DrawChip(OverlayCanvas& canvas,
              const UiPalette& palette,
              int x,
              int y,
              std::string_view text,
              bool active) {
  const int width = std::max(48, static_cast<int>(text.size()) * 6 + 12);
  canvas.FillRect(x, y, width, 16, active ? palette.accent : palette.header_background);
  canvas.DrawRect(x, y, width, 16, active ? palette.warning : palette.section_border);
  canvas.DrawText(x + 6,
                  y + 5,
                  text,
                  active ? palette.panel_background : palette.text_secondary,
                  1);
}

void DrawChipRows(OverlayCanvas& canvas,
                  const UiPalette& palette,
                  const std::vector<std::string>& chips,
                  int x,
                  int y,
                  int width,
                  int max_rows,
                  int active_index) {
  int cursor_x = x;
  int cursor_y = y;
  int row = 0;
  for (std::size_t index = 0; index < chips.size(); ++index) {
    const auto text = TruncateText(chips[index], 18);
    const int chip_width = std::max(54, static_cast<int>(text.size()) * 6 + 12);
    if (cursor_x != x && cursor_x + chip_width > x + width) {
      ++row;
      if (row >= max_rows) {
        break;
      }
      cursor_x = x;
      cursor_y += 18;
    }
    DrawChip(canvas, palette, cursor_x, cursor_y, text, static_cast<int>(index) == active_index);
    cursor_x += chip_width + 6;
  }
}

void DrawMetricRow(OverlayCanvas& canvas,
                   const UiPalette& palette,
                   int x,
                   int y,
                   std::string_view label,
                   std::string_view value) {
  canvas.DrawText(x, y, label, palette.text_secondary, 1);
  canvas.DrawText(x + 76, y, value, palette.text_primary, 1);
}

void DrawProgressBar(OverlayCanvas& canvas,
                     const UiPalette& palette,
                     int x,
                     int y,
                     int width,
                     std::size_t value,
                     std::size_t maximum,
                     UiColor fill) {
  const std::size_t safe_maximum = std::max<std::size_t>(maximum, 1);
  const double ratio =
      std::clamp(static_cast<double>(value) / static_cast<double>(safe_maximum), 0.0, 1.0);
  canvas.FillRect(x, y, width, 8, palette.header_background);
  canvas.DrawRect(x, y, width, 8, palette.section_border);
  canvas.FillRect(x + 1, y + 1, std::max(1, static_cast<int>((width - 2) * ratio)), 6, fill);
}

void DrawItemBadge(OverlayCanvas& canvas,
                   const UiPalette& palette,
                   int x,
                   int y,
                   int size,
                   const BundleItemRenderMetadata& item,
                   bool received,
                   const TrackerOverlayAssetResolver* asset_resolver) {
  const UiColor fill = received ? UiColor{86, 214, 142, 255} : UiColor{48, 58, 72, 255};
  const UiColor border = received ? UiColor{190, 248, 184, 255} : palette.section_border;
  const UiColor text = received ? UiColor{4, 20, 12, 255} : palette.text_secondary;
  canvas.FillRect(x, y, size, size, fill);
  canvas.DrawRect(x, y, size, size, border);
  bool drew_icon = false;
  if (asset_resolver != nullptr && !item.icon.empty()) {
    const auto icon = asset_resolver->ResolveTrackerAsset(item.icon);
    if (icon.has_value() && icon->rgba_pixels != nullptr && icon->width > 0 && icon->height > 0) {
      canvas.DrawImage(x + 2,
                       y + 2,
                       std::max(1, size - 4),
                       std::max(1, size - 4),
                       icon->rgba_pixels,
                       icon->width,
                       icon->height);
      drew_icon = true;
    }
  }
  if (!drew_icon) {
    canvas.DrawText(x + (size >= 16 ? 3 : 1), y + (size >= 16 ? 4 : 3), BadgeText(item.abbreviation, item.id), text, 1);
  }
  if (!item.icon.empty()) {
    canvas.FillRect(x + size - 4, y + 2, 2, 2, received ? palette.panel_background : palette.accent_soft);
  }
  if (item.stage > 1 || item.count > 1) {
    const auto badge_text = std::to_string(std::max<std::int64_t>(item.stage, item.count));
    canvas.FillRect(x + size - 12, y + size - 10, 10, 8, UiColor{4, 20, 12, 220});
    canvas.DrawText(x + size - 10, y + size - 8, badge_text, palette.text_primary, 1);
  }
}

UiColor PinFillColor(std::string_view color, bool checked) {
  UiColor fill = checked ? UiColor{63, 63, 63, 255} : UiColor{255, 88, 88, 255};
  if (color == "green") {
    fill = UiColor{68, 255, 184, 255};
  } else if (color == "yellow") {
    fill = UiColor{255, 232, 56, 255};
  } else if (color == "blue") {
    fill = UiColor{90, 180, 255, 255};
  } else if (color == "black") {
    fill = UiColor{18, 18, 18, 255};
  } else if (color == "mixed") {
    fill = UiColor{255, 168, 56, 255};
  } else if (color == "red") {
    fill = UiColor{255, 88, 88, 255};
  }
  return fill;
}

UiColor PinBorderColor(std::string_view color, bool checked) {
  if (checked || color == "black") {
    return UiColor{238, 238, 238, 255};
  }
  return UiColor{0, 0, 0, 255};
}

void DrawPinSegment(OverlayCanvas& canvas,
                    int x,
                    int y,
                    int width,
                    int height,
                    const BundlePinSegmentRenderMetadata& segment) {
  const auto base_color = PinFillColor(segment.color, segment.checked);
  canvas.FillRect(x, y, width, height, base_color);
  if (segment.mixed && segment.checked_count > 0 && segment.total_count > 0) {
    const int checked_width =
        std::max(1, static_cast<int>(std::round(width * static_cast<double>(segment.checked_count) /
                                                static_cast<double>(segment.total_count))));
    canvas.FillRect(x, y, std::min(width, checked_width), height, PinFillColor("black", true));
  }
}

void DrawPinMarker(OverlayCanvas& canvas,
                   const UiPalette& palette,
                   int center_x,
                   int center_y,
                   const BundlePinRenderMetadata& pin,
                   bool checked) {
  const UiColor outline{0, 0, 0, 255};
  const auto fill = PinFillColor(pin.color, checked);
  const auto border = PinBorderColor(pin.color, checked);
  canvas.DrawRect(center_x - 6, center_y - 6, 13, 13, outline);
  if (!pin.segments.empty()) {
    const int left = center_x - 4;
    const int top = center_y - 4;
    if (pin.segments.size() == 1) {
      DrawPinSegment(canvas, left, top, 9, 9, pin.segments.front());
    } else if (pin.segments.size() == 2) {
      DrawPinSegment(canvas, left, top, 4, 9, pin.segments[0]);
      DrawPinSegment(canvas, left + 4, top, 5, 9, pin.segments[1]);
    } else if (pin.segments.size() == 3) {
      DrawPinSegment(canvas, left, top, 4, 9, pin.segments[0]);
      DrawPinSegment(canvas, left + 4, top, 5, 4, pin.segments[1]);
      DrawPinSegment(canvas, left + 4, top + 4, 5, 5, pin.segments[2]);
    } else {
      DrawPinSegment(canvas, left, top, 4, 4, pin.segments[0]);
      DrawPinSegment(canvas, left + 4, top, 5, 4, pin.segments[1]);
      DrawPinSegment(canvas, left, top + 4, 4, 5, pin.segments[2]);
      DrawPinSegment(canvas, left + 4, top + 4, 5, 5, pin.segments[3]);
    }
    if (pin.segments.size() > 1) {
      canvas.FillRect(left + 4, top, 1, 9, outline);
      if (pin.segments.size() > 2) {
        canvas.FillRect(left, top + 4, 9, 1, outline);
      }
    }
  } else {
    canvas.FillRect(center_x - 4, center_y - 4, 9, 9, fill);
  }
  canvas.DrawRect(center_x - 5, center_y - 5, 11, 11, border);
  canvas.FillRect(center_x - 2, center_y + 5, 5, 4, outline);
  canvas.FillRect(center_x - 1, center_y + 5, 3, 3, fill);
}

int ResolveMapCoordinate(double coordinate,
                         int target_origin,
                         int target_size,
                         unsigned source_size) {
  double normalized = coordinate;
  if (coordinate > 1.0 && coordinate <= 100.0) {
    normalized = coordinate / 100.0;
  } else if (coordinate > 100.0 && source_size > 0) {
    normalized = coordinate / static_cast<double>(source_size);
  }
  normalized = std::clamp(normalized, 0.0, 1.0);
  return target_origin + static_cast<int>(std::round(normalized * static_cast<double>(std::max(0, target_size - 1))));
}

}  // namespace sekaiemu::spike
