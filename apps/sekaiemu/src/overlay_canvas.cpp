#include "overlay_canvas.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace sekaiemu::spike {

namespace {

constexpr std::uint8_t kBlank[7] = {0, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t kSpace[7] = {0, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t kDash[7] = {0, 0, 0, 0x1F, 0, 0, 0};
constexpr std::uint8_t kColon[7] = {0, 0x04, 0, 0, 0x04, 0, 0};
constexpr std::uint8_t kDot[7] = {0, 0, 0, 0, 0, 0x0C, 0x0C};
constexpr std::uint8_t kSlash[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0, 0};
constexpr std::uint8_t kOpenParen[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
constexpr std::uint8_t kCloseParen[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
constexpr std::uint8_t kPercent[7] = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
constexpr std::uint8_t kUnderscore[7] = {0, 0, 0, 0, 0, 0, 0x1F};
constexpr std::uint8_t kPlus[7] = {0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0};
constexpr std::uint8_t kComma[7] = {0, 0, 0, 0, 0, 0x04, 0x08};
constexpr std::uint8_t kOpenBracket[7] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E};
constexpr std::uint8_t kCloseBracket[7] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E};
constexpr std::uint8_t kQuestion[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0, 0x04};
constexpr std::uint8_t kExclamation[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04};
constexpr std::uint8_t kDoubleQuote[7] = {0x0A, 0x0A, 0x0A, 0, 0, 0, 0};
constexpr std::uint8_t kHash[7] = {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A};
constexpr std::uint8_t kDollar[7] = {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04};
constexpr std::uint8_t kAmpersand[7] = {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D};
constexpr std::uint8_t kApostrophe[7] = {0x04, 0x04, 0x08, 0, 0, 0, 0};
constexpr std::uint8_t kStar[7] = {0, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0};
constexpr std::uint8_t kSemicolon[7] = {0, 0x04, 0, 0, 0x04, 0x04, 0x08};
constexpr std::uint8_t kLess[7] = {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02};
constexpr std::uint8_t kEqual[7] = {0, 0, 0x1F, 0, 0x1F, 0, 0};
constexpr std::uint8_t kGreater[7] = {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08};
constexpr std::uint8_t kAt[7] = {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0F};
constexpr std::uint8_t kBackslash[7] = {0x10, 0x08, 0x04, 0x02, 0x01, 0, 0};
constexpr std::uint8_t kCaret[7] = {0x04, 0x0A, 0x11, 0, 0, 0, 0};
constexpr std::uint8_t kBacktick[7] = {0x08, 0x04, 0x02, 0, 0, 0, 0};
constexpr std::uint8_t kOpenBrace[7] = {0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02};
constexpr std::uint8_t kPipe[7] = {0x04, 0x04, 0x04, 0, 0x04, 0x04, 0x04};
constexpr std::uint8_t kCloseBrace[7] = {0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08};
constexpr std::uint8_t kTilde[7] = {0, 0, 0x08, 0x15, 0x02, 0, 0};

constexpr std::uint8_t k0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
constexpr std::uint8_t k1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
constexpr std::uint8_t k2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
constexpr std::uint8_t k3[7] = {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E};
constexpr std::uint8_t k4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
constexpr std::uint8_t k5[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
constexpr std::uint8_t k6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
constexpr std::uint8_t k7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
constexpr std::uint8_t k8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
constexpr std::uint8_t k9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};

constexpr std::uint8_t kA[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
constexpr std::uint8_t kB[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
constexpr std::uint8_t kC[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
constexpr std::uint8_t kD[7] = {0x1E, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1E};
constexpr std::uint8_t kE[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
constexpr std::uint8_t kF[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
constexpr std::uint8_t kG[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
constexpr std::uint8_t kH[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
constexpr std::uint8_t kI[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
constexpr std::uint8_t kJ[7] = {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
constexpr std::uint8_t kK[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
constexpr std::uint8_t kL[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
constexpr std::uint8_t kM[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
constexpr std::uint8_t kN[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
constexpr std::uint8_t kO[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
constexpr std::uint8_t kP[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
constexpr std::uint8_t kQ[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
constexpr std::uint8_t kR[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
constexpr std::uint8_t kS[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
constexpr std::uint8_t kT[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
constexpr std::uint8_t kU[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
constexpr std::uint8_t kV[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
constexpr std::uint8_t kW[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
constexpr std::uint8_t kX[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
constexpr std::uint8_t kY[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
constexpr std::uint8_t kZ[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

bool IsWithinTolerance(std::uint8_t value, std::uint8_t expected, int tolerance) {
  const int diff = static_cast<int>(value) - static_cast<int>(expected);
  return diff >= -tolerance && diff <= tolerance;
}

bool MatchesColorKey(const std::uint8_t* pixel, UiColor key_color, int tolerance) {
  return IsWithinTolerance(pixel[0], key_color.r, tolerance) &&
         IsWithinTolerance(pixel[1], key_color.g, tolerance) &&
         IsWithinTolerance(pixel[2], key_color.b, tolerance);
}

}

OverlayCanvas::OverlayCanvas(unsigned width, unsigned height)
    : width_(width),
      height_(height),
      pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0) {}

void OverlayCanvas::Clear(UiColor color) {
  for (std::size_t index = 0; index < pixels_.size(); index += 4) {
    pixels_[index + 0] = color.r;
    pixels_[index + 1] = color.g;
    pixels_[index + 2] = color.b;
    pixels_[index + 3] = color.a;
  }
}

void OverlayCanvas::FillRect(int x, int y, int width, int height, UiColor color) {
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      BlendPixel(x + col, y + row, color);
    }
  }
}

void OverlayCanvas::DrawRect(int x, int y, int width, int height, UiColor color) {
  FillRect(x, y, width, 1, color);
  FillRect(x, y + height - 1, width, 1, color);
  FillRect(x, y, 1, height, color);
  FillRect(x + width - 1, y, 1, height, color);
}

void OverlayCanvas::DrawText(int x, int y, std::string_view text, UiColor color, int scale) {
  const auto normalized = NormalizeText(text);
  int cursor_x = x;
  for (char character : normalized) {
    DrawGlyph(cursor_x, y, character, color, scale);
    cursor_x += 6 * scale;
  }
}

void OverlayCanvas::DrawWrappedText(int x,
                                    int y,
                                    int width,
                                    std::string_view text,
                                    UiColor color,
                                    int scale,
                                    int line_spacing) {
  const auto normalized = NormalizeText(text);
  std::string line;
  int max_chars = std::max(1, width / (6 * scale));
  int line_index = 0;

  auto flush_line = [&]() {
    if (!line.empty()) {
      DrawText(x, y + line_index * (8 * scale + line_spacing), line, color, scale);
      line.clear();
      ++line_index;
    }
  };

  std::stringstream stream(normalized);
  std::string word;
  while (stream >> word) {
    const int projected = static_cast<int>(line.size() + (line.empty() ? 0 : 1) + word.size());
    if (projected > max_chars) {
      flush_line();
    }
    if (!line.empty()) {
      line += ' ';
    }
    line += word;
  }
  flush_line();
}

void OverlayCanvas::DrawImage(int x,
                              int y,
                              int width,
                              int height,
                              const std::uint8_t* rgba_pixels,
                              unsigned source_width,
                              unsigned source_height) {
  if (!rgba_pixels || source_width == 0 || source_height == 0 || width <= 0 || height <= 0) {
    return;
  }

  for (int destination_y = 0; destination_y < height; ++destination_y) {
    const unsigned source_y = std::min<unsigned>(
        (static_cast<unsigned long long>(destination_y) * source_height) / static_cast<unsigned>(height),
        source_height - 1);
    for (int destination_x = 0; destination_x < width; ++destination_x) {
      const unsigned source_x = std::min<unsigned>(
          (static_cast<unsigned long long>(destination_x) * source_width) / static_cast<unsigned>(width),
          source_width - 1);
      const std::size_t source_offset =
          (static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source_width) +
           static_cast<std::size_t>(source_x)) *
          4U;
      BlendPixel(x + destination_x,
                 y + destination_y,
                 UiColor{rgba_pixels[source_offset + 0],
                         rgba_pixels[source_offset + 1],
                         rgba_pixels[source_offset + 2],
                         rgba_pixels[source_offset + 3]});
    }
  }
}

void OverlayCanvas::DrawImageWithColorKey(int x,
                                          int y,
                                          int width,
                                          int height,
                                          const std::uint8_t* rgba_pixels,
                                          unsigned source_width,
                                          unsigned source_height,
                                          UiColor key_color,
                                          UiColor replacement_color,
                                          int tolerance) {
  if (!rgba_pixels || source_width == 0 || source_height == 0 || width <= 0 || height <= 0) {
    return;
  }

  tolerance = std::max(0, tolerance);
  for (int destination_y = 0; destination_y < height; ++destination_y) {
    const unsigned source_y = std::min<unsigned>(
        (static_cast<unsigned long long>(destination_y) * source_height) / static_cast<unsigned>(height),
        source_height - 1);
    for (int destination_x = 0; destination_x < width; ++destination_x) {
      const unsigned source_x = std::min<unsigned>(
          (static_cast<unsigned long long>(destination_x) * source_width) / static_cast<unsigned>(width),
          source_width - 1);
      const std::size_t source_offset =
          (static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source_width) +
           static_cast<std::size_t>(source_x)) *
          4U;
      const auto* source_pixel = rgba_pixels + source_offset;
      const UiColor color = MatchesColorKey(source_pixel, key_color, tolerance)
                                ? replacement_color
                                : UiColor{source_pixel[0], source_pixel[1], source_pixel[2], source_pixel[3]};
      BlendPixel(x + destination_x, y + destination_y, color);
    }
  }
}

void OverlayCanvas::BlendPixel(int x, int y, UiColor color) {
  if (x < 0 || y < 0 || x >= static_cast<int>(width_) || y >= static_cast<int>(height_)) {
    return;
  }

  const std::size_t offset =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)) * 4u;
  const auto alpha = static_cast<std::uint16_t>(color.a);
  const auto inv_alpha = static_cast<std::uint16_t>(255 - color.a);

  pixels_[offset + 0] =
      static_cast<std::uint8_t>((color.r * alpha + pixels_[offset + 0] * inv_alpha) / 255);
  pixels_[offset + 1] =
      static_cast<std::uint8_t>((color.g * alpha + pixels_[offset + 1] * inv_alpha) / 255);
  pixels_[offset + 2] =
      static_cast<std::uint8_t>((color.b * alpha + pixels_[offset + 2] * inv_alpha) / 255);
  pixels_[offset + 3] = static_cast<std::uint8_t>(
      std::min<unsigned>(255, alpha + (pixels_[offset + 3] * inv_alpha) / 255));
}

void OverlayCanvas::DrawGlyph(int x, int y, char character, UiColor color, int scale) {
  const auto* rows = GlyphRows(character);
  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if ((rows[row] & (1u << (4 - col))) == 0) {
        continue;
      }
      FillRect(x + col * scale, y + row * scale, scale, scale, color);
    }
  }
}

const std::uint8_t* OverlayCanvas::GlyphRows(char character) {
  switch (character) {
    case '0': return k0;
    case '1': return k1;
    case '2': return k2;
    case '3': return k3;
    case '4': return k4;
    case '5': return k5;
    case '6': return k6;
    case '7': return k7;
    case '8': return k8;
    case '9': return k9;
    case 'A': return kA;
    case 'B': return kB;
    case 'C': return kC;
    case 'D': return kD;
    case 'E': return kE;
    case 'F': return kF;
    case 'G': return kG;
    case 'H': return kH;
    case 'I': return kI;
    case 'J': return kJ;
    case 'K': return kK;
    case 'L': return kL;
    case 'M': return kM;
    case 'N': return kN;
    case 'O': return kO;
    case 'P': return kP;
    case 'Q': return kQ;
    case 'R': return kR;
    case 'S': return kS;
    case 'T': return kT;
    case 'U': return kU;
    case 'V': return kV;
    case 'W': return kW;
    case 'X': return kX;
    case 'Y': return kY;
    case 'Z': return kZ;
    case '-': return kDash;
    case ':': return kColon;
    case '.': return kDot;
    case '/': return kSlash;
    case '(': return kOpenParen;
    case ')': return kCloseParen;
    case '%': return kPercent;
    case '_': return kUnderscore;
    case '+': return kPlus;
    case ',': return kComma;
    case '[': return kOpenBracket;
    case ']': return kCloseBracket;
    case '?': return kQuestion;
    case '!': return kExclamation;
    case '"': return kDoubleQuote;
    case '#': return kHash;
    case '$': return kDollar;
    case '&': return kAmpersand;
    case '\'': return kApostrophe;
    case '*': return kStar;
    case ';': return kSemicolon;
    case '<': return kLess;
    case '=': return kEqual;
    case '>': return kGreater;
    case '@': return kAt;
    case '\\': return kBackslash;
    case '^': return kCaret;
    case '`': return kBacktick;
    case '{': return kOpenBrace;
    case '|': return kPipe;
    case '}': return kCloseBrace;
    case '~': return kTilde;
    case ' ': return kSpace;
    default: return kBlank;
  }
}

std::string OverlayCanvas::NormalizeText(std::string_view text) {
  std::string output;
  output.reserve(text.size());
  for (unsigned char value : text) {
    if (std::isalpha(value)) {
      output.push_back(static_cast<char>(std::toupper(value)));
    } else if (value == '\n' || value == '\r' || value == '\t') {
      output.push_back(' ');
    } else {
      output.push_back(static_cast<char>(value));
    }
  }
  return output;
}

}  // namespace sekaiemu::spike
