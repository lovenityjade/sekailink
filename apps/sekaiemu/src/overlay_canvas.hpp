#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

struct UiColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 255;
};

class OverlayCanvas {
 public:
  OverlayCanvas(unsigned width, unsigned height);

  void Clear(UiColor color);
  void FillRect(int x, int y, int width, int height, UiColor color);
  void DrawRect(int x, int y, int width, int height, UiColor color);
  void DrawText(int x, int y, std::string_view text, UiColor color, int scale = 2);
  void DrawWrappedText(int x,
                       int y,
                       int width,
                       std::string_view text,
                       UiColor color,
                       int scale = 2,
                       int line_spacing = 4);
  void DrawImage(int x,
                 int y,
                 int width,
                 int height,
                 const std::uint8_t* rgba_pixels,
                 unsigned source_width,
                 unsigned source_height);
  void DrawImageWithColorKey(int x,
                             int y,
                             int width,
                             int height,
                             const std::uint8_t* rgba_pixels,
                             unsigned source_width,
                             unsigned source_height,
                             UiColor key_color,
                             UiColor replacement_color,
                             int tolerance = 0);

  unsigned Width() const { return width_; }
  unsigned Height() const { return height_; }
  const std::uint8_t* Data() const { return pixels_.data(); }

 private:
  void BlendPixel(int x, int y, UiColor color);
  void DrawGlyph(int x, int y, char character, UiColor color, int scale);
  static const std::uint8_t* GlyphRows(char character);
  static std::string NormalizeText(std::string_view text);

  unsigned width_ = 0;
  unsigned height_ = 0;
  std::vector<std::uint8_t> pixels_;
};

}  // namespace sekaiemu::spike
