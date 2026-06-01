#include "input_script.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace sekaiemu::spike {

namespace {

std::string Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

std::string Normalize(std::string_view text) {
  std::string output;
  output.reserve(text.size());
  for (const auto ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isalnum(c)) {
      output.push_back(static_cast<char>(std::tolower(c)));
    }
  }
  return output;
}

bool ParseUint(std::string_view text, std::uint64_t& value) {
  const auto trimmed = Trim(text);
  if (trimmed.empty()) {
    return false;
  }
  const auto* begin = trimmed.data();
  const auto* end = trimmed.data() + trimmed.size();
  const auto result = std::from_chars(begin, end, value, 10);
  return result.ec == std::errc{} && result.ptr == end;
}

bool ParseRange(std::string_view token, std::uint64_t& start, std::uint64_t& end) {
  const auto range = token.find("..");
  if (range == std::string_view::npos) {
    if (!ParseUint(token, start)) {
      return false;
    }
    end = start;
    return true;
  }
  if (!ParseUint(token.substr(0, range), start) || !ParseUint(token.substr(range + 2), end)) {
    return false;
  }
  return start <= end;
}

std::optional<InputState::LogicalControl> ParseControl(std::string_view text) {
  const auto key = Normalize(text);
  using Control = InputState::LogicalControl;
  if (key == "up" || key == "dpadup") return Control::Up;
  if (key == "down" || key == "dpaddown") return Control::Down;
  if (key == "left" || key == "dpadleft") return Control::Left;
  if (key == "right" || key == "dpadright") return Control::Right;
  if (key == "a") return Control::A;
  if (key == "b") return Control::B;
  if (key == "x") return Control::X;
  if (key == "y") return Control::Y;
  if (key == "l" || key == "lb" || key == "l1") return Control::L;
  if (key == "r" || key == "rb" || key == "r1") return Control::R;
  if (key == "start" || key == "plus") return Control::Start;
  if (key == "select" || key == "back" || key == "minus") return Control::Select;
  if (key == "analogleft") return Control::AnalogLeft;
  if (key == "analogright") return Control::AnalogRight;
  if (key == "analogup") return Control::AnalogUp;
  if (key == "analogdown") return Control::AnalogDown;
  return std::nullopt;
}

std::vector<std::string> SplitControls(std::string_view text) {
  std::vector<std::string> tokens;
  std::string current;
  for (const auto ch : text) {
    if (ch == ',' || ch == '+' || std::isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

}  // namespace

bool InputScriptPlayer::LoadFromFile(const std::filesystem::path& path, std::string& error) {
  entries_.clear();
  last_frame_ = 0;

  std::ifstream input(path);
  if (!input) {
    error = "input script could not be opened: " + path.string();
    return false;
  }

  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    const auto comment = line.find('#');
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    line = Trim(line);
    if (line.empty()) {
      continue;
    }

    std::istringstream stream(line);
    std::string first;
    stream >> first;
    if (first.empty()) {
      continue;
    }

    Entry entry;
    std::string controls_text;
    if (!ParseRange(first, entry.start_frame, entry.end_frame)) {
      error = "input script line " + std::to_string(line_number) + " has an invalid frame range.";
      return false;
    }

    std::getline(stream, controls_text);
    controls_text = Trim(controls_text);
    if (controls_text.empty()) {
      error = "input script line " + std::to_string(line_number) + " has no controls.";
      return false;
    }

    for (const auto& token : SplitControls(controls_text)) {
      const auto control = ParseControl(token);
      if (!control.has_value()) {
        error = "input script line " + std::to_string(line_number) +
                " has an unknown control: " + token;
        return false;
      }
      entry.controls.push_back(*control);
    }
    if (entry.controls.empty()) {
      error = "input script line " + std::to_string(line_number) + " has no controls.";
      return false;
    }

    last_frame_ = std::max(last_frame_, entry.end_frame);
    entries_.push_back(std::move(entry));
  }

  return true;
}

std::vector<InputState::LogicalControl> InputScriptPlayer::ControlsForFrame(std::uint64_t frame) const {
  std::vector<InputState::LogicalControl> controls;
  for (const auto& entry : entries_) {
    if (frame < entry.start_frame || frame > entry.end_frame) {
      continue;
    }
    controls.insert(controls.end(), entry.controls.begin(), entry.controls.end());
  }
  return controls;
}

}  // namespace sekaiemu::spike
